/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_server.h"

#include "config-kwin.h"

#include "backends/drm/drm_backend.h"
#include "core/drmdevice.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/session.h"
#include "idle_inhibition.h"
#include "inputpanelv1integration.h"
#include "layershellv1integration.h"
#include "layershellv1window.h"
#include "main.h"
#include "options.h"
#include "utils/envvar.h"
#include "utils/kernel.h"
#include "utils/serviceutils.h"
#include "virtualdesktops.h"
#include "wayland/alphamodifier_v1.h"
#include "wayland/appmenu.h"
#include "wayland/clientconnection.h"
#include "wayland/colormanagement_v1.h"
#include "wayland/colorrepresentation_v1.h"
#include "wayland/compositor.h"
#include "wayland/contenttype_v1.h"
#include "wayland/cursorshape_v1.h"
#include "wayland/datacontroldevicemanager_v1.h"
#include "wayland/datadevicemanager.h"
#include "wayland/display.h"
#include "wayland/dpms.h"
#include "wayland/drmclientbuffer.h"
#include "wayland/drmlease_v1.h"
#include "wayland/externalbrightness_v1.h"
#include "wayland/fifo_v1.h"
#include "wayland/filtered_display.h"
#include "wayland/fixes.h"
#include "wayland/fractionalscale_v1.h"
#include "wayland/frog_colormanagement_v1.h"
#include "wayland/idle.h"
#include "wayland/idleinhibit_v1.h"
#include "wayland/idlenotify_v1.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/keyboard_shortcuts_inhibit_v1.h"
#include "wayland/keystate.h"
#include "wayland/linux_drm_syncobj_v1.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/lockscreen_overlay_v1.h"
#include "wayland/output.h"
#include "wayland/output_order_v1.h"
#include "wayland/outputdevice_v2.h"
#include "wayland/outputmanagement_v2.h"
#include "wayland/plasmashell.h"
#include "wayland/plasmavirtualdesktop.h"
#include "wayland/plasmawindowmanagement.h"
#include "wayland/pointerconstraints_v1.h"
#include "wayland/pointergestures_v1.h"
#include "wayland/pointerwarp_v1.h"
#include "wayland/presentationtime.h"
#include "wayland/primaryselectiondevicemanager_v1.h"
#include "wayland/relativepointer_v1.h"
#include "wayland/screenedge_v1.h"
#include "wayland/seat.h"
#include "wayland/securitycontext_v1.h"
#include "wayland/server_decoration.h"
#include "wayland/server_decoration_palette.h"
#include "wayland/shadow.h"
#include "wayland/singlepixelbuffer.h"
#include "wayland/subcompositor.h"
#include "wayland/tablet_v2.h"
#include "wayland/tearingcontrol_v1.h"
#include "wayland/viewporter.h"
#include "wayland/xdgactivation_v1.h"
#include "wayland/xdgdecoration_v1.h"
#include "wayland/xdgdialog_v1.h"
#include "wayland/xdgforeign_v2.h"
#include "wayland/xdgoutput_v1.h"
#include "wayland/xdgsession_v1.h"
#include "wayland/xdgshell.h"
#include "wayland/xdgtopleveldrag_v1.h"
#include "wayland/xdgtoplevelicon_v1.h"
#include "wayland/xdgtopleveltag_v1.h"
#include "workspace.h"
#include "xdgactivationv1.h"
#include "xdgshellintegration.h"
#include "xdgshellwindow.h"
#include "xxpipv1integration.h"
#if KWIN_BUILD_X11
#include "wayland/xwaylandkeyboardgrab_v1.h"
#include "wayland/xwaylandshell_v1.h"
#include "x11window.h"
#endif

// Qt
#include <QDir>
#include <QFileInfo>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

namespace KWin
{

KWIN_SINGLETON_FACTORY(WaylandServer)

class KWinDisplay : public FilteredDisplay
{
public:
    KWinDisplay(QObject *parent)
        : FilteredDisplay(parent)
    {
    }

