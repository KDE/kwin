/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_server.h"

#include "composite.h"
#include "core/output.h"
#include "core/platform.h"
#include "idle_inhibition.h"
#include "inputpanelv1integration.h"
#include "keyboard_input.h"
#include "layershellv1integration.h"
#include "main.h"
#include "scene.h"
#include "unmanaged.h"
#include "utils/serviceutils.h"
#include "virtualdesktops.h"
#include "wayland/appmenu_interface.h"
#include "wayland/blur_interface.h"
#include "wayland/compositor_interface.h"
#include "wayland/datacontroldevicemanager_v1_interface.h"
#include "wayland/datadevicemanager_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/display.h"
#include "wayland/dpms_interface.h"
#include "wayland/filtered_display.h"
#include "wayland/idle_interface.h"
#include "wayland/idleinhibit_v1_interface.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland/keyboard_shortcuts_inhibit_v1_interface.h"
#include "wayland/keystate_interface.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/lockscreen_overlay_v1_interface.h"
#include "wayland/output_interface.h"
#include "wayland/outputdevice_v2_interface.h"
#include "wayland/outputmanagement_v2_interface.h"
#include "wayland/plasmashell_interface.h"
#include "wayland/plasmavirtualdesktop_interface.h"
#include "wayland/plasmawindowmanagement_interface.h"
#include "wayland/pointerconstraints_v1_interface.h"
#include "wayland/pointergestures_v1_interface.h"
#include "wayland/primaryoutput_v1_interface.h"
#include "wayland/primaryselectiondevicemanager_v1_interface.h"
#include "wayland/relativepointer_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/server_decoration_interface.h"
#include "wayland/server_decoration_palette_interface.h"
#include "wayland/shadow_interface.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/tablet_v2_interface.h"
#include "wayland/viewporter_interface.h"
#include "wayland/xdgactivation_v1_interface.h"
#include "wayland/xdgdecoration_v1_interface.h"
#include "wayland/xdgforeign_v2_interface.h"
#include "wayland/xdgoutput_v1_interface.h"
#include "wayland/xdgshell_interface.h"
#include "wayland/xwaylandkeyboardgrab_v1_interface.h"
#include "waylandoutput.h"
#include "workspace.h"
#include "x11window.h"
#include "xdgactivationv1.h"
#include "xdgshellintegration.h"
#include "xdgshellwindow.h"

// Qt
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QWindow>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

using namespace KWaylandServer;

