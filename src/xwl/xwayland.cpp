/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwayland.h"
#include "databridge.h"
#include "xwaylandsocket.h"

#include "main_wayland.h"
#include "options.h"
#include "utils.h"
#include "wayland_server.h"
#include "xcbutils.h"
#include "xwayland_logging.h"

#include <KLocalizedString>
#include <KNotification>
#include <KSelectionOwner>

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QTimer>
#include <QtConcurrentRun>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#endif

#include <sys/socket.h>
#include <cerrno>
#include <cstring>

namespace KWin
{
namespace Xwl
{

Xwayland::Xwayland(ApplicationWaylandAbstract *app, QObject *parent)
    : XwaylandInterface(parent)
    , m_app(app)
{
    m_resetCrashCountTimer = new QTimer(this);
    m_resetCrashCountTimer->setSingleShot(true);
    connect(m_resetCrashCountTimer, &QTimer::timeout, this, &Xwayland::resetCrashCount);
}

Xwayland::~Xwayland()
{
    stop();
}

QProcess *Xwayland::process() const
{
    return m_xwaylandProcess;
}

static void writeXauthorityEntry(QDataStream &stream, quint16 family,
                                 const QByteArray &address, const QByteArray &display,
                                 const QByteArray &name, const QByteArray &cookie)
{
    stream << quint16(family);

    auto writeArray = [&stream](const QByteArray &str) {
        stream << quint16(str.size());
        stream.writeRawData(str.constData(), str.size());
    };

    writeArray(address);
    writeArray(display);
    writeArray(name);
    writeArray(cookie);
}

static QByteArray generateXauthorityCookie()
{
    QByteArray cookie;
    cookie.resize(16); // Cookie must be 128bits

    QRandomGenerator *generator = QRandomGenerator::system();
    for (int i = 0; i < cookie.size(); ++i) {
        cookie[i] = uint8_t(generator->bounded(256));
    }
    return cookie;
}

static bool generateXauthorityFile(int display, QTemporaryFile *authorityFile)
{
    const QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);

    authorityFile->setFileTemplate(runtimeDirectory + QStringLiteral("/xauth_XXXXXX"));
    if (!authorityFile->open()) {
        return false;
    }

    const QByteArray hostname = QHostInfo::localHostName().toUtf8();
    const QByteArray displayName = QByteArray::number(display);
    const QByteArray name = QByteArrayLiteral("MIT-MAGIC-COOKIE-1");
    const QByteArray cookie = generateXauthorityCookie();

    QDataStream stream(authorityFile);
    stream.setByteOrder(QDataStream::BigEndian);

    // Write entry with FamilyLocal and the host name as address
    writeXauthorityEntry(stream, 256 /* FamilyLocal */, hostname, displayName, name, cookie);

    // Write entry with FamilyWild, no address
    writeXauthorityEntry(stream, 65535 /* FamilyWild */, QByteArray{}, displayName, name, cookie);

    if (stream.status() != QDataStream::Ok || !authorityFile->flush()) {
        authorityFile->remove();
        return false;
    }