    QStringList fetchRequestedInterfaces(ClientConnection *client) const
    {
        if (!client->securityContextAppId().isEmpty()) {
            return KWin::fetchRequestedInterfacesForDesktopId(client->securityContextAppId());
        }
        return KWin::fetchRequestedInterfaces(client->executablePath());
    }

    const QSet<QByteArray> interfacesBlackList = {
        QByteArrayLiteral("org_kde_plasma_window_management"),
        QByteArrayLiteral("org_kde_kwin_fake_input"),
        QByteArrayLiteral("org_kde_kwin_keystate"),
        QByteArrayLiteral("zkde_screencast_unstable_v1"),
        QByteArrayLiteral("org_kde_plasma_activation_feedback"),
        QByteArrayLiteral("kde_lockscreen_overlay_v1"),
    };

    const QSet<QByteArray> inputmethodInterfaces = {"zwp_input_panel_v1", "zwp_input_method_v1"};
    const QSet<QByteArray> xwaylandInterfaces = {
        QByteArrayLiteral("zwp_xwayland_keyboard_grab_manager_v1"),
        QByteArrayLiteral("xwayland_shell_v1"),
    };

    QSet<QString> m_reported;

    bool allowInterface(ClientConnection *client, const QByteArray &interfaceName) override
    {
        if (!client->securityContextAppId().isEmpty() && interfaceName == QByteArrayLiteral("wp_security_context_manager_v1")) {
            return false;
        }

        if (client->processId() == getpid()) {
            return true;
        }

        if (client != waylandServer()->inputMethodConnection() && inputmethodInterfaces.contains(interfaceName)) {
            return false;
        }

        if (client != waylandServer()->xWaylandConnection() && xwaylandInterfaces.contains(interfaceName)) {
            return false;
        }

        if (!interfacesBlackList.contains(interfaceName)) {
            return true;
        }

        static bool permissionCheckDisabled = qEnvironmentVariableIntValue("KWIN_WAYLAND_NO_PERMISSION_CHECKS") == 1;
        if (!permissionCheckDisabled) {
            if (client->executablePath().isEmpty()) {
                qCDebug(KWIN_CORE) << "Could not identify process with pid" << client->processId();
                return false;
            }
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

        qCDebug(KWIN_CORE) << "authorized" << client->executablePath() << interfaceName;
        return true;
    }
};

static size_t mibToBytes(size_t mib)
{
    return mib * (size_t(1) << 20);
}

static size_t defaultMaxBufferSize()
{
    if (int hint = qEnvironmentVariableIntValue("KWIN_WAYLAND_DEFAULT_MAX_CONNECTION_BUFFER_SIZE"); hint > 0) {
        return hint;
    }
    return mibToBytes(1);
}

WaylandServer::WaylandServer(QObject *parent)
    : QObject(parent)
    , m_display(new KWinDisplay(this))
{
    m_display->setDefaultMaxBufferSize(defaultMaxBufferSize());
}

WaylandServer::~WaylandServer()
{
    s_self = nullptr;
}

ClientConnection *WaylandServer::xWaylandConnection() const
{
    return m_xwaylandConnection;
}

ClientConnection *WaylandServer::inputMethodConnection() const
{
    return m_inputMethodServerConnection;
}

ClientConnection *WaylandServer::screenLockerClientConnection() const
{
    return m_screenLockerClientConnection;
}

void WaylandServer::registerWindow(Window *window)
{
    if (window->readyForPainting()) {
        Q_EMIT windowAdded(window);
    } else {
        connect(window, &Window::readyForPaintingChanged, this, [this, window]() {
            Q_EMIT windowAdded(window);
        });
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
    if (auto dialog = m_xdgDialogWm->dialogForToplevel(window->shellSurface())) {
        window->installXdgDialogV1(dialog);
    }

    connect(m_XdgForeign, &XdgForeignV2Interface::transientChanged, window, [this](SurfaceInterface *child) {
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

void WaylandServer::handleOutputAdded(LogicalOutput *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        m_waylandOutputDevices.insert(output, new OutputDeviceV2Interface(m_display, output));
    }
}

void WaylandServer::handleOutputRemoved(LogicalOutput *output)
{
    if (auto outputDevice = m_waylandOutputDevices.take(output)) {
        outputDevice->remove();
    }
}

void WaylandServer::handleOutputEnabled(LogicalOutput *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        auto waylandOutput = new OutputInterface(waylandServer()->display(), output);
        m_xdgOutputManagerV1->offer(waylandOutput);

        m_waylandOutputs.insert(output, waylandOutput);
    }
}

void WaylandServer::handleOutputDisabled(LogicalOutput *output)
{
    if (auto waylandOutput = m_waylandOutputs.take(output)) {
        waylandOutput->remove();
    }
}

bool WaylandServer::start()
{
    QProcessEnvironment environment = kwinApp()->processStartupEnvironment();
    if (!socketName().isEmpty()) {
        environment.insert(QStringLiteral("WAYLAND_DISPLAY"), socketName());
        qputenv("WAYLAND_DISPLAY", socketName().toUtf8());
    }
    kwinApp()->setProcessStartupEnvironment(environment);

    return m_display->start();
}

bool WaylandServer::init(const QString &socketName)
{
    if (!m_display->addSocketName(socketName)) {
        return false;
    }
    return init();
}

bool WaylandServer::init()
{
    m_compositor = new CompositorInterface(m_display, m_display);

#if KWIN_BUILD_X11
    m_xwaylandShell = new XwaylandShellV1Interface(m_display, m_display);
    connect(m_xwaylandShell, &XwaylandShellV1Interface::surfaceAssociated, this, [](XwaylandSurfaceV1Interface *surface) {
        X11Window *window = workspace()->findClient([&surface](const X11Window *window) {
            return window->surfaceSerial() == surface->serial();
        });
        if (window) {
            window->associate(surface);
            return;
        }

        X11Window *unmanaged = workspace()->findUnmanaged([&surface](const X11Window *window) {
            return window->surfaceSerial() == surface->serial();
        });
        if (unmanaged) {
            unmanaged->associate(surface);
            return;
        }
    });
#endif

    m_tabletManagerV2 = new TabletManagerV2Interface(m_display, m_display);
    m_keyboardShortcutsInhibitManager = new KeyboardShortcutsInhibitManagerV1Interface(m_display, m_display);

    if (qEnvironmentVariableIntValue("KWIN_WAYLAND_SUPPORT_XX_SESSION_MANAGER") == 1) {
        auto storage = new XdgSessionStorageV1(KSharedConfig::openStateConfig(QStringLiteral("kwinsessionrc")), this);
        new XdgSessionManagerV1Interface(m_display, storage, m_display);
    }

    m_xdgDecorationManagerV1 = new XdgDecorationManagerV1Interface(m_display, m_display);
    connect(m_xdgDecorationManagerV1, &XdgDecorationManagerV1Interface::decorationCreated, this, [this](XdgToplevelDecorationV1Interface *decoration) {
        if (XdgToplevelWindow *toplevel = findXdgToplevelWindow(decoration->toplevel()->surface())) {
            toplevel->installXdgDecoration(decoration);
        }
    });

    new ViewporterInterface(m_display, m_display);
    new SecurityContextManagerV1Interface(m_display, m_display);
    new FractionalScaleManagerV1Interface(m_display, m_display);
    m_display->createShm();
    m_seat = new SeatInterface(m_display, kwinApp()->session()->seat(), m_display);
    new PointerGesturesV1Interface(m_display, m_display);
    new PointerConstraintsV1Interface(m_display, m_display);
    new RelativePointerManagerV1Interface(m_display, m_display);
    m_dataDeviceManager = new DataDeviceManagerInterface(m_display, m_display);
    new DataControlDeviceManagerV1Interface(m_display, m_display);
    new CursorShapeManagerV1Interface(m_display, m_display);

    const auto kwinConfig = kwinApp()->config();
    if (kwinConfig->group(QStringLiteral("Wayland")).readEntry("EnablePrimarySelection", true)) {
        new PrimarySelectionDeviceManagerV1Interface(m_display, m_display);
    }

    m_idle = new IdleInterface(m_display, m_display);
    new IdleInhibition(m_idle);
    new IdleInhibitManagerV1Interface(m_display, m_display);
    new IdleNotifyV1Interface(m_display, m_display);
    m_plasmaShell = new PlasmaShellInterface(m_display, m_display);
    connect(m_plasmaShell, &PlasmaShellInterface::surfaceCreated, this, [this](PlasmaShellSurfaceInterface *surface) {
        if (XdgSurfaceWindow *window = findXdgSurfaceWindow(surface->surface())) {
            window->installPlasmaShellSurface(surface);
        }
    });
    m_appMenuManager = new AppMenuManagerInterface(m_display, m_display);
    connect(m_appMenuManager, &AppMenuManagerInterface::appMenuCreated, this, [this](AppMenuInterface *appMenu) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(appMenu->surface())) {
            window->installAppMenu(appMenu);
        }
    });
    m_paletteManager = new ServerSideDecorationPaletteManagerInterface(m_display, m_display);
    connect(m_paletteManager, &ServerSideDecorationPaletteManagerInterface::paletteCreated, this, [this](ServerSideDecorationPaletteInterface *palette) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(palette->surface())) {
            window->installPalette(palette);
        }
    });

