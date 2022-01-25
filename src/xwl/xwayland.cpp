/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwayland.h"
#include "cursor.h"
#include "databridge.h"
#include "dnd.h"
#include "xwldrophandler.h"

#include "abstract_output.h"
#include "main_wayland.h"
#include "options.h"
#include "utils/common.h"
#include "platform.h"
#include "utils/xcbutils.h"
#include "wayland_server.h"
#include "xwayland_logging.h"
#include "x11eventfilter.h"

#include "xwaylandsocket.h"

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

class XrandrEventFilter : public X11EventFilter
{
public:
    explicit XrandrEventFilter(Xwayland *backend);

    bool event(xcb_generic_event_t *event) override;

private:
    Xwayland *const m_backend;
};

XrandrEventFilter::XrandrEventFilter(Xwayland *backend)
    : X11EventFilter(Xcb::Extensions::self()->randrNotifyEvent())
    , m_backend(backend)
{
}

bool XrandrEventFilter::event(xcb_generic_event_t *event)
{
    Q_ASSERT((event->response_type & ~0x80) == Xcb::Extensions::self()->randrNotifyEvent());
    m_backend->updatePrimary(kwinApp()->platform()->primaryOutput());
    return false;
}


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

void Xwayland::start()
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

void Xwayland::setListenFDs(const QVector<int> &listenFds)
{
    m_listenFds = listenFds;
}

void Xwayland::setDisplayName(const QString &displayName)
{
    m_displayName = displayName;
}

void Xwayland::setXauthority(const QString &xauthority)
{
    m_xAuthority = xauthority;
}

bool Xwayland::startInternal()
{
    Q_ASSERT(!m_xwaylandProcess);

    QVector<int> fdsToClose;
    auto cleanup = qScopeGuard([&fdsToClose] {
        for (const int fd : qAsConst(fdsToClose)) {
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

        for (int socket : qAsConst(m_listenFds)) {
            int dupSocket = dup(socket);
            fdsToClose << dupSocket;
            #if defined(HAVE_XWAYLAND_LISTENFD)
                arguments << QStringLiteral("-listenfd") << QString::number(dupSocket);
            #else
                arguments << QStringLiteral("-listen") << QString::number(dupSocket);
            #endif
        }
    }

    arguments << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);
    arguments << QStringLiteral("-rootless");
    arguments << QStringLiteral("-wm") << QString::number(fd);

    m_xwaylandProcess = new QProcess(this);
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

    return true;
}

void Xwayland::stop()
{
    if (!m_xwaylandProcess) {
        return;
    }

    stopInternal();
}

void Xwayland::stopInternal()
{
    disconnect(kwinApp()->platform(), &Platform::primaryOutputChanged, this, &Xwayland::updatePrimary);
    Q_ASSERT(m_xwaylandProcess);
    m_app->setClosingX11Connection(true);

    delete m_xrandrEventsFilter;
    m_xrandrEventsFilter = nullptr;

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
    Q_EMIT errorOccurred();
}

void Xwayland::handleXwaylandReady()
{
    // We don't care what Xwayland writes to the displayfd, we just want to know when it's ready.
    maybeDestroyReadyNotifier();

    if (!createX11Connection()) {
        Q_EMIT errorOccurred();
        return;
    }

    qCInfo(KWIN_XWL) << "Xwayland server started on display" << m_displayName;

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    m_selectionOwner.reset(new KSelectionOwner("WM_S0", kwinApp()->x11Connection(), kwinApp()->x11RootWindow()));
    connect(m_selectionOwner.data(), &KSelectionOwner::lostOwnership,
            this, &Xwayland::handleSelectionLostOwnership);
    connect(m_selectionOwner.data(), &KSelectionOwner::claimedOwnership,
            this, &Xwayland::handleSelectionClaimedOwnership);
    connect(m_selectionOwner.data(), &KSelectionOwner::failedToClaimOwnership,
            this, &Xwayland::handleSelectionFailedToClaimOwnership);
    m_selectionOwner->claim(true);

    Cursor *mouseCursor = Cursors::self()->mouse();
    if (mouseCursor) {
        Xcb::defineCursor(kwinApp()->x11RootWindow(), mouseCursor->x11Cursor(Qt::ArrowCursor));
    }

    DataBridge::create(this);

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), m_displayName);
    env.insert(QStringLiteral("XAUTHORITY"), m_xAuthority);
    qputenv("DISPLAY", m_displayName.toUtf8());
    qputenv("XAUTHORITY", m_xAuthority.toUtf8());
    m_app->setProcessStartupEnvironment(env);

    connect(kwinApp()->platform(), &Platform::primaryOutputChanged, this, &Xwayland::updatePrimary);
    updatePrimary(kwinApp()->platform()->primaryOutput());

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort

    delete m_xrandrEventsFilter;
    m_xrandrEventsFilter = new XrandrEventFilter(this);
}

void Xwayland::updatePrimary(AbstractOutput *primaryOutput)
{
    Xcb::RandR::ScreenResources resources(rootWindow());
    xcb_randr_crtc_t *crtcs = resources.crtcs();
    if (!crtcs) {
        return;
    }

    for (int i = 0; i < resources->num_crtcs; ++i) {
        Xcb::RandR::CrtcInfo crtcInfo(crtcs[i], resources->config_timestamp);
        const QRect geometry = crtcInfo.rect();
        if (geometry.topLeft() == primaryOutput->geometry().topLeft()) {
            auto outputs = crtcInfo.outputs();
            if (outputs && crtcInfo->num_outputs > 0) {
                qCDebug(KWIN_XWL) << "Setting primary" << primaryOutput << outputs[0];
                xcb_randr_set_output_primary(kwinApp()->x11Connection(), rootWindow(), outputs[0]);
                break;
            }
        }
    }
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
    Q_EMIT started();
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
    Q_EMIT m_app->x11ConnectionChanged();

    return true;
}

void Xwayland::destroyX11Connection()
{
    if (!m_app->x11Connection()) {
        return;
    }

    Q_EMIT m_app->x11ConnectionAboutToBeDestroyed();

    Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    m_app->destroyAtoms();
    m_app->removeNativeX11EventFilter();

    xcb_disconnect(m_app->x11Connection());
    m_xcbConnectionFd = -1;

    m_app->setX11Connection(nullptr);
    m_app->setX11DefaultScreen(nullptr);
    m_app->setX11ScreenNumber(-1);
    m_app->setX11RootWindow(XCB_WINDOW_NONE);

    Q_EMIT m_app->x11ConnectionChanged();
}

DragEventReply Xwayland::dragMoveFilter(Toplevel *target, const QPoint &pos)
{
    DataBridge *bridge = DataBridge::self();
    if (!bridge) {
        return DragEventReply::Wayland;
    }
    return bridge->dragMoveFilter(target, pos);
}

KWaylandServer::AbstractDropHandler *Xwayland::xwlDropHandler()
{
    DataBridge *bridge = DataBridge::self();
    if (bridge) {
        return bridge->dnd()->dropHandler();
    }
    return nullptr;
}

} // namespace Xwl
} // namespace KWin
