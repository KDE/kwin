/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwaylandlauncher.h"

#include <config-kwin.h>

#include "xwayland_logging.h"
#include "xwaylandsocket.h"

#include "options.h"
#include "wayland_server.h"

#if KWIN_BUILD_NOTIFICATIONS
#include <KLocalizedString>
#include <KNotification>
#endif

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTimer>

// system
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace KWin
{
namespace Xwl
{

XwaylandLauncher::XwaylandLauncher()
{
    m_resetCrashCountTimer = std::make_unique<QTimer>();
    m_resetCrashCountTimer->setSingleShot(true);
    connect(m_resetCrashCountTimer.get(), &QTimer::timeout, this, &XwaylandLauncher::resetCrashCount);
}

XwaylandLauncher::~XwaylandLauncher()
{
}

void XwaylandLauncher::setListenFDs(const QVector<int> &listenFds)
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

void XwaylandLauncher::start()
{
    if (m_xwaylandProcess) {
        return;
    }

    if (!m_listenFds.isEmpty()) {
        Q_ASSERT(!m_displayName.isEmpty());
    } else {
        m_socket.reset(new XwaylandSocket(XwaylandSocket::OperationMode::CloseFdsOnExec));
        if (!m_socket->isValid()) {
            qFatal("Failed to establish X11 socket");
        }
        m_displayName = m_socket->name();
        m_listenFds = m_socket->fileDescriptors();
    }

    startInternal();
}

bool XwaylandLauncher::startInternal()
{
    Q_ASSERT(!m_xwaylandProcess);

    QVector<int> fdsToClose;
    auto cleanup = qScopeGuard([&fdsToClose] {
        for (const int fd : std::as_const(fdsToClose)) {
            close(fd);
        }
    });

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        qCWarning(KWIN_XWL, "Failed to create pipe to start Xwayland: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    fdsToClose << pipeFds[1];

    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    m_xcbConnectionFd = sx[0];

    QStringList arguments;

    arguments << m_displayName;

    if (!m_listenFds.isEmpty()) {
        // xauthority externally set and managed
        if (!m_xAuthority.isEmpty()) {
            arguments << QStringLiteral("-auth") << m_xAuthority;
        }

        for (int socket : std::as_const(m_listenFds)) {
            int dupSocket = dup(socket);
            fdsToClose << dupSocket;
#if HAVE_XWAYLAND_LISTENFD
            arguments << QStringLiteral("-listenfd") << QString::number(dupSocket);
#else
            arguments << QStringLiteral("-listen") << QString::number(dupSocket);
#endif
        }
    }

    arguments << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);
    arguments << QStringLiteral("-rootless");
    arguments << QStringLiteral("-wm") << QString::number(fd);

    m_xwaylandProcess = std::make_unique<QProcess>();
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    if (qEnvironmentVariableIsSet("KWIN_XWAYLAND_DEBUG")) {
        env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments(arguments);
    connect(m_xwaylandProcess.get(), &QProcess::errorOccurred, this, &XwaylandLauncher::handleXwaylandError);
    connect(m_xwaylandProcess.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &XwaylandLauncher::handleXwaylandFinished);

    // When Xwayland starts writing the display name to displayfd, it is ready. Alternatively,
    // the Xwayland can send us the SIGUSR1 signal, but it's already reserved for VT hand-off.
    m_readyNotifier = std::make_unique<QSocketNotifier>(pipeFds[0], QSocketNotifier::Read);
    connect(m_readyNotifier.get(), &QSocketNotifier::activated, this, [this]() {
        maybeDestroyReadyNotifier();
        Q_EMIT started();
    });

    m_xwaylandProcess->start();

    return true;
}

void XwaylandLauncher::stop()
{
    if (!m_xwaylandProcess) {
        return;
    }

    stopInternal();
}

QString XwaylandLauncher::displayName() const
{
    return m_displayName;
}

QString XwaylandLauncher::xauthority() const
{
    return m_xAuthority;
}

int XwaylandLauncher::xcbConnectionFd() const
{
    return m_xcbConnectionFd;
}

QProcess *XwaylandLauncher::process() const
{
    return m_xwaylandProcess.get();
}

void XwaylandLauncher::stopInternal()
{
    Q_EMIT finished();

    maybeDestroyReadyNotifier();
    waylandServer()->destroyXWaylandConnection();

    // When the Xwayland process is finally terminated, the finished() signal will be emitted,
    // however we don't actually want to process it anymore. Furthermore, we also don't really
    // want to handle any errors that may occur during the teardown.
    if (m_xwaylandProcess->state() != QProcess::NotRunning) {
        disconnect(m_xwaylandProcess.get(), nullptr, this, nullptr);
        m_xwaylandProcess->terminate();
        m_xwaylandProcess->waitForFinished(5000);
    }
    m_xwaylandProcess.reset();
}

void XwaylandLauncher::restartInternal()
{
    if (m_xwaylandProcess) {
        stopInternal();
    }
    startInternal();
}

void XwaylandLauncher::maybeDestroyReadyNotifier()
{
    if (m_readyNotifier) {
        close(m_readyNotifier->socket());
        m_readyNotifier.reset();
    }
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
            restartInternal();
            m_resetCrashCountTimer->start(std::chrono::minutes(10));
        } else {
            qCWarning(KWIN_XWL, "Stopping Xwayland server because it has crashed %d times "
                                "over the past 10 minutes",
                      m_crashCount);
            stop();
        }
        break;
    case XwaylandCrashPolicy::Stop:
        stop();
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