    return true;
}

void Xwayland::start()
{
    if (m_xwaylandProcess) {
        return;
    }

    QScopedPointer<XwaylandSocket> socket(new XwaylandSocket());
    if (!socket->isValid()) {
        qCWarning(KWIN_XWL) << "Failed to create Xwayland connection sockets";
        emit errorOccurred();
        return;
    }

    if (!qEnvironmentVariableIsSet("KWIN_WAYLAND_NO_XAUTHORITY")) {
        if (!generateXauthorityFile(socket->display(), &m_authorityFile)) {
            qCWarning(KWIN_XWL) << "Failed to create an Xauthority file";
            emit errorOccurred();
            return;
        }
    }

    m_socket.reset(socket.take());

    if (!startInternal()) {
        m_authorityFile.remove();
        m_socket.reset();
    }
}

bool Xwayland::startInternal()
{
    Q_ASSERT(!m_xwaylandProcess);

    // The abstract socket file descriptor will be passed to Xwayland and closed by us.
    const int abstractSocket = dup(m_socket->abstractFileDescriptor());
    if (abstractSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to duplicate file descriptor: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }
    auto abstractSocketCleanup = qScopeGuard([&abstractSocket]() {
        close(abstractSocket);
    });

    // The unix socket file descriptor will be passed to Xwayland and closed by us.
    const int unixSocket = dup(m_socket->unixFileDescriptor());
    if (unixSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to duplicate file descriptor: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }
    auto unixSocketCleanup = qScopeGuard([&unixSocket]() {
        close(unixSocket);
    });

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        qCWarning(KWIN_XWL, "Failed to create pipe to start Xwayland: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        emit errorOccurred();
        return false;
    }

    m_xcbConnectionFd = sx[0];

    QStringList arguments {
        m_socket->name(),
        QStringLiteral("-displayfd"), QString::number(pipeFds[1]),
        QStringLiteral("-rootless"),
        QStringLiteral("-wm"), QString::number(fd),
    };

    if (m_authorityFile.isOpen()) {
        arguments << QStringLiteral("-auth") << m_authorityFile.fileName();
    }

#if defined(HAVE_XWAYLAND_LISTENFD)
    arguments << QStringLiteral("-listenfd") << QString::number(abstractSocket)
              << QStringLiteral("-listenfd") << QString::number(unixSocket);
#else
    arguments << QStringLiteral("-listen") << QString::number(abstractSocket)
              << QStringLiteral("-listen") << QString::number(unixSocket);
#endif

    m_xwaylandProcess = new Process(this);
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = m_app->processStartupEnvironment();
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    env.insert("EGL_PLATFORM", QByteArrayLiteral("DRM"));
    if (qEnvironmentVariableIsSet("KWIN_XWAYLAND_DEBUG")) {
        env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments(arguments);
    connect(m_xwaylandProcess, &QProcess::errorOccurred, this, &Xwayland::handleXwaylandError);
    connect(m_xwaylandProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Xwayland::handleXwaylandFinished);

    // When Xwayland starts writing the display name to displayfd, it is ready. Alternatively,
    // the Xwayland can send us the SIGUSR1 signal, but it's already reserved for VT hand-off.
    m_readyNotifier = new QSocketNotifier(pipeFds[0], QSocketNotifier::Read, this);
    connect(m_readyNotifier, &QSocketNotifier::activated, this, &Xwayland::handleXwaylandReady);

    m_xwaylandProcess->start();
    close(pipeFds[1]);

    return true;
}

void Xwayland::stop()
{
    if (!m_xwaylandProcess) {
        return;
    }

    stopInternal();

    m_socket.reset();
    m_authorityFile.remove();
}

void Xwayland::stopInternal()
{
    Q_ASSERT(m_xwaylandProcess);
    m_app->setClosingX11Connection(true);

    // If Xwayland has crashed, we must deactivate the socket notifier and ensure that no X11
    // events will be dispatched before blocking; otherwise we will simply hang...
    uninstallSocketNotifier();
    maybeDestroyReadyNotifier();

    DataBridge::destroy();
    m_selectionOwner.reset();

    destroyX11Connection();

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

    waylandServer()->destroyXWaylandConnection(); // This one must be destroyed last!

    m_app->setClosingX11Connection(false);
}

void Xwayland::restartInternal()
{
    if (m_xwaylandProcess) {
        stopInternal();
    }
    startInternal();
}

void Xwayland::dispatchEvents()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        qCWarning(KWIN_XWL, "Attempting to dispatch X11 events with no connection");
        return;
    }

    const int connectionError = xcb_connection_has_error(connection);
    if (connectionError) {
        qCWarning(KWIN_XWL, "The X11 connection broke (error %d)", connectionError);
        stop();
        return;
    }

    while (xcb_generic_event_t *event = xcb_poll_for_event(connection)) {
        long result = 0;
        QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
        dispatcher->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result);
        free(event);
    }

    xcb_flush(connection);
}

void Xwayland::installSocketNotifier()
{
    const int fileDescriptor = xcb_get_file_descriptor(kwinApp()->x11Connection());

    m_socketNotifier = new QSocketNotifier(fileDescriptor, QSocketNotifier::Read, this);
    connect(m_socketNotifier, &QSocketNotifier::activated, this, &Xwayland::dispatchEvents);

    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Xwayland::dispatchEvents);
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, &Xwayland::dispatchEvents);
}

void Xwayland::uninstallSocketNotifier()
{
    QAbstractEventDispatcher *dispatcher = QCoreApplication::eventDispatcher();
    disconnect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &Xwayland::dispatchEvents);
    disconnect(dispatcher, &QAbstractEventDispatcher::awake, this, &Xwayland::dispatchEvents);

    delete m_socketNotifier;
    m_socketNotifier = nullptr;
}

void Xwayland::handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KWIN_XWL) << "Xwayland process has quit with exit code" << exitCode;

    switch (exitStatus) {
    case QProcess::NormalExit:
        stop();
        break;
    case QProcess::CrashExit:
        handleXwaylandCrashed();
        break;
    }
}