    m_windowManagement = new PlasmaWindowManagementInterface(m_display, m_display);
    m_windowManagement->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
    connect(m_windowManagement, &PlasmaWindowManagementInterface::requestChangeShowingDesktop, this, [](PlasmaWindowManagementInterface::ShowingDesktopState state) {
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

    m_virtualDesktopManagement = new PlasmaVirtualDesktopManagementInterface(m_display, m_display);
    m_windowManagement->setPlasmaVirtualDesktopManagementInterface(m_virtualDesktopManagement);

    m_plasmaActivationFeedback = new PlasmaWindowActivationFeedbackInterface(m_display, m_display);

    new ShadowManagerInterface(m_display, m_display);
    new DpmsManagerInterface(m_display, m_display);

    m_decorationManager = new ServerSideDecorationManagerInterface(m_display, m_display);
    connect(m_decorationManager, &ServerSideDecorationManagerInterface::decorationCreated, this, [this](ServerSideDecorationInterface *decoration) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(decoration->surface())) {
            window->installServerDecoration(decoration);
        }
    });

    m_outputManagement = new OutputManagementV2Interface(m_display, m_display);

    m_xdgOutputManagerV1 = new XdgOutputManagerV1Interface(m_display, m_display);
    new SubCompositorInterface(m_display, m_display);
    m_XdgForeign = new XdgForeignV2Interface(m_display, m_display);
    m_inputMethod = new InputMethodV1Interface(m_display, m_display);
#if KWIN_BUILD_X11
    m_xWaylandKeyboardGrabManager = new XWaylandKeyboardGrabManagerV1Interface(m_display, m_display);
#endif