namespace KWin
{

KWIN_SINGLETON_FACTORY(WaylandServer)

class KWinDisplay : public KWaylandServer::FilteredDisplay
{
public:
    static QByteArray sha256(const QString &fileName)
    {
        QFile f(fileName);
        if (f.open(QFile::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (hash.addData(&f)) {
                return hash.result();
            }
        }
        return QByteArray();
    }

    bool isTrustedOrigin(KWaylandServer::ClientConnection *client) const
    {
        const auto fullPathSha = sha256(client->executablePath());
        const auto localSha = sha256(QLatin1String("/proc/") + QString::number(client->processId()) + QLatin1String("/exe"));
        const bool trusted = !localSha.isEmpty() && fullPathSha == localSha;

        if (!trusted) {
            qCWarning(KWIN_CORE) << "Could not trust" << client->executablePath() << "sha" << localSha << fullPathSha;
        }

        return trusted;
    }

    QStringList fetchRequestedInterfaces(KWaylandServer::ClientConnection *client) const
    {
        return KWin::fetchRequestedInterfaces(client->executablePath());
    }

    const QSet<QByteArray> interfacesBlackList = {
        QByteArrayLiteral("org_kde_kwin_remote_access_manager"),
        QByteArrayLiteral("org_kde_plasma_window_management"),
        QByteArrayLiteral("org_kde_kwin_fake_input"),
        QByteArrayLiteral("org_kde_kwin_keystate"),
        QByteArrayLiteral("zkde_screencast_unstable_v1"),
        QByteArrayLiteral("org_kde_plasma_activation_feedback"),
        QByteArrayLiteral("kde_lockscreen_overlay_v1"),
    };

    const QSet<QByteArray> inputmethodInterfaces = {"zwp_input_panel_v1", "zwp_input_method_v1"};

    QSet<QString> m_reported;

    bool allowInterface(KWaylandServer::ClientConnection *client, const QByteArray &interfaceName) override
    {
        if (client->processId() == getpid()) {
            return true;
        }

        if (client != waylandServer()->inputMethodConnection() && inputmethodInterfaces.contains(interfaceName)) {
            return false;
        }

        if (client != waylandServer()->xWaylandConnection() && interfaceName == "zwp_xwayland_keyboard_grab_manager_v1") {
            return false;
        }

        if (!interfacesBlackList.contains(interfaceName)) {
            return true;
        }

        if (client->executablePath().isEmpty()) {
            qCDebug(KWIN_CORE) << "Could not identify process with pid" << client->processId();
            return false;
        }

        {
            auto requestedInterfaces = client->property("requestedInterfaces");
            if (requestedInterfaces.isNull()) {
                requestedInterfaces = fetchRequestedInterfaces(client);
                client->setProperty("requestedInterfaces", requestedInterfaces);
            }
            if (!requestedInterfaces.toStringList().contains(QString::fromUtf8(interfaceName))) {
                if (KWIN_CORE().isDebugEnabled()) {
                    const QString id = client->executablePath() + QLatin1Char('|') + QString::fromUtf8(interfaceName);
                    if (!m_reported.contains(id)) {
                        m_reported.insert(id);
                        qCDebug(KWIN_CORE) << "Interface" << interfaceName << "not in X-KDE-Wayland-Interfaces of" << client->executablePath();
                    }
                }
                return false;
            }
        }

        {
            auto trustedOrigin = client->property("isPrivileged");
            if (trustedOrigin.isNull()) {
                trustedOrigin = isTrustedOrigin(client);
                client->setProperty("isPrivileged", trustedOrigin);
            }

            if (!trustedOrigin.toBool()) {
                return false;
            }
        }
        qCDebug(KWIN_CORE) << "authorized" << client->executablePath() << interfaceName;
        return true;
    }
};

WaylandServer::WaylandServer(QObject *parent)
    : QObject(parent)
    , m_display(std::make_unique<KWinDisplay>())
{
}

WaylandServer::~WaylandServer()
{
    s_self = nullptr;
    // the display must be destroyed before the rest
    m_display.reset();
}

KWaylandServer::ClientConnection *WaylandServer::xWaylandConnection() const
{
    return m_xwaylandConnection;
}

KWaylandServer::ClientConnection *WaylandServer::inputMethodConnection() const
{
    return m_inputMethodServerConnection;
}

void WaylandServer::registerWindow(Window *window)
{
    if (window->readyForPainting()) {
        Q_EMIT windowAdded(window);
    } else {
        connect(window, &Window::windowShown, this, &WaylandServer::windowShown);
    }
    m_windows << window;
}

void WaylandServer::registerXdgToplevelWindow(XdgToplevelWindow *window)
{
    // TODO: Find a better way and more generic to install extensions.

    SurfaceInterface *surface = window->surface();

    registerWindow(window);

    if (auto shellSurface = PlasmaShellSurfaceInterface::get(surface)) {
        window->installPlasmaShellSurface(shellSurface);
    }
    if (auto decoration = ServerSideDecorationInterface::get(surface)) {
        window->installServerDecoration(decoration);
    }
    if (auto decoration = XdgToplevelDecorationV1Interface::get(window->shellSurface())) {
        window->installXdgDecoration(decoration);
    }
    if (auto menu = m_appMenuManager->appMenuForSurface(surface)) {
        window->installAppMenu(menu);
    }
    if (auto palette = m_paletteManager->paletteForSurface(surface)) {
        window->installPalette(palette);
    }

    connect(m_XdgForeign.get(), &XdgForeignV2Interface::transientChanged, window, [this](SurfaceInterface *child) {
        Q_EMIT foreignTransientChanged(child);
    });
}

void WaylandServer::registerXdgGenericWindow(Window *window)
{
    if (auto toplevel = qobject_cast<XdgToplevelWindow *>(window)) {
        registerXdgToplevelWindow(toplevel);
        return;
    }
    if (auto popup = qobject_cast<XdgPopupWindow *>(window)) {
        registerWindow(popup);
        if (auto shellSurface = PlasmaShellSurfaceInterface::get(popup->surface())) {
            popup->installPlasmaShellSurface(shellSurface);
        }
        return;
    }
    qCDebug(KWIN_CORE) << "Received invalid xdg shell window:" << window->surface();
}

void WaylandServer::handleOutputAdded(Output *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        m_waylandOutputDevices[output] = std::make_unique<KWaylandServer::OutputDeviceV2Interface>(m_display.get(), output);
    }
}

void WaylandServer::handleOutputRemoved(Output *output)
{
    m_waylandOutputDevices.erase(output);
}

void WaylandServer::handleOutputEnabled(Output *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        m_waylandOutputs[output] = std::make_unique<WaylandOutput>(output);
    }
}

