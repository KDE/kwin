/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwaylandlauncher.h"

#include "config-kwin.h"

#include "xwayland_logging.h"
#include "xwaylandsocket.h"

#include "options.h"
#include "utils/pipe.h"
#include "utils/socketpair.h"
#include "wayland_server.h"

#if KWIN_BUILD_NOTIFICATIONS
#include <KLocalizedString>
#include <KNotification>
#endif

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QSocketNotifier>
#include <QTimer>

// system
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace KWin
{
namespace Xwl
{

XwaylandLauncher::XwaylandLauncher(QObject *parent)
    : QObject(parent)
{
    m_resetCrashCountTimer = new QTimer(this);
    m_resetCrashCountTimer->setSingleShot(true);
    connect(m_resetCrashCountTimer, &QTimer::timeout, this, &XwaylandLauncher::resetCrashCount);
}

XwaylandLauncher::~XwaylandLauncher()
{
}

void XwaylandLauncher::setListenFDs(const QList<int> &listenFds)
{
    m_listenFds = listenFds;
}

void XwaylandLauncher::setDisplayName(const QString &displayName)
{
    m_displayName = displayName;
}

void XwaylandLauncher::setXauthority(const QString &xauthority)
{
    m_xAuthority = xauthority;
}

void XwaylandLauncher::enable()
{
    if (m_enabled) {
        return;
    }

    if (!m_listenFds.isEmpty()) {
        Q_ASSERT(!m_displayName.isEmpty());
    } else {
        auto socket = std::make_unique<XwaylandSocket>(XwaylandSocket::OperationMode::CloseFdsOnExec);
        if (!socket->isValid()) {
            qCWarning(KWIN_XWL) << "Failed to establish X11 socket";
            return;
        }
        m_socket = std::move(socket);
        m_displayName = m_socket->name();
        m_listenFds = m_socket->fileDescriptors();
    }

    for (int socket : std::as_const(m_listenFds)) {
        QSocketNotifier *notifier = new QSocketNotifier(socket, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, &XwaylandLauncher::start);
        connect(this, &XwaylandLauncher::started, notifier, [notifier]() {
            notifier->setEnabled(false);
        });
        connect(this, &XwaylandLauncher::finished, notifier, [this, notifier]() {
            // only reactivate if we've not shut down due to the crash count
            notifier->setEnabled(m_enabled);
        });
    }

    m_enabled = true;
}

void XwaylandLauncher::disable()
{
    m_enabled = false;
    stop();
}

bool XwaylandLauncher::start()
{
    Q_ASSERT(m_enabled);
    if (m_xwaylandProcess) {
        return false;
    }

    auto displayfd = Pipe::create(O_CLOEXEC);
    if (!displayfd.has_value()) {
        qCWarning(KWIN_XWL, "Failed to create pipe to start Xwayland: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    auto wmfd = SocketPair::create(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (!wmfd.has_value()) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    FileDescriptor waylandSocket = waylandServer()->createXWaylandConnection();
    if (!waylandSocket.isValid()) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    QList<int> fdsToPass;
    QStringList arguments;
    arguments << m_displayName;

    if (!m_listenFds.isEmpty()) {
        // xauthority externally set and managed
        if (!m_xAuthority.isEmpty()) {
            arguments << QStringLiteral("-auth") << m_xAuthority;
        }

        for (int socket : std::as_const(m_listenFds)) {
            fdsToPass << socket;
            arguments << QStringLiteral("-listenfd") << QString::number(socket);
        }
    }

    arguments << QStringLiteral("-displayfd") << QString::number(displayfd->fds[1].get());
    fdsToPass << displayfd->fds[1].get();

    arguments << QStringLiteral("-wm") << QString::number(wmfd->fds[1].get());
    fdsToPass << wmfd->fds[1].get();

    arguments << QStringLiteral("-rootless");
#if HAVE_XWAYLAND_ENABLE_EI_PORTAL
    arguments << QStringLiteral("-enable-ei-portal");
#endif

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    env.insert("WAYLAND_SOCKET", QByteArray::number(waylandSocket.get()));
    fdsToPass << waylandSocket.get();

    if (qEnvironmentVariableIntValue("KWIN_XWAYLAND_DEBUG") == 1) {
        env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }

    m_xwaylandProcess = new QProcess(this);
    m_xwaylandProcess->setProgram(QStandardPaths::findExecutable("Xwayland"));
    m_xwaylandProcess->setArguments(arguments);
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setChildProcessModifier([this, fdsToPass]() {
        for (const int &fd : fdsToPass) {
            const int originalFlags = fcntl(fd, F_GETFD);
            if (originalFlags < 0) {
                m_xwaylandProcess->failChildProcessModifier("failed to get file descriptor flags", errno);
                break;
            }
            if (fcntl(fd, F_SETFD, originalFlags & ~FD_CLOEXEC) < 0) {
                m_xwaylandProcess->failChildProcessModifier("failed to unset O_CLOEXEC", errno);
                break;
            }
        }
    });
    connect(m_xwaylandProcess, &QProcess::errorOccurred, this, &XwaylandLauncher::handleXwaylandError);
    connect(m_xwaylandProcess, &QProcess::finished, this, &XwaylandLauncher::handleXwaylandFinished);

    // When Xwayland starts writing the display name to displayfd, it is ready. Alternatively,
    // the Xwayland can send us the SIGUSR1 signal, but it's already reserved for VT hand-off.
    // Xwayland writes the display name followed by "\n". If either write() fails, Xwayland
    // will quit. So we keep the ready file descriptor open instead of closing it when the socket
    // notifier is activated.
    m_xcbConnectionFd = std::move(wmfd->fds[0]);
    m_readyFd = std::move(displayfd->fds[0]);
    m_readyNotifier = std::make_unique<QSocketNotifier>(m_readyFd.get(), QSocketNotifier::Read);
    connect(m_readyNotifier.get(), &QSocketNotifier::activated, this, [this]() {
        m_readyNotifier.reset();
        Q_EMIT ready();
    });

    m_xwaylandProcess->start();

    Q_EMIT started();
    return true;
}

QString XwaylandLauncher::displayName() const
{
    return m_displayName;
}

QString XwaylandLauncher::xauthority() const
{
    return m_xAuthority;
}

FileDescriptor XwaylandLauncher::takeXcbConnectionFd()
{
    return std::move(m_xcbConnectionFd);
}

QProcess *XwaylandLauncher::process() const
{
    return m_xwaylandProcess;
}

void XwaylandLauncher::stop()
{
    if (!m_xwaylandProcess) {
        return;
    }
    Q_EMIT finished();

    waylandServer()->destroyXWaylandConnection();

    m_readyNotifier.reset();
    m_readyFd.reset();
    m_xcbConnectionFd.reset();

    // When the Xwayland process is finally terminated, the finished() signal will be emitted,
    // however we don't actually want to process it anymore. Furthermore, we also don't really
    // want to handle any errors that may occur during the teardown.
    if (m_xwaylandProcess->state() != QProcess::NotRunning) {
        disconnect(m_xwaylandProcess, nullptr, this, nullptr);
        m_xwaylandProcess->terminate();
        m_xwaylandProcess->waitForFinished(5000);
    }
    delete m_xwaylandProcess;
    m_xwaylandProcess = nullptr;
}

void XwaylandLauncher::handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KWIN_XWL) << "Xwayland process has quit with exit status:" << exitStatus << "exit code:" << exitCode;

#if KWIN_BUILD_NOTIFICATIONS
    KNotification::event(QStringLiteral("xwaylandcrash"), i18n("Xwayland has crashed"));
#endif
    m_resetCrashCountTimer->stop();

    switch (options->xwaylandCrashPolicy()) {
    case XwaylandCrashPolicy::Restart:
        if (++m_crashCount <= options->xwaylandMaxCrashCount()) {
            stop();
            m_resetCrashCountTimer->start(std::chrono::minutes(10));
        } else {
            qCWarning(KWIN_XWL, "Stopping Xwayland server because it has crashed %d times "
                                "over the past 10 minutes",
                      m_crashCount);
            disable();
        }
        break;
    case XwaylandCrashPolicy::Stop:
        disable();
        break;
    }
}

void XwaylandLauncher::resetCrashCount()
{
    qCDebug(KWIN_XWL) << "Resetting the crash counter, its current value is" << m_crashCount;
    m_crashCount = 0;
}

void XwaylandLauncher::handleXwaylandError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        qCWarning(KWIN_XWL) << "Xwayland process failed to start";
        return;
    case QProcess::Crashed:
        qCWarning(KWIN_XWL) << "Xwayland process crashed";
        break;
    case QProcess::Timedout:
        qCWarning(KWIN_XWL) << "Xwayland operation timed out";
        break;
    case QProcess::WriteError:
    case QProcess::ReadError:
        qCWarning(KWIN_XWL) << "An error occurred while communicating with Xwayland";
        break;
    case QProcess::UnknownError:
        qCWarning(KWIN_XWL) << "An unknown error has occurred in Xwayland";
        break;
    }
    Q_EMIT errorOccurred();
}

}
}

#include "moc_xwaylandlauncher.cpp"