    auto activation = new XdgActivationV1Interface(m_display, this);
    auto init = [this, activation] {
        m_xdgActivationIntegration = new XdgActivationV1Integration(activation, this);
    };
    if (Workspace::self()) {
        init();
    } else {
        connect(static_cast<Application *>(qApp), &Application::workspaceCreated, this, init);
    }

    auto aboveLockscreen = new LockscreenOverlayV1Interface(m_display, this);
    connect(aboveLockscreen, &LockscreenOverlayV1Interface::allowRequested, this, [](SurfaceInterface *surface) {
        auto w = waylandServer()->findWindow(surface);
        if (!w) {
            return;
        }
        w->setLockScreenOverlay(true);
    });

    m_contentTypeManager = new ContentTypeManagerV1Interface(m_display, m_display);
    m_tearingControlInterface = new TearingControlManagerV1Interface(m_display, m_display);
    new XdgToplevelDragManagerV1Interface(m_display, this);

    new XdgToplevelIconManagerV1Interface(m_display, this);

    auto screenEdgeManager = new ScreenEdgeManagerV1Interface(m_display, m_display);
    connect(screenEdgeManager, &ScreenEdgeManagerV1Interface::edgeRequested, this, [this](AutoHideScreenEdgeV1Interface *edge) {
        if (auto window = qobject_cast<LayerShellV1Window *>(findWindow(edge->surface()))) {
            window->installAutoHideScreenEdgeV1(edge);
        }
    });