void WaylandServer::handleOutputDisabled(Output *output)
{
    m_waylandOutputs.erase(output);
}

bool WaylandServer::start()
{
    return m_display->start();
}

bool WaylandServer::init(const QString &socketName, InitializationFlags flags)
{
    if (!m_display->addSocketName(socketName)) {
        return false;
    }
    return init(flags);
}

bool WaylandServer::init(InitializationFlags flags)
{
    m_initFlags = flags;
    m_compositor = std::make_unique<CompositorInterface>(m_display.get());
    connect(m_compositor.get(), &CompositorInterface::surfaceCreated, this, [this](SurfaceInterface *surface) {
        // check whether we have a Window with the Surface's id
        Workspace *ws = Workspace::self();
        if (!ws) {
            // it's possible that a Surface gets created before Workspace is created
            return;
        }
        if (surface->client() != xWaylandConnection()) {
            // setting surface is only relevant for Xwayland clients
            return;
        }

        X11Window *window = ws->findClient([surface](const X11Window *window) {
            return window->pendingSurfaceId() == surface->id();
        });
        if (window) {
            window->setSurface(surface);
            return;
        }

        Unmanaged *unmanaged = ws->findUnmanaged([surface](const Unmanaged *unmanaged) {
            return unmanaged->pendingSurfaceId() == surface->id();
        });
        if (unmanaged) {
            unmanaged->setSurface(surface);
            return;
        }

        // The surface will be bound later when a WL_SURFACE_ID message is received.
    });

    m_tabletManagerV2 = std::make_unique<TabletManagerV2Interface>(m_display.get());
    m_keyboardShortcutsInhibitManager = std::make_unique<KeyboardShortcutsInhibitManagerV1Interface>(m_display.get());

    m_inputPanelV1Integration = std::make_unique<InputPanelV1Integration>();
    connect(m_inputPanelV1Integration.get(), &InputPanelV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    m_xdgShellIntegration = std::make_unique<XdgShellIntegration>();
    connect(m_xdgShellIntegration.get(), &XdgShellIntegration::windowCreated,
            this, &WaylandServer::registerXdgGenericWindow);

    m_layerShellV1Integration = std::make_unique<LayerShellV1Integration>();
    connect(m_layerShellV1Integration.get(), &LayerShellV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    m_xdgDecorationManagerV1 = std::make_unique<XdgDecorationManagerV1Interface>(m_display.get());
    connect(m_xdgDecorationManagerV1.get(), &XdgDecorationManagerV1Interface::decorationCreated, this, [this](XdgToplevelDecorationV1Interface *decoration) {
        if (XdgToplevelWindow *toplevel = findXdgToplevelWindow(decoration->toplevel()->surface())) {
            toplevel->installXdgDecoration(decoration);
        }
    });

    m_viewPorter = std::make_unique<ViewporterInterface>(m_display.get());
    m_display->createShm();
    m_seat = std::make_unique<SeatInterface>(m_display.get());
    m_pointerGestures = std::make_unique<PointerGesturesV1Interface>(m_display.get());
    m_pointerConstraints = std::make_unique<PointerConstraintsV1Interface>(m_display.get());
    m_relativePointerManager = std::make_unique<RelativePointerManagerV1Interface>(m_display.get());
    m_dataDeviceManager = std::make_unique<DataDeviceManagerInterface>(m_display.get());
    m_dataControlDeviceManager = std::make_unique<DataControlDeviceManagerV1Interface>(m_display.get());

    const auto kwinConfig = kwinApp()->config();
    m_seat->setPrimarySelectionEnabled(kwinConfig->group("Wayland").readEntry("EnablePrimarySelection", true));
    m_primarySelectionDeviceManager = std::make_unique<PrimarySelectionDeviceManagerV1Interface>(m_display.get());

    m_idle = std::make_unique<IdleInterface>(m_display.get());
    m_idleInhibition = std::make_unique<IdleInhibition>();
    connect(this, &WaylandServer::windowAdded, m_idleInhibition.get(), &IdleInhibition::registerClient);
    m_idleInhibtManager = std::make_unique<IdleInhibitManagerV1Interface>(m_display.get());
    m_plasmaShell = std::make_unique<PlasmaShellInterface>(m_display.get());
    connect(m_plasmaShell.get(), &PlasmaShellInterface::surfaceCreated, this, [this](PlasmaShellSurfaceInterface *surface) {
        if (XdgSurfaceWindow *window = findXdgSurfaceWindow(surface->surface())) {
            window->installPlasmaShellSurface(surface);
        }
    });
    m_appMenuManager = std::make_unique<AppMenuManagerInterface>(m_display.get());
    connect(m_appMenuManager.get(), &AppMenuManagerInterface::appMenuCreated, this, [this](AppMenuInterface *appMenu) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(appMenu->surface())) {
            window->installAppMenu(appMenu);
        }
    });
    m_paletteManager = std::make_unique<ServerSideDecorationPaletteManagerInterface>(m_display.get());
    connect(m_paletteManager.get(), &ServerSideDecorationPaletteManagerInterface::paletteCreated, this, [this](ServerSideDecorationPaletteInterface *palette) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(palette->surface())) {
            window->installPalette(palette);
        }
    });

    m_windowManagement = std::make_unique<PlasmaWindowManagementInterface>(m_display.get());
    m_windowManagement->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
    connect(m_windowManagement.get(), &PlasmaWindowManagementInterface::requestChangeShowingDesktop, this, [](PlasmaWindowManagementInterface::ShowingDesktopState state) {
        if (!workspace()) {
            return;
        }
        bool set = false;
        switch (state) {
        case PlasmaWindowManagementInterface::ShowingDesktopState::Disabled:
            set = false;
            break;
        case PlasmaWindowManagementInterface::ShowingDesktopState::Enabled:
            set = true;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        if (set == workspace()->showingDesktop()) {
            return;
        }
        workspace()->setShowingDesktop(set);
    });

    m_virtualDesktopManagement = std::make_unique<PlasmaVirtualDesktopManagementInterface>(m_display.get());
    m_windowManagement->setPlasmaVirtualDesktopManagementInterface(m_virtualDesktopManagement.get());

    m_plasmaActivationFeedback = std::make_unique<PlasmaWindowActivationFeedbackInterface>(m_display.get());

    m_shadowManager = std::make_unique<ShadowManagerInterface>(m_display.get());
    m_dpmsManager = std::make_unique<DpmsManagerInterface>(m_display.get());

    m_decorationManager = std::make_unique<ServerSideDecorationManagerInterface>(m_display.get());
    connect(m_decorationManager.get(), &ServerSideDecorationManagerInterface::decorationCreated, this, [this](ServerSideDecorationInterface *decoration) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(decoration->surface())) {
            window->installServerDecoration(decoration);
        }
    });

    m_outputManagement = std::make_unique<OutputManagementV2Interface>(m_display.get());
    m_primary = std::make_unique<PrimaryOutputV1Interface>(m_display.get());

    m_xdgOutputManagerV1 = std::make_unique<XdgOutputManagerV1Interface>(m_display.get());
    m_subCompositor = std::make_unique<SubCompositorInterface>(m_display.get());
    m_XdgForeign = std::make_unique<XdgForeignV2Interface>(m_display.get());
    m_inputMethod = std::make_unique<InputMethodV1Interface>(m_display.get());
    m_xWaylandKeyboardGrabManager = std::make_unique<XWaylandKeyboardGrabManagerV1Interface>(m_display.get());

    m_activationInterface = std::make_unique<KWaylandServer::XdgActivationV1Interface>(m_display.get());
    auto init = [this] {
        m_xdgActivationIntegration = std::make_unique<XdgActivationV1Integration>(m_activationInterface.get());
    };
    if (Workspace::self()) {
        init();
    } else {
        connect(static_cast<Application *>(qApp), &Application::workspaceCreated, this, init);
    }

    m_lockscreenOverlayInterface = std::make_unique<KWaylandServer::LockscreenOverlayV1Interface>(m_display.get());
    connect(m_lockscreenOverlayInterface.get(), &KWaylandServer::LockscreenOverlayV1Interface::allowRequested, this, [](SurfaceInterface *surface) {
        auto w = waylandServer()->findWindow(surface);
        if (!w) {
            return;
        }
        w->setLockScreenOverlay(true);
    });

    return true;
}

KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *WaylandServer::linuxDmabuf()
{
    if (!m_linuxDmabuf) {
        m_linuxDmabuf = std::make_unique<LinuxDmaBufV1ClientBufferIntegration>(m_display.get());
    }
    return m_linuxDmabuf.get();
}

SurfaceInterface *WaylandServer::findForeignTransientForSurface(SurfaceInterface *surface)
{
    return m_XdgForeign->transientFor(surface);
}

void WaylandServer::windowShown(Window *window)
{
    disconnect(window, &Window::windowShown, this, &WaylandServer::windowShown);
    Q_EMIT windowAdded(window);
}

void WaylandServer::initWorkspace()
{
    m_keyStateInterface = std::make_unique<KeyStateInterface>(m_display.get());

    VirtualDesktopManager::self()->setVirtualDesktopManagement(m_virtualDesktopManagement.get());

    if (m_windowManagement) {
        connect(workspace(), &Workspace::showingDesktopChanged, this, [this](bool set) {
            using namespace KWaylandServer;
            m_windowManagement->setShowingDesktopState(set ? PlasmaWindowManagementInterface::ShowingDesktopState::Enabled : PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
        });

        connect(workspace(), &Workspace::workspaceInitialized, this, [this] {
            auto f = [this]() {
                QVector<quint32> ids;
                QVector<QString> uuids;
                for (Window *toplevel : workspace()->stackingOrder()) {
                    if (toplevel->windowManagementInterface()) {
                        ids << toplevel->windowManagementInterface()->internalId();
                        uuids << toplevel->windowManagementInterface()->uuid();
                    }
                }
                m_windowManagement->setStackingOrder(ids);
                m_windowManagement->setStackingOrderUuids(uuids);
            };
            f();
            connect(workspace(), &Workspace::stackingOrderChanged, this, f);
        });
    }

    if (auto primaryOutput = workspace()->primaryOutput()) {
        m_primary->setPrimaryOutput(primaryOutput->name());
    }
    connect(workspace(), &Workspace::primaryOutputChanged, this, [this]() {
        const Output *primaryOutput = workspace()->primaryOutput();
        m_primary->setPrimaryOutput(primaryOutput ? primaryOutput->name() : QString());
    });

    const auto availableOutputs = kwinApp()->platform()->outputs();
    for (Output *output : availableOutputs) {
        handleOutputAdded(output);
    }
    connect(kwinApp()->platform(), &Platform::outputAdded, this, &WaylandServer::handleOutputAdded);
    connect(kwinApp()->platform(), &Platform::outputRemoved, this, &WaylandServer::handleOutputRemoved);

    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputEnabled(output);
    }
    connect(workspace(), &Workspace::outputAdded, this, &WaylandServer::handleOutputEnabled);
    connect(workspace(), &Workspace::outputRemoved, this, &WaylandServer::handleOutputDisabled);

    if (hasScreenLockerIntegration()) {
        initScreenLocker();
    }
    Q_EMIT initialized();
}