void Xwayland::handleXwaylandCrashed()
{
    KNotification::event(QStringLiteral("xwaylandcrash"), i18n("Xwayland has crashed"));
    m_resetCrashCountTimer->stop();

    switch (options->xwaylandCrashPolicy()) {
    case XwaylandCrashPolicy::Restart:
        if (++m_crashCount <= options->xwaylandMaxCrashCount()) {
            restartInternal();
            m_resetCrashCountTimer->start(std::chrono::minutes(10));
        } else {
            qCWarning(KWIN_XWL, "Stopping Xwayland server because it has crashed %d times "
                      "over the past 10 minutes", m_crashCount);
            stop();
        }
        break;
    case XwaylandCrashPolicy::Stop:
        stop();
        break;
    }
}

void Xwayland::resetCrashCount()
{
    qCDebug(KWIN_XWL) << "Resetting the crash counter, its current value is" << m_crashCount;
    m_crashCount = 0;
}

void Xwayland::handleXwaylandError(QProcess::ProcessError error)
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
    emit errorOccurred();
}

void Xwayland::handleXwaylandReady()
{
    // We don't care what Xwayland writes to the displayfd, we just want to know when it's ready.
    maybeDestroyReadyNotifier();

    if (!createX11Connection()) {
        emit errorOccurred();
        return;
    }

    const QByteArray displayName = ':' + QByteArray::number(m_socket->display());

    qCInfo(KWIN_XWL) << "Xwayland server started on display" << displayName;
    qputenv("DISPLAY", displayName);
    if (m_authorityFile.isOpen()) {
        qputenv("XAUTHORITY", m_authorityFile.fileName().toUtf8());
    }

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    m_selectionOwner.reset(new KSelectionOwner("WM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow()));
    connect(m_selectionOwner.data(), &KSelectionOwner::lostOwnership,
            this, &Xwayland::handleSelectionLostOwnership);
    connect(m_selectionOwner.data(), &KSelectionOwner::claimedOwnership,
            this, &Xwayland::handleSelectionClaimedOwnership);
    connect(m_selectionOwner.data(), &KSelectionOwner::failedToClaimOwnership,
            this, &Xwayland::handleSelectionFailedToClaimOwnership);
    m_selectionOwner->claim(true);

    DataBridge::create(this);

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), displayName);
    if (m_authorityFile.isOpen()) {
        env.insert(QStringLiteral("XAUTHORITY"), m_authorityFile.fileName());
    }
    m_app->setProcessStartupEnvironment(env);

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort
}

void Xwayland::handleSelectionLostOwnership()
{
    qCWarning(KWIN_XWL) << "Somebody else claimed ownership of WM_S0. This should never happen!";
    stop();
}

void Xwayland::handleSelectionFailedToClaimOwnership()
{
    qCWarning(KWIN_XWL) << "Failed to claim ownership of WM_S0. This should never happen!";
    stop();
}

void Xwayland::handleSelectionClaimedOwnership()
{
    emit started();
}

void Xwayland::maybeDestroyReadyNotifier()
{
    if (m_readyNotifier) {
        close(m_readyNotifier->socket());

        delete m_readyNotifier;
        m_readyNotifier = nullptr;
    }
}

bool Xwayland::createX11Connection()
{
    xcb_connection_t *connection = xcb_connect_to_fd(m_xcbConnectionFd, nullptr);

    const int errorCode = xcb_connection_has_error(connection);
    if (errorCode) {
        qCDebug(KWIN_XWL, "Failed to establish the XCB connection (error %d)", errorCode);
        return false;
    }

    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    Q_ASSERT(screen);

    m_app->setX11Connection(connection);
    m_app->setX11DefaultScreen(screen);
    m_app->setX11ScreenNumber(0);
    m_app->setX11RootWindow(screen->root);

    m_app->createAtoms();
    m_app->installNativeX11EventFilter();

    installSocketNotifier();

    // Note that it's very important to have valid x11RootWindow(), x11ScreenNumber(), and
    // atoms when the rest of kwin is notified about the new X11 connection.
    emit m_app->x11ConnectionChanged();

    return true;
}

void Xwayland::destroyX11Connection()
{
    if (!m_app->x11Connection()) {
        return;
    }

    emit m_app->x11ConnectionAboutToBeDestroyed();

    Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    m_app->destroyAtoms();
    m_app->removeNativeX11EventFilter();

    xcb_disconnect(m_app->x11Connection());
    m_xcbConnectionFd = -1;

    m_app->setX11Connection(nullptr);
    m_app->setX11DefaultScreen(nullptr);
    m_app->setX11ScreenNumber(-1);
    m_app->setX11RootWindow(XCB_WINDOW_NONE);

    emit m_app->x11ConnectionChanged();
}

DragEventReply Xwayland::dragMoveFilter(Toplevel *target, const QPoint &pos)
{
    DataBridge *bridge = DataBridge::self();
    if (!bridge) {
        return DragEventReply::Wayland;
    }
    return bridge->dragMoveFilter(target, pos);
}

} // namespace Xwl
} // namespace KWin