    new FrogColorManagementV1(m_display, m_display);
    new PresentationTime(m_display, m_display);
    m_colorManager = new ColorManagerV1(m_display, m_display);
    m_xdgDialogWm = new KWin::XdgDialogWmV1Interface(m_display, m_display);
    connect(m_xdgDialogWm, &KWin::XdgDialogWmV1Interface::dialogCreated, this, [this](KWin::XdgDialogV1Interface *dialog) {
        if (auto window = findXdgToplevelWindow(dialog->toplevel()->surface())) {
            window->installXdgDialogV1(dialog);
        }
    });

    m_externalBrightness = new ExternalBrightnessV1(m_display, m_display);
    m_alphaModifierManager = new AlphaModifierManagerV1(m_display, m_display);
    new FixesInterface(m_display, m_display);
    m_fifoManager = new FifoManagerV1(m_display, m_display);
    m_singlePixelBuffer = new SinglePixelBufferManagerV1(m_display, m_display);
    m_toplevelTag = new XdgToplevelTagManagerV1(m_display, m_display);
    m_colorRepresentation = new ColorRepresentationManagerV1(m_display, m_display);
    m_pointerWarp = new PointerWarpV1(m_display, m_display);
    return true;
}

static const bool s_reenableWlDrm = environmentVariableBoolValue("KWIN_WAYLAND_REENABLE_WL_DRM").value_or(false);

DrmClientBufferIntegration *WaylandServer::drm()
{
    if (!m_drm && s_reenableWlDrm) {
        m_drm = new DrmClientBufferIntegration(m_display);
    }
    return m_drm;
}

LinuxDmaBufV1ClientBufferIntegration *WaylandServer::linuxDmabuf()
{
    if (!m_linuxDmabuf) {
        m_linuxDmabuf = new LinuxDmaBufV1ClientBufferIntegration(m_display);
    }
    return m_linuxDmabuf;
}

SurfaceInterface *WaylandServer::findForeignTransientForSurface(SurfaceInterface *surface)
{
    return m_XdgForeign->transientFor(surface);
}

XdgExportedSurface *WaylandServer::exportAsForeign(SurfaceInterface *surface)
{
    return m_XdgForeign->exportSurface(surface);
}