void WaylandServer::initScreenLocker()
{
#if KWIN_BUILD_SCREENLOCKER
    auto *screenLockerApp = ScreenLocker::KSldApp::self();

    ScreenLocker::KSldApp::self()->setGreeterEnvironment(kwinApp()->processStartupEnvironment());

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToLock, this, [this, screenLockerApp]() {
        if (m_screenLockerClientConnection) {
            // Already sent data to KScreenLocker.
            return;
        }
        int clientFd = createScreenLockerConnection();
        if (clientFd < 0) {
            return;
        }
        ScreenLocker::KSldApp::self()->setWaylandFd(clientFd);

        m_lockscreenWatcher = std::make_unique<LockScreenPresentationWatcher>(this);

        const QVector<SeatInterface *> seatIfaces = m_display->seats();
        for (auto *seat : seatIfaces) {
            connect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                    screenLockerApp, &ScreenLocker::KSldApp::userActivity);
        }
    });

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::unlocked, this, [this, screenLockerApp]() {
        if (m_screenLockerClientConnection) {
            m_screenLockerClientConnection->destroy();
            m_screenLockerClientConnection = nullptr;
        }

        const QVector<SeatInterface *> seatIfaces = m_display->seats();
        for (auto *seat : seatIfaces) {
            disconnect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                       screenLockerApp, &ScreenLocker::KSldApp::userActivity);
        }
        ScreenLocker::KSldApp::self()->setWaylandFd(-1);
    });

    connect(screenLockerApp, &ScreenLocker::KSldApp::lockStateChanged, this, &WaylandServer::lockStateChanged);

    ScreenLocker::KSldApp::self()->initialize();

    if (m_initFlags.testFlag(InitializationFlag::LockScreen)) {
        ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    }
