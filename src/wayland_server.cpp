/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_server.h"
#include "abstract_wayland_output.h"
#include "x11client.h"
#include "platform.h"
#include "composite.h"
#include "idle_inhibition.h"
#include "inputpanelv1integration.h"
#include "keyboard_input.h"
#include "screens.h"
#include "layershellv1integration.h"
#include "main.h"
#include "xdgshellintegration.h"
#include "workspace.h"
#include "xdgshellclient.h"
#include "xdgactivationv1.h"
#include "unmanaged.h"
#include "utils/serviceutils.h"
#include "waylandoutput.h"
#include "waylandoutputdevicev2.h"
#include "virtualdesktops.h"

// Server
#include <KWaylandServer/appmenu_interface.h>
#include <KWaylandServer/compositor_interface.h>
#include <KWaylandServer/datadevicemanager_interface.h>
#include <KWaylandServer/datasource_interface.h>
#include <KWaylandServer/display.h>
#include <KWaylandServer/dpms_interface.h>
#include <KWaylandServer/idle_interface.h>
#include <KWaylandServer/idleinhibit_v1_interface.h>
#include <KWaylandServer/linuxdmabufv1clientbuffer.h>
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/plasmashell_interface.h>
#include <KWaylandServer/plasmavirtualdesktop_interface.h>
#include <KWaylandServer/plasmawindowmanagement_interface.h>
#include <KWaylandServer/pointerconstraints_v1_interface.h>
#include <KWaylandServer/pointergestures_v1_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/server_decoration_interface.h>
#include <KWaylandServer/server_decoration_palette_interface.h>
#include <KWaylandServer/shadow_interface.h>
#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/blur_interface.h>
#include <KWaylandServer/outputmanagement_v2_interface.h>
#include <KWaylandServer/outputconfiguration_v2_interface.h>
#include <KWaylandServer/primaryoutput_v1_interface.h>
#include <KWaylandServer/xdgactivation_v1_interface.h>
#include <KWaylandServer/xdgdecoration_v1_interface.h>
#include <KWaylandServer/xdgshell_interface.h>
#include <KWaylandServer/xdgforeign_v2_interface.h>
#include <KWaylandServer/xdgoutput_v1_interface.h>
#include <KWaylandServer/keystate_interface.h>
#include <KWaylandServer/filtered_display.h>
#include <KWaylandServer/keyboard_shortcuts_inhibit_v1_interface.h>
#include <KWaylandServer/inputmethod_v1_interface.h>
#include <KWaylandServer/tablet_v2_interface.h>
#include <KWaylandServer/viewporter_interface.h>
#include <KWaylandServer/datacontroldevicemanager_v1_interface.h>
#include <KWaylandServer/primaryselectiondevicemanager_v1_interface.h>
#include <KWaylandServer/relativepointer_v1_interface.h>

// Qt
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QWindow>

// system
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

//screenlocker
#include <KScreenLocker/KsldApp>

using namespace KWaylandServer;

namespace KWin
{

KWIN_SINGLETON_FACTORY(WaylandServer)

class KWinDisplay : public KWaylandServer::FilteredDisplay
{
public:
    KWinDisplay(QObject *parent)
        : KWaylandServer::FilteredDisplay(parent)
    {}

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

    bool isTrustedOrigin(KWaylandServer::ClientConnection *client) const {
        const auto fullPathSha = sha256(client->executablePath());
        const auto localSha = sha256(QLatin1String("/proc/") + QString::number(client->processId()) + QLatin1String("/exe"));
        const bool trusted = !localSha.isEmpty() && fullPathSha == localSha;

        if (!trusted) {
            qCWarning(KWIN_CORE) << "Could not trust" << client->executablePath() << "sha" << localSha << fullPathSha;
        }

        return trusted;
    }

    QStringList fetchRequestedInterfaces(KWaylandServer::ClientConnection *client) const {
        return KWin::fetchRequestedInterfaces(client->executablePath());
    }