void WaylandServer::initWorkspace()
{
    auto inputPanelV1Integration = new InputPanelV1Integration(this);
    connect(inputPanelV1Integration, &InputPanelV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    auto xdgShellIntegration = new XdgShellIntegration(this);
    connect(xdgShellIntegration, &XdgShellIntegration::windowCreated,
            this, &WaylandServer::registerXdgGenericWindow);

    auto setPingTimeout = [xdgShellIntegration] {
        xdgShellIntegration->setPingTimeout(std::chrono::milliseconds(options->killPingTimeout()));
    };
    connect(options, &Options::killPingTimeoutChanged, xdgShellIntegration, setPingTimeout);
    setPingTimeout();

    auto layerShellV1Integration = new LayerShellV1Integration(this);
    connect(layerShellV1Integration, &LayerShellV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    if (qEnvironmentVariableIntValue("KWIN_WAYLAND_SUPPORT_XX_PIP_V1") == 1) {
        auto pipV1Integration = new XXPipV1Integration(this);
        connect(pipV1Integration, &XXPipV1Integration::windowCreated,
                this, &WaylandServer::registerWindow);
    }

    new KeyStateInterface(m_display, m_display);

    VirtualDesktopManager::self()->setVirtualDesktopManagement(m_virtualDesktopManagement);

    if (m_windowManagement) {
        connect(workspace(), &Workspace::showingDesktopChanged, this, [this](bool set) {
            m_windowManagement->setShowingDesktopState(set ? PlasmaWindowManagementInterface::ShowingDesktopState::Enabled : PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
        });

        connect(workspace(), &Workspace::workspaceInitialized, this, [this] {
            auto f = [this]() {
                QList<quint32> ids;
                QList<QString> uuids;
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

    const auto availableOutputs = kwinApp()->outputBackend()->outputs();
    for (LogicalOutput *output : availableOutputs) {
        handleOutputAdded(output);
    }
    connect(kwinApp()->outputBackend(), &OutputBackend::outputAdded, this, &WaylandServer::handleOutputAdded);
    connect(kwinApp()->outputBackend(), &OutputBackend::outputRemoved, this, &WaylandServer::handleOutputRemoved);

    const auto outputs = workspace()->outputs();
    for (LogicalOutput *output : outputs) {
        handleOutputEnabled(output);
    }
    connect(workspace(), &Workspace::outputAdded, this, &WaylandServer::handleOutputEnabled);
    connect(workspace(), &Workspace::outputRemoved, this, &WaylandServer::handleOutputDisabled);

    if (kwinApp()->supportsLockScreen()) {
        initScreenLocker();
    }

    if (auto backend = qobject_cast<DrmBackend *>(kwinApp()->outputBackend())) {
        m_leaseManager = new DrmLeaseManagerV1(backend, m_display, m_display);
    }

    m_outputOrder = new OutputOrderV1Interface(m_display, m_display);
    m_outputOrder->setOutputOrder(workspace()->outputOrder());
    connect(workspace(), &Workspace::outputOrderChanged, m_outputOrder, [this]() {
        m_outputOrder->setOutputOrder(workspace()->outputOrder());
    });

    Q_EMIT initialized();
}

void WaylandServer::initScreenLocker()
{
#if KWIN_BUILD_SCREENLOCKER
    auto *screenLockerApp = ScreenLocker::KSldApp::self();

    ScreenLocker::KSldApp::self()->setGreeterEnvironment(kwinApp()->processStartupEnvironment());

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToLock, this, [this]() {
        new LockScreenPresentationWatcher(this);
    });

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToStartGreeter, this, [this]() {
        if (m_screenLockerClientConnection) {
            m_screenLockerClientConnection->destroy();
            delete m_screenLockerClientConnection;
            m_screenLockerClientConnection = nullptr;
        }
        int clientFd = createScreenLockerConnection();
        if (clientFd < 0) {
            return;
        }
        ScreenLocker::KSldApp::self()->setWaylandFd(clientFd);
    });

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::unlocked, this, [this]() {
        if (m_screenLockerClientConnection) {
            m_screenLockerClientConnection->destroy();
            delete m_screenLockerClientConnection;
            m_screenLockerClientConnection = nullptr;
        }

        ScreenLocker::KSldApp::self()->setWaylandFd(-1);
    });

    connect(screenLockerApp, &ScreenLocker::KSldApp::lockStateChanged, this, &WaylandServer::lockStateChanged);

    ScreenLocker::KSldApp::self()->initialize();

    if (kwinApp()->initiallyLocked()) {
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
    return socket.fd;
}

#if KWIN_BUILD_X11
FileDescriptor WaylandServer::createXWaylandConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return FileDescriptor();
    }
    m_xwaylandConnection = socket.connection;

    m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    connect(kwinApp(), &Application::xwaylandScaleChanged, m_xwaylandConnection, [this]() {
        m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    });

    return FileDescriptor(socket.fd);
}

void WaylandServer::destroyXWaylandConnection()
{
    if (!m_xwaylandConnection) {
        return;
    }
    m_xwaylandConnection->destroy();
    m_xwaylandConnection = nullptr;
}
#endif

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

static Window *findWindowInList(const QList<Window *> &windows, const SurfaceInterface *surface)
{
    auto it = std::find_if(windows.begin(), windows.end(), [surface](Window *w) {
        return w->surface() == surface;
    });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}

Window *WaylandServer::findWindow(const SurfaceInterface *surface) const
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
    if (!kwinApp()->supportsLockScreen()) {
        return false;
    }
    return ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked || ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::AcquiringLock;
#else
    return false;
#endif
}

bool WaylandServer::isKeyboardShortcutsInhibited() const
{
    auto surface = seat()->focusedKeyboardSurface();
    if (surface) {
        auto inhibitor = keyboardShortcutsInhibitManager()->findInhibitor(surface, seat());
        if (inhibitor && inhibitor->isActive()) {
            return true;
        }
#if KWIN_BUILD_X11
        if (m_xWaylandKeyboardGrabManager->hasGrab(surface, seat())) {
            return true;
        }
#endif
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

LinuxDrmSyncObjV1Interface *WaylandServer::linuxSyncObj() const
{
    return m_linuxDrmSyncObj;
}

ExternalBrightnessV1 *WaylandServer::externalBrightness() const
{
    return m_externalBrightness;
}

PointerWarpV1 *WaylandServer::pointerWarp() const
{
    return m_pointerWarp;
}

void WaylandServer::setRenderBackend(RenderBackend *backend)
{
    if (backend->drmDevice()->supportsSyncObjTimelines()) {
        // ensure the DRM_IOCTL_SYNCOBJ_EVENTFD ioctl is supported
        const auto linuxVersion = linuxKernelVersion();
        if (linuxVersion.majorVersion() < 6 && linuxVersion.minorVersion() < 6) {
            return;
        }
        // also ensure the implementation isn't totally broken, see https://lists.freedesktop.org/archives/dri-devel/2024-January/439101.html
        if (linuxVersion.majorVersion() == 6 && (linuxVersion.minorVersion() == 7 || (linuxVersion.minorVersion() == 6 && linuxVersion.patchVersion() < 19))) {
            return;
        }
        if (!m_linuxDrmSyncObj) {
            m_linuxDrmSyncObj = new LinuxDrmSyncObjV1Interface(m_display, m_display, backend->drmDevice());
        }
    } else if (m_linuxDrmSyncObj) {
        m_linuxDrmSyncObj->remove();
        m_linuxDrmSyncObj = nullptr;
    }
}

#if KWIN_BUILD_SCREENLOCKER
WaylandServer::LockScreenPresentationWatcher::LockScreenPresentationWatcher(WaylandServer *server)
{
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::unlocked, this, [this] {
        delete this;
    });
    connect(server, &WaylandServer::windowAdded, this, [this](Window *window) {
        if (window->isLockScreen()) {
            // only signal lockScreenShown once all outputs have been presented at least once
            connect(window->output()->renderLoop(), &RenderLoop::framePresented, this, [this, windowGuard = QPointer(window)]() {
                // window might be destroyed before a frame is presented, so it's wrapped in QPointer
                if (windowGuard) {
                    m_signaledOutputs << windowGuard->output();
                    if (m_signaledOutputs.size() == workspace()->outputs().size()) {
                        ScreenLocker::KSldApp::self()->lockScreenShown();
                        delete this;
                    }
                }
            });
        }
    });
    QTimer::singleShot(1000, this, [this]() {
        ScreenLocker::KSldApp::self()->lockScreenShown();
        delete this;
    });
}
#endif
}

#include "moc_wayland_server.cpp"