#endif
}

WaylandServer::SocketPairConnection WaylandServer::createConnection()
{
    SocketPairConnection ret;
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return ret;
    }
    ret.connection = m_display->createClient(sx[0]);
    ret.fd = sx[1];
    return ret;
}

int WaylandServer::createScreenLockerConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_screenLockerClientConnection = socket.connection;
    connect(m_screenLockerClientConnection, &KWaylandServer::ClientConnection::disconnected, this, [this]() {
        m_screenLockerClientConnection = nullptr;
    });
    return socket.fd;
}

int WaylandServer::createXWaylandConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_xwaylandConnection = socket.connection;

    m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    connect(kwinApp(), &Application::xwaylandScaleChanged, m_xwaylandConnection, [this]() {
        m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    });

    return socket.fd;
}

void WaylandServer::destroyXWaylandConnection()
{
    if (!m_xwaylandConnection) {
        return;
    }
    m_xwaylandConnection->destroy();
    m_xwaylandConnection = nullptr;
}

int WaylandServer::createInputMethodConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_inputMethodServerConnection = socket.connection;
    return socket.fd;
}

void WaylandServer::destroyInputMethodConnection()
{
    if (!m_inputMethodServerConnection) {
        return;
    }
    m_inputMethodServerConnection->destroy();
    m_inputMethodServerConnection = nullptr;
}