    const QSet<QByteArray> interfacesBlackList = {
        QByteArrayLiteral("org_kde_kwin_remote_access_manager"),
        QByteArrayLiteral("org_kde_plasma_window_management"),
        QByteArrayLiteral("org_kde_kwin_fake_input"),
        QByteArrayLiteral("org_kde_kwin_keystate"),
        QByteArrayLiteral("zkde_screencast_unstable_v1"),
        QByteArrayLiteral("org_kde_plasma_activation_feedback"),
    };

    const QSet<QByteArray> inputmethodInterfaces = { "zwp_input_panel_v1", "zwp_input_method_v1" };

    QSet<QString> m_reported;

    bool allowInterface(KWaylandServer::ClientConnection *client, const QByteArray &interfaceName) override {
        if (client->processId() == getpid()) {
            return true;
        }

        if (client != waylandServer()->inputMethodConnection() && inputmethodInterfaces.contains(interfaceName)) {
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
                    if (!m_reported.contains({id})) {
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
    , m_display(new KWinDisplay(this))
{
    qRegisterMetaType<KWaylandServer::OutputInterface::DpmsMode>();
}

WaylandServer::~WaylandServer()
{
    s_self = nullptr;
}

KWaylandServer::ClientConnection *WaylandServer::xWaylandConnection() const
{
    return m_xwaylandConnection;
}

KWaylandServer::ClientConnection *WaylandServer::inputMethodConnection() const
{
    return m_inputMethodServerConnection;
}

void WaylandServer::registerShellClient(AbstractClient *client)
{
    if (client->isLockScreen()) {
        ScreenLocker::KSldApp::self()->lockScreenShown();
    }

    if (client->readyForPainting()) {
        Q_EMIT shellClientAdded(client);
    } else {
        connect(client, &AbstractClient::windowShown, this, &WaylandServer::shellClientShown);
    }
    m_clients << client;
}

void WaylandServer::registerXdgToplevelClient(XdgToplevelClient *client)
{
    // TODO: Find a better way and more generic to install extensions.

    SurfaceInterface *surface = client->surface();

    registerShellClient(client);

    if (auto shellSurface = PlasmaShellSurfaceInterface::get(surface)) {
        client->installPlasmaShellSurface(shellSurface);
    }
    if (auto decoration = ServerSideDecorationInterface::get(surface)) {
        client->installServerDecoration(decoration);
    }
    if (auto decoration = XdgToplevelDecorationV1Interface::get(client->shellSurface())) {
        client->installXdgDecoration(decoration);
    }
    if (auto menu = m_appMenuManager->appMenuForSurface(surface)) {
        client->installAppMenu(menu);
    }
    if (auto palette = m_paletteManager->paletteForSurface(surface)) {
        client->installPalette(palette);
    }

    connect(m_XdgForeign, &XdgForeignV2Interface::transientChanged, client, [this](SurfaceInterface *child) {
        Q_EMIT foreignTransientChanged(child);
    });
}

void WaylandServer::registerXdgGenericClient(AbstractClient *client)
{
    XdgToplevelClient *toplevelClient = qobject_cast<XdgToplevelClient *>(client);
    if (toplevelClient) {
        registerXdgToplevelClient(toplevelClient);
        return;
    }
    XdgPopupClient *popupClient = qobject_cast<XdgPopupClient *>(client);
    if (popupClient) {
        registerShellClient(popupClient);
        if (auto shellSurface = PlasmaShellSurfaceInterface::get(client->surface())) {
            popupClient->installPlasmaShellSurface(shellSurface);
        }
        return;
    }
    qCDebug(KWIN_CORE) << "Received invalid xdg client:" << client->surface();
}

void WaylandServer::initPlatform()
{
    connect(kwinApp()->platform(), &Platform::outputAdded, this, &WaylandServer::handleOutputAdded);
    connect(kwinApp()->platform(), &Platform::outputRemoved, this, &WaylandServer::handleOutputRemoved);

    connect(kwinApp()->platform(), &Platform::outputEnabled, this, &WaylandServer::handleOutputEnabled);
    connect(kwinApp()->platform(), &Platform::outputDisabled, this, &WaylandServer::handleOutputDisabled);

    connect(kwinApp()->platform(), &Platform::primaryOutputChanged, this, [this] (AbstractOutput *primaryOutput) {
        m_primary->setPrimaryOutput(primaryOutput ? primaryOutput->name() : QString());
    });
    if (auto primaryOutput = kwinApp()->platform()->primaryOutput()) {
        m_primary->setPrimaryOutput(primaryOutput->name());
    }

    const QVector<AbstractOutput *> outputs = kwinApp()->platform()->outputs();
    for (AbstractOutput *output : outputs) {
        handleOutputAdded(output);
    }

    const QVector<AbstractOutput *> enabledOutputs = kwinApp()->platform()->enabledOutputs();
    for (AbstractOutput *output : enabledOutputs) {
        handleOutputEnabled(output);
    }
}

void WaylandServer::handleOutputAdded(AbstractOutput *output)
{
    auto o = static_cast<AbstractWaylandOutput *>(output);
    if (!o->isPlaceholder()) {
        m_waylandOutputDevices.insert(o, new WaylandOutputDevice(o));
    }
}

void WaylandServer::handleOutputRemoved(AbstractOutput *output)
{
    auto o = static_cast<AbstractWaylandOutput *>(output);
    if (!o->isPlaceholder()) {
        delete m_waylandOutputDevices.take(o);
    }
}

void WaylandServer::handleOutputEnabled(AbstractOutput *output)
{
    auto o = static_cast<AbstractWaylandOutput *>(output);
    if (!o->isPlaceholder()) {
        m_waylandOutputs.insert(o, new WaylandOutput(o));
    }
}

void WaylandServer::handleOutputDisabled(AbstractOutput *output)
{
    auto o = static_cast<AbstractWaylandOutput *>(output);
    if (!o->isPlaceholder()) {
        delete m_waylandOutputs.take(o);
    }
}

AbstractWaylandOutput *WaylandServer::findOutput(KWaylandServer::OutputInterface *outputIface) const
{
    for (auto it = m_waylandOutputs.constBegin(); it != m_waylandOutputs.constEnd(); ++it) {
        if ((*it)->waylandOutput() == outputIface) {
            return it.key();
        }
    }
    return nullptr;
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
    m_compositor = new CompositorInterface(m_display, m_display);
    connect(m_compositor, &CompositorInterface::surfaceCreated, this,
        [this] (SurfaceInterface *surface) {
            // check whether we have a Toplevel with the Surface's id
            Workspace *ws = Workspace::self();
            if (!ws) {
                // it's possible that a Surface gets created before Workspace is created
                return;
            }
            if (surface->client() != xWaylandConnection()) {
                // setting surface is only relevant for Xwayland clients
                return;
            }

            X11Client *client = ws->findClient([surface](const X11Client *client) {
                return client->pendingSurfaceId() == surface->id();
            });
            if (client) {
                client->setSurface(surface);
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
        }
    );

    m_tabletManagerV2 = new TabletManagerV2Interface(m_display, m_display);
    m_keyboardShortcutsInhibitManager = new KeyboardShortcutsInhibitManagerV1Interface(m_display, m_display);

    auto inputPanelV1Integration = new InputPanelV1Integration(this);
    connect(inputPanelV1Integration, &InputPanelV1Integration::clientCreated,
            this, &WaylandServer::registerShellClient);

    auto xdgShellIntegration = new XdgShellIntegration(this);
    connect(xdgShellIntegration, &XdgShellIntegration::clientCreated,
            this, &WaylandServer::registerXdgGenericClient);

    auto layerShellV1Integration = new LayerShellV1Integration(this);
    connect(layerShellV1Integration, &LayerShellV1Integration::clientCreated,
            this, &WaylandServer::registerShellClient);

    m_xdgDecorationManagerV1 = new XdgDecorationManagerV1Interface(m_display, m_display);
    connect(m_xdgDecorationManagerV1, &XdgDecorationManagerV1Interface::decorationCreated, this,
        [this](XdgToplevelDecorationV1Interface *decoration) {
            if (XdgToplevelClient *toplevel = findXdgToplevelClient(decoration->toplevel()->surface())) {
                toplevel->installXdgDecoration(decoration);
            }
        }
    );

    new ViewporterInterface(m_display, m_display);
    m_display->createShm();
    m_seat = new SeatInterface(m_display, m_display);
    new PointerGesturesV1Interface(m_display, m_display);
    new PointerConstraintsV1Interface(m_display, m_display);
    new RelativePointerManagerV1Interface(m_display, m_display);
    m_dataDeviceManager = new DataDeviceManagerInterface(m_display, m_display);
    new DataControlDeviceManagerV1Interface(m_display, m_display);
    new PrimarySelectionDeviceManagerV1Interface(m_display, m_display);
    m_idle = new IdleInterface(m_display, m_display);
    auto idleInhibition = new IdleInhibition(m_idle);
    connect(this, &WaylandServer::shellClientAdded, idleInhibition, &IdleInhibition::registerClient);
    new IdleInhibitManagerV1Interface(m_display, m_display);
    m_plasmaShell = new PlasmaShellInterface(m_display, m_display);
    connect(m_plasmaShell, &PlasmaShellInterface::surfaceCreated, this,
        [this] (PlasmaShellSurfaceInterface *surface) {
            if (XdgSurfaceClient *client = findXdgSurfaceClient(surface->surface())) {
                client->installPlasmaShellSurface(surface);
            }
        }
    );
    m_appMenuManager = new AppMenuManagerInterface(m_display, m_display);
    connect(m_appMenuManager, &AppMenuManagerInterface::appMenuCreated, this,
        [this] (AppMenuInterface *appMenu) {
            if (XdgToplevelClient *client = findXdgToplevelClient(appMenu->surface())) {
                client->installAppMenu(appMenu);
            }
        }
    );
    m_paletteManager = new ServerSideDecorationPaletteManagerInterface(m_display, m_display);
    connect(m_paletteManager, &ServerSideDecorationPaletteManagerInterface::paletteCreated, this,
        [this] (ServerSideDecorationPaletteInterface *palette) {
            if (XdgToplevelClient *client = findXdgToplevelClient(palette->surface())) {
                client->installPalette(palette);
            }
        }
    );

    m_windowManagement = new PlasmaWindowManagementInterface(m_display, m_display);
    m_windowManagement->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
    connect(m_windowManagement, &PlasmaWindowManagementInterface::requestChangeShowingDesktop, this,
        [] (PlasmaWindowManagementInterface::ShowingDesktopState state) {
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
        }
    );

    m_virtualDesktopManagement = new PlasmaVirtualDesktopManagementInterface(m_display, m_display);
    m_windowManagement->setPlasmaVirtualDesktopManagementInterface(m_virtualDesktopManagement);

    m_plasmaActivationFeedback = new PlasmaWindowActivationFeedbackInterface(m_display, m_display);

    new ShadowManagerInterface(m_display, m_display);
    new DpmsManagerInterface(m_display, m_display);

    m_decorationManager = new ServerSideDecorationManagerInterface(m_display, m_display);
    connect(m_decorationManager, &ServerSideDecorationManagerInterface::decorationCreated, this,
        [this] (ServerSideDecorationInterface *decoration) {
            if (XdgToplevelClient *client = findXdgToplevelClient(decoration->surface())) {
                client->installServerDecoration(decoration);
            }
        }
    );

    m_outputManagement = new OutputManagementV2Interface(m_display, m_display);
    connect(m_outputManagement, &OutputManagementV2Interface::configurationChangeRequested,
            this, [](KWaylandServer::OutputConfigurationV2Interface *config) {
                kwinApp()->platform()->requestOutputsChange(config);
    });
    m_primary = new PrimaryOutputV1Interface(m_display, m_display);

    m_xdgOutputManagerV1 = new XdgOutputManagerV1Interface(m_display, m_display);
    new SubCompositorInterface(m_display, m_display);
    m_XdgForeign = new XdgForeignV2Interface(m_display, m_display);
    m_keyState = new KeyStateInterface(m_display, m_display);
    m_inputMethod = new InputMethodV1Interface(m_display, m_display);

    auto activation = new KWaylandServer::XdgActivationV1Interface(m_display, this);
    auto init = [this, activation] {
        new XdgActivationV1Integration(activation, this);
    };
    if (Workspace::self()) {
        init();
    } else {
        connect(static_cast<Application*>(qApp), &Application::workspaceCreated, this, init);
    }

    return true;
}

KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *WaylandServer::linuxDmabuf()
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

void WaylandServer::shellClientShown(Toplevel *toplevel)
{
    AbstractClient *client = qobject_cast<AbstractClient *>(toplevel);
    if (!client) {
        qCWarning(KWIN_CORE) << "Failed to cast a Toplevel which is supposed to be an AbstractClient to AbstractClient";
        return;
    }
    disconnect(client, &AbstractClient::windowShown, this, &WaylandServer::shellClientShown);
    Q_EMIT shellClientAdded(client);
}

void WaylandServer::initWorkspace()
{
    // TODO: Moe the keyboard leds somewhere else.
    updateKeyState(input()->keyboard()->xkb()->leds());
    connect(input()->keyboard(), &KeyboardInputRedirection::ledsChanged, this, &WaylandServer::updateKeyState);

    VirtualDesktopManager::self()->setVirtualDesktopManagement(m_virtualDesktopManagement);

    if (m_windowManagement) {
        connect(workspace(), &Workspace::showingDesktopChanged, this,
            [this] (bool set) {
                using namespace KWaylandServer;
                m_windowManagement->setShowingDesktopState(set ?
                    PlasmaWindowManagementInterface::ShowingDesktopState::Enabled :
                    PlasmaWindowManagementInterface::ShowingDesktopState::Disabled
                );
            }
        );

        connect(workspace(), &Workspace::workspaceInitialized, this, [this] {
            auto f = [this] () {
                QVector<quint32> ids;
                QVector<QString> uuids;
                for (Toplevel *toplevel : workspace()->stackingOrder()) {
                    auto *client = qobject_cast<AbstractClient *>(toplevel);
                    if (client && client->windowManagementInterface()) {
                        ids << client->windowManagementInterface()->internalId();
                        uuids << client->windowManagementInterface()->uuid();
                    }
                }
                m_windowManagement->setStackingOrder(ids);
                m_windowManagement->setStackingOrderUuids(uuids);
            };
            f();
            connect(workspace(), &Workspace::stackingOrderChanged, this, f);
        });
    }

    if (hasScreenLockerIntegration()) {
        initScreenLocker();
    }
    Q_EMIT initialized();
}

void WaylandServer::initScreenLocker()
{
    auto *screenLockerApp = ScreenLocker::KSldApp::self();

    ScreenLocker::KSldApp::self()->setGreeterEnvironment(kwinApp()->processStartupEnvironment());
    ScreenLocker::KSldApp::self()->initialize();

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToLock, this,
        [this, screenLockerApp] () {
            if (m_screenLockerClientConnection) {
                // Already sent data to KScreenLocker.
                return;
            }
            int clientFd = createScreenLockerConnection();
            if (clientFd < 0) {
                return;
            }
            ScreenLocker::KSldApp::self()->setWaylandFd(clientFd);

            const QVector<SeatInterface *> seatIfaces = m_display->seats();
            for (auto *seat : seatIfaces) {
                connect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                        screenLockerApp, &ScreenLocker::KSldApp::userActivity);
            }
        }
    );

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::unlocked, this,
        [this, screenLockerApp] () {
            if (m_screenLockerClientConnection) {
                m_screenLockerClientConnection->destroy();
                delete m_screenLockerClientConnection;
                m_screenLockerClientConnection = nullptr;
            }

            const QVector<SeatInterface *> seatIfaces = m_display->seats();
            for (auto *seat : seatIfaces) {
                disconnect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                           screenLockerApp, &ScreenLocker::KSldApp::userActivity);
            }
            ScreenLocker::KSldApp::self()->setWaylandFd(-1);
        }
    );

    if (m_initFlags.testFlag(InitializationFlag::LockScreen)) {
        ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    }
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
    connect(m_screenLockerClientConnection, &KWaylandServer::ClientConnection::disconnected,
            this, [this] { m_screenLockerClientConnection = nullptr; });
    return socket.fd;
}

int WaylandServer::createXWaylandConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_xwaylandConnection = socket.connection;
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

void WaylandServer::removeClient(AbstractClient *c)
{
    m_clients.removeAll(c);
    Q_EMIT shellClientRemoved(c);
}

static AbstractClient *findClientInList(const QList<AbstractClient *> &clients, const KWaylandServer::SurfaceInterface *surface)
{
    auto it = std::find_if(clients.begin(), clients.end(),
        [surface] (AbstractClient *c) {
            return c->surface() == surface;
        }
    );
    if (it == clients.end()) {
        return nullptr;
    }
    return *it;
}

AbstractClient *WaylandServer::findClient(const KWaylandServer::SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    if (AbstractClient *c = findClientInList(m_clients, surface)) {
        return c;
    }
    return nullptr;
}

XdgToplevelClient *WaylandServer::findXdgToplevelClient(SurfaceInterface *surface) const
{
    return qobject_cast<XdgToplevelClient *>(findClient(surface));
}

XdgSurfaceClient *WaylandServer::findXdgSurfaceClient(SurfaceInterface *surface) const
{
    return qobject_cast<XdgSurfaceClient *>(findClient(surface));
}

bool WaylandServer::isScreenLocked() const
{
    if (!hasScreenLockerIntegration()) {
        return false;
    }
    return ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked ||
           ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::AcquiringLock;
}

bool WaylandServer::hasScreenLockerIntegration() const
{
    return !m_initFlags.testFlag(InitializationFlag::NoLockScreenIntegration);
}

bool WaylandServer::hasGlobalShortcutSupport() const
{
    return !m_initFlags.testFlag(InitializationFlag::NoGlobalShortcuts);
}

void WaylandServer::simulateUserActivity()
{
    if (m_idle) {
        m_idle->simulateUserActivity();
    }
}

void WaylandServer::updateKeyState(KWin::LEDs leds)
{
    if (!m_keyState)
        return;

    m_keyState->setState(KeyStateInterface::Key::CapsLock, leds & KWin::LED::CapsLock ? KeyStateInterface::State::Locked : KeyStateInterface::State::Unlocked);
    m_keyState->setState(KeyStateInterface::Key::NumLock, leds & KWin::LED::NumLock ? KeyStateInterface::State::Locked : KeyStateInterface::State::Unlocked);
    m_keyState->setState(KeyStateInterface::Key::ScrollLock, leds & KWin::LED::ScrollLock ? KeyStateInterface::State::Locked : KeyStateInterface::State::Unlocked);
}

bool WaylandServer::isKeyboardShortcutsInhibited() const
{
    auto surface = seat()->focusedKeyboardSurface();
    if (surface) {
        auto inhibitor = keyboardShortcutsInhibitManager()->findInhibitor(surface, seat());
        return inhibitor && inhibitor->isActive();
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

}