void WaylandServer::removeWindow(Window *c)
{
    m_windows.removeAll(c);
    if (c->readyForPainting()) {
        Q_EMIT windowRemoved(c);
    }
}

static Window *findWindowInList(const QList<Window *> &windows, const KWaylandServer::SurfaceInterface *surface)
{
    auto it = std::find_if(windows.begin(), windows.end(), [surface](Window *w) {
        return w->surface() == surface;
    });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}

Window *WaylandServer::findWindow(const KWaylandServer::SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    if (Window *c = findWindowInList(m_windows, surface)) {
        return c;
    }
    return nullptr;
}

XdgToplevelWindow *WaylandServer::findXdgToplevelWindow(SurfaceInterface *surface) const
{
    return qobject_cast<XdgToplevelWindow *>(findWindow(surface));
}

XdgSurfaceWindow *WaylandServer::findXdgSurfaceWindow(SurfaceInterface *surface) const
{
    return qobject_cast<XdgSurfaceWindow *>(findWindow(surface));
}

bool WaylandServer::isScreenLocked() const
{
#if KWIN_BUILD_SCREENLOCKER
    if (!hasScreenLockerIntegration()) {
        return false;
    }
    return ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked || ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::AcquiringLock;
#else
    return false;
#endif
}

bool WaylandServer::hasScreenLockerIntegration() const
{
#if KWIN_BUILD_SCREENLOCKER
    return !m_initFlags.testFlag(InitializationFlag::NoLockScreenIntegration);
#else
    return false;
#endif
}

bool WaylandServer::hasGlobalShortcutSupport() const
{
    return !m_initFlags.testFlag(InitializationFlag::NoGlobalShortcuts);
}

bool WaylandServer::isKeyboardShortcutsInhibited() const
{
    auto surface = seat()->focusedKeyboardSurface();
    if (surface) {
        auto inhibitor = keyboardShortcutsInhibitManager()->findInhibitor(surface, seat());
        if (inhibitor && inhibitor->isActive()) {
            return true;
        }
        if (m_xWaylandKeyboardGrabManager->hasGrab(surface, seat())) {
            return true;
        }
    }
    return false;
}

QString WaylandServer::socketName() const
{
    const QStringList socketNames = display()->socketNames();
    if (!socketNames.isEmpty()) {
        return socketNames.first();
    }
    return QString();
}

#if KWIN_BUILD_SCREENLOCKER
WaylandServer::LockScreenPresentationWatcher::LockScreenPresentationWatcher(WaylandServer *server)
{
    connect(server, &WaylandServer::windowAdded, this, [this, server](Window *window) {
        if (window->isLockScreen()) {
            // only signal lockScreenShown once all outputs have been presented at least once
            connect(window->output()->renderLoop(), &RenderLoop::framePresented, this, [this, server, windowGuard = QPointer(window)]() {
                // window might be destroyed before a frame is presented, so it's wrapped in QPointer
                if (windowGuard) {
                    m_signaledOutputs << windowGuard->output();
                    if (m_signaledOutputs.size() == workspace()->outputs().size()) {
                        ScreenLocker::KSldApp::self()->lockScreenShown();
                        server->m_lockscreenWatcher.reset();
                    }
                }
            });
        }
    });
    QTimer::singleShot(1000, this, [server]() {
        ScreenLocker::KSldApp::self()->lockScreenShown();
        server->m_lockscreenWatcher.reset();
    });
}
#endif
}
