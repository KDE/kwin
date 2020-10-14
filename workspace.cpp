/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "workspace.h"
// kwin libs
#include <kwinglplatform.h>
// kwin
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "atoms.h"
#include "x11client.h"
#include "composite.h"
#include "cursor.h"
#include "dbusinterface.h"
#include "deleted.h"
#include "effects.h"
#include "focuschain.h"
#include "group.h"
#include "input.h"
#include "internal_client.h"
#include "logind.h"
#include "moving_client_x11_filter.h"
#include "killwindow.h"
#include "netinfo.h"
#include "outline.h"
#include "placement.h"
#include "rules.h"
#include "screenedge.h"
#include "screens.h"
#include "platform.h"
#include "scripting/scripting.h"
#include "syncalarmx11filter.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "unmanaged.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "was_user_interaction_x11_filter.h"
#include "wayland_server.h"
#include "xcbutils.h"
#include "main.h"
#include "decorations/decorationbridge.h"
#include "xwaylandclient.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KStartupInfo>
// Qt
#include <QtConcurrentRun>

namespace KWin
{

extern int screen_number;
extern bool is_multihead;

X11EventFilterContainer::X11EventFilterContainer(X11EventFilter *filter)
    : m_filter(filter)
{
}

X11EventFilter *X11EventFilterContainer::filter() const
{
    return m_filter;
}

ColorMapper::ColorMapper(QObject *parent)
    : QObject(parent)
    , m_default(kwinApp()->x11DefaultScreen()->default_colormap)
    , m_installed(kwinApp()->x11DefaultScreen()->default_colormap)
{
}

ColorMapper::~ColorMapper()
{
}

void ColorMapper::update()
{
    xcb_colormap_t cmap = m_default;
    if (X11Client *c = dynamic_cast<X11Client *>(Workspace::self()->activeClient())) {
        if (c->colormap() != XCB_COLORMAP_NONE) {
            cmap = c->colormap();
        }
    }
    if (cmap != m_installed) {
        xcb_install_colormap(connection(), cmap);
        m_installed = cmap;
    }
}

Workspace* Workspace::_self = nullptr;

Workspace::Workspace()
    : QObject(nullptr)
    , m_compositor(nullptr)
    // Unsorted
    , m_quickTileCombineTimer(nullptr)
    , active_popup(nullptr)
    , active_popup_client(nullptr)
    , m_initialDesktop(1)
    , active_client(nullptr)
    , last_active_client(nullptr)
    , most_recently_raised(nullptr)
    , movingClient(nullptr)
    , delayfocus_client(nullptr)
    , force_restacking(false)
    , showing_desktop(false)
    , was_user_interaction(false)
    , block_focus(0)
    , m_userActionsMenu(new UserActionsMenu(this))
    , client_keys_dialog(nullptr)
    , client_keys_client(nullptr)
    , global_shortcuts_disabled_for_client(false)
    , workspaceInit(true)
    , set_active_client_recursion(0)
    , block_stacking_updates(0)
    , m_sessionManager(new SessionManager(this))
{
    // If KWin was already running it saved its configuration after loosing the selection -> Reread
    QFuture<void> reparseConfigFuture = QtConcurrent::run(options, &Options::reparseConfiguration);

    ApplicationMenu::create(this);

    _self = this;

#ifdef KWIN_BUILD_ACTIVITIES
    Activities *activities = nullptr;
    if (kwinApp()->usesKActivities()) {
        activities = Activities::create(this);
    }
    if (activities) {
        connect(activities, SIGNAL(currentChanged(QString)), SLOT(updateCurrentActivity(QString)));
    }
#endif

    // PluginMgr needs access to the config file, so we need to wait for it for finishing
    reparseConfigFuture.waitForFinished();

    options->loadConfig();
    options->loadCompositingConfig(false);

    delayFocusTimer = nullptr;

    m_quickTileCombineTimer = new QTimer(this);
    m_quickTileCombineTimer->setSingleShot(true);

    RuleBook::create(this)->load();

    kwinApp()->createScreens();
    ScreenEdges::create(this);

    // VirtualDesktopManager needs to be created prior to init shortcuts
    // and prior to TabBox, due to TabBox connecting to signals
    // actual initialization happens in init()
    VirtualDesktopManager::create(this);
    //dbus interface
    new VirtualDesktopManagerDBusInterface(VirtualDesktopManager::self());

#ifdef KWIN_BUILD_TABBOX
    // need to create the tabbox before compositing scene is setup
    TabBox::TabBox::create(this);
#endif

    if (Compositor::self()) {
        m_compositor = Compositor::self();
    } else {
        Q_ASSERT(kwinApp()->operationMode() == Application::OperationMode::OperationModeX11);
        m_compositor = X11Compositor::create(this);
    }
    connect(this, &Workspace::currentDesktopChanged, m_compositor, &Compositor::addRepaintFull);
    connect(m_compositor, &QObject::destroyed, this, [this] { m_compositor = nullptr; });

    auto decorationBridge = Decoration::DecorationBridge::create(this);
    decorationBridge->init();
    connect(this, &Workspace::configChanged, decorationBridge, &Decoration::DecorationBridge::reconfigure);

    connect(m_sessionManager, &SessionManager::loadSessionRequested, this, &Workspace::loadSessionInfo);

    connect(m_sessionManager, &SessionManager::prepareSessionSaveRequested, this, [this](const QString &name) {
        storeSession(name, SMSavePhase0);
    });
    connect(m_sessionManager, &SessionManager::finishSessionSaveRequested, this, [this](const QString &name) {
        storeSession(name, SMSavePhase2);
    });

    new DBusInterface(this);

    Outline::create(this);

    initShortcuts();

    init();
}

void Workspace::init()
{
    KSharedConfigPtr config = kwinApp()->config();
    Screens *screens = Screens::self();
    // get screen support
    connect(screens, SIGNAL(changed()), SLOT(desktopResized()));
    screens->setConfig(config);
    screens->reconfigure();
    connect(options, SIGNAL(configChanged()), screens, SLOT(reconfigure()));
    ScreenEdges *screenEdges = ScreenEdges::self();
    screenEdges->setConfig(config);
    screenEdges->init();
    connect(options, SIGNAL(configChanged()), screenEdges, SLOT(reconfigure()));
    connect(VirtualDesktopManager::self(), SIGNAL(layoutChanged(int,int)), screenEdges, SLOT(updateLayout()));
    connect(this, &Workspace::clientActivated, screenEdges, &ScreenEdges::checkBlocking);

    FocusChain *focusChain = FocusChain::create(this);
    connect(this, &Workspace::clientRemoved, focusChain, &FocusChain::remove);
    connect(this, &Workspace::clientActivated, focusChain, &FocusChain::setActiveClient);
    connect(VirtualDesktopManager::self(), SIGNAL(countChanged(uint,uint)), focusChain, SLOT(resize(uint,uint)));
    connect(VirtualDesktopManager::self(), SIGNAL(currentChanged(uint,uint)), focusChain, SLOT(setCurrentDesktop(uint,uint)));
    connect(options, SIGNAL(separateScreenFocusChanged(bool)), focusChain, SLOT(setSeparateScreenFocus(bool)));
    focusChain->setSeparateScreenFocus(options->isSeparateScreenFocus());

    // create VirtualDesktopManager and perform dependency injection
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, &VirtualDesktopManager::desktopRemoved, this,
        [this](KWin::VirtualDesktop *desktop) {
            //Wayland
            if (kwinApp()->operationMode() == Application::OperationModeWaylandOnly ||
                kwinApp()->operationMode() == Application::OperationModeXwayland) {
                for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
                    if (!(*it)->desktops().contains(desktop)) {
                        continue;
                    }
                    if ((*it)->desktops().count() > 1) {
                        (*it)->leaveDesktop(desktop);
                    } else {
                        sendClientToDesktop(*it, qMin(desktop->x11DesktopNumber(), VirtualDesktopManager::self()->count()), true);
                    }
                }
            //X11
            } else {
                for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
                    if (!(*it)->isOnAllDesktops() && ((*it)->desktop() > static_cast<int>(VirtualDesktopManager::self()->count()))) {
                        sendClientToDesktop(*it, VirtualDesktopManager::self()->count(), true);
                    }
                }
            }
        }
    );

    connect(vds, SIGNAL(countChanged(uint,uint)), SLOT(slotDesktopCountChanged(uint,uint)));
    connect(vds, SIGNAL(currentChanged(uint,uint)), SLOT(slotCurrentDesktopChanged(uint,uint)));
    vds->setNavigationWrappingAround(options->isRollOverDesktops());
    connect(options, SIGNAL(rollOverDesktopsChanged(bool)), vds, SLOT(setNavigationWrappingAround(bool)));
    vds->setConfig(config);

    // Now we know how many desktops we'll have, thus we initialize the positioning object
    Placement::create(this);

    // positioning object needs to be created before the virtual desktops are loaded.
    vds->load();
    vds->updateLayout();
    //makes sure any autogenerated id is saved, necessary as in case of xwayland, load will be called 2 times
    // load is needed to be called again when starting xwayalnd to sync to RootInfo, see BUG 385260
    vds->save();

    if (!VirtualDesktopManager::self()->setCurrent(m_initialDesktop))
        VirtualDesktopManager::self()->setCurrent(1);

    reconfigureTimer.setSingleShot(true);
    updateToolWindowsTimer.setSingleShot(true);

    connect(&reconfigureTimer, SIGNAL(timeout()), this, SLOT(slotReconfigure()));
    connect(&updateToolWindowsTimer, SIGNAL(timeout()), this, SLOT(slotUpdateToolWindows()));

    // TODO: do we really need to reconfigure everything when fonts change?
    // maybe just reconfigure the decorations? Move this into libkdecoration?
    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/KDEPlatformTheme"),
                                          QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"),
                                          this, SLOT(reconfigure()));

    active_client = nullptr;

    // We want to have some xcb connection while tearing down X11 components. We don't really
    // care if the xcb connection is broken or has an error.
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &Workspace::initializeX11);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this, &Workspace::cleanupX11);
    initializeX11();

    Scripting::create(this);

    if (auto server = waylandServer()) {
        connect(server, &WaylandServer::shellClientAdded, this, &Workspace::addShellClient);
        connect(server, &WaylandServer::shellClientRemoved, this, &Workspace::removeShellClient);
    }

    // SELI TODO: This won't work with unreasonable focus policies,
    // and maybe in rare cases also if the selected client doesn't
    // want focus
    workspaceInit = false;

    // broadcast that Workspace is ready, but first process all events.
    QMetaObject::invokeMethod(this, "workspaceInitialized", Qt::QueuedConnection);

    // TODO: ungrabXServer()
}

void Workspace::initializeX11()
{
    if (!kwinApp()->x11Connection()) {
        return;
    }

    atoms->retrieveHelpers();

    // first initialize the extensions
    Xcb::Extensions::self();
    m_colorMapper.reset(new ColorMapper(this));
    connect(this, &Workspace::clientActivated, m_colorMapper.data(), &ColorMapper::update);

    // Call this before XSelectInput() on the root window
    m_startup.reset(new KStartupInfo(
        KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this));

    // Select windowmanager privileges
    selectWmInputEventMask();

    // Compatibility
    int32_t data = 1;

    xcb_change_property(connection(), XCB_PROP_MODE_APPEND, rootWindow(), atoms->kwin_running,
                        atoms->kwin_running, 32, 1, &data);

    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        m_wasUserInteractionFilter.reset(new WasUserInteractionX11Filter);
        m_movingClientFilter.reset(new MovingClientX11Filter);
    }
    if (Xcb::Extensions::self()->isSyncAvailable()) {
        m_syncAlarmFilter.reset(new SyncAlarmX11Filter);
    }
    updateXTime(); // Needed for proper initialization of user_time in Client ctor

    const uint32_t nullFocusValues[] = {true};
    m_nullFocus.reset(new Xcb::Window(QRect(-1, -1, 1, 1), XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, nullFocusValues));
    m_nullFocus->map();

    RootInfo *rootInfo = RootInfo::create();
    const auto vds = VirtualDesktopManager::self();
    vds->setRootInfo(rootInfo);
    rootInfo->activate();

    // TODO: only in X11 mode
    // Extra NETRootInfo instance in Client mode is needed to get the values of the properties
    NETRootInfo client_info(connection(), NET::ActiveWindow | NET::CurrentDesktop);
    if (!qApp->isSessionRestored()) {
        m_initialDesktop = client_info.currentDesktop();
        vds->setCurrent(m_initialDesktop);
    }

    // TODO: better value
    rootInfo->setActiveWindow(None);
    focusToNull();

    if (!qApp->isSessionRestored())
        ++block_focus; // Because it will be set below

    {
        // Begin updates blocker block
        StackingUpdatesBlocker blocker(this);

        Xcb::Tree tree(rootWindow());
        xcb_window_t *wins = xcb_query_tree_children(tree.data());

        QVector<Xcb::WindowAttributes> windowAttributes(tree->children_len);
        QVector<Xcb::WindowGeometry> windowGeometries(tree->children_len);

        // Request the attributes and geometries of all toplevel windows
        for (int i = 0; i < tree->children_len; i++) {
            windowAttributes[i] = Xcb::WindowAttributes(wins[i]);
            windowGeometries[i] = Xcb::WindowGeometry(wins[i]);
        }

        // Get the replies
        for (int i = 0; i < tree->children_len; i++) {
            Xcb::WindowAttributes attr(windowAttributes.at(i));

            if (attr.isNull()) {
                continue;
            }

            if (attr->override_redirect) {
                if (attr->map_state == XCB_MAP_STATE_VIEWABLE &&
                    attr->_class != XCB_WINDOW_CLASS_INPUT_ONLY)
                    // ### This will request the attributes again
                    createUnmanaged(wins[i]);
            } else if (attr->map_state != XCB_MAP_STATE_UNMAPPED) {
                if (Application::wasCrash()) {
                    fixPositionAfterCrash(wins[i], windowGeometries.at(i).data());
                }

                // ### This will request the attributes again
                createClient(wins[i], true);
            }
        }

        // Propagate clients, will really happen at the end of the updates blocker block
        updateStackingOrder(true);

        saveOldScreenSizes();
        updateClientArea();

        // NETWM spec says we have to set it to (0,0) if we don't support it
        NETPoint* viewports = new NETPoint[VirtualDesktopManager::self()->count()];
        rootInfo->setDesktopViewport(VirtualDesktopManager::self()->count(), *viewports);
        delete[] viewports;
        QRect geom;
        for (int i = 0; i < screens()->count(); i++) {
            geom |= screens()->geometry(i);
        }
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
        rootInfo->setDesktopGeometry(desktop_geometry);
        setShowingDesktop(false);

    } // End updates blocker block

    // TODO: only on X11?
    AbstractClient* new_active_client = nullptr;
    if (!qApp->isSessionRestored()) {
        --block_focus;
        new_active_client = findClient(Predicate::WindowMatch, client_info.activeWindow());
    }
    if (new_active_client == nullptr
            && activeClient() == nullptr && should_get_focus.count() == 0) {
        // No client activated in manage()
        if (new_active_client == nullptr)
            new_active_client = topClientOnDesktop(VirtualDesktopManager::self()->current(), -1);
        if (new_active_client == nullptr)
            new_active_client = findDesktop(true, VirtualDesktopManager::self()->current());
    }
    if (new_active_client != nullptr)
        activateClient(new_active_client);
}

void Workspace::cleanupX11()
{
    // We expect that other components will unregister their X11 event filters after the
    // connection to the X server has been lost.

    StackingUpdatesBlocker blocker(this);

    // Use stacking_order, so that kwin --replace keeps stacking order.
    const QList<X11Client *> orderedClients = ensureStackingOrder(clients);
    for (X11Client *client : orderedClients) {
        client->releaseWindow(true);
        unconstrained_stacking_order.removeOne(client);
        stacking_order.removeOne(client);
    }

    // We need a shadow copy because windows get removed as we go through them.
    const QList<Unmanaged *> unmanaged = m_unmanaged;
    for (Unmanaged *overrideRedirect : unmanaged) {
        overrideRedirect->release(ReleaseReason::KWinShutsDown);
        unconstrained_stacking_order.removeOne(overrideRedirect);
        stacking_order.removeOne(overrideRedirect);
    }

    manual_overlays.clear();

    VirtualDesktopManager *desktopManager = VirtualDesktopManager::self();
    desktopManager->setRootInfo(nullptr);

    X11Client::cleanupX11();
    RootInfo::destroy();
    Xcb::Extensions::destroy();

    if (xcb_connection_t *connection = kwinApp()->x11Connection()) {
        xcb_delete_property(connection, kwinApp()->x11RootWindow(), atoms->kwin_running);
    }

    m_colorMapper.reset();
    m_movingClientFilter.reset();
    m_startup.reset();
    m_nullFocus.reset();
    m_syncAlarmFilter.reset();
    m_wasUserInteractionFilter.reset();
    m_xStackingQueryTree.reset();
}

Workspace::~Workspace()
{
    blockStackingUpdates(true);

    cleanupX11();

    if (waylandServer()) {
        const QList<AbstractClient *> shellClients = waylandServer()->clients();
        for (AbstractClient *client : shellClients) {
            client->destroyClient();
        }
    }

    // We need a shadow copy because clients get removed as we go through them.
    const QList<InternalClient *> internalClients = m_internalClients;
    for (InternalClient *client : internalClients) {
        client->destroyClient();
    }

    for (auto it = deleted.begin(); it != deleted.end();) {
        emit deletedRemoved(*it);
        it = deleted.erase(it);
    }

    delete RuleBook::self();
    kwinApp()->config()->sync();

    delete Placement::self();
    delete client_keys_dialog;
    qDeleteAll(session);

    _self = nullptr;
}

void Workspace::setupClientConnections(AbstractClient *c)
{
    connect(c, &Toplevel::needsRepaint, m_compositor, &Compositor::scheduleRepaint);
    connect(c, &AbstractClient::desktopPresenceChanged, this, &Workspace::desktopPresenceChanged);
    connect(c, &AbstractClient::minimizedChanged, this, std::bind(&Workspace::clientMinimizedChanged, this, c));
}

X11Client *Workspace::createClient(xcb_window_t w, bool is_mapped)
{
    StackingUpdatesBlocker blocker(this);
    X11Client *c = nullptr;
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        c = new X11Client();
    } else {
        c = new XwaylandClient();
    }
    setupClientConnections(c);
    if (X11Compositor *compositor = X11Compositor::self()) {
        connect(c, &X11Client::blockingCompositingChanged, compositor, &X11Compositor::updateClientCompositeBlocking);
    }
    connect(c, SIGNAL(clientFullScreenSet(KWin::X11Client *,bool,bool)), ScreenEdges::self(), SIGNAL(checkBlocking()));
    if (!c->manage(w, is_mapped)) {
        X11Client::deleteClient(c);
        return nullptr;
    }
    addClient(c);
    return c;
}

Unmanaged* Workspace::createUnmanaged(xcb_window_t w)
{
    if (X11Compositor *compositor = X11Compositor::self()) {
        if (compositor->checkForOverlayWindow(w)) {
            return nullptr;
        }
    }
    Unmanaged* c = new Unmanaged();
    if (!c->track(w)) {
        Unmanaged::deleteUnmanaged(c);
        return nullptr;
    }
    connect(c, &Unmanaged::needsRepaint, m_compositor, &Compositor::scheduleRepaint);
    addUnmanaged(c);
    emit unmanagedAdded(c);
    return c;
}

void Workspace::addClient(X11Client *c)
{
    Group* grp = findGroup(c->window());

    emit clientAdded(c);

    if (grp != nullptr)
        grp->gotLeader(c);

    if (c->isDesktop()) {
        if (active_client == nullptr && should_get_focus.isEmpty() && c->isOnCurrentDesktop())
            requestFocus(c);   // TODO: Make sure desktop is active after startup if there's no other window active
    } else {
        FocusChain::self()->update(c, FocusChain::Update);
    }
    clients.append(c);
    m_allClients.append(c);
    if (!unconstrained_stacking_order.contains(c))
        unconstrained_stacking_order.append(c);   // Raise if it hasn't got any stacking position yet
    if (!stacking_order.contains(c))    // It'll be updated later, and updateToolWindows() requires
        stacking_order.append(c);      // c to be in stacking_order
    markXStackingOrderAsDirty();
    updateClientArea(); // This cannot be in manage(), because the client got added only now
    updateClientLayer(c);
    if (c->isDesktop()) {
        raiseClient(c);
        // If there's no active client, make this desktop the active one
        if (activeClient() == nullptr && should_get_focus.count() == 0)
            activateClient(findDesktop(true, VirtualDesktopManager::self()->current()));
    }
    c->checkActiveModal();
    checkTransients(c->window());   // SELI TODO: Does this really belong here?
    updateStackingOrder(true);   // Propagate new client
    if (c->isUtility() || c->isMenu() || c->isToolbar())
        updateToolWindows(true);
    updateTabbox();
}

void Workspace::addUnmanaged(Unmanaged* c)
{
    m_unmanaged.append(c);
    markXStackingOrderAsDirty();
}

/**
 * Destroys the client \a c
 */
void Workspace::removeClient(X11Client *c)
{
    if (c == active_popup_client)
        closeActivePopup();
    if (m_userActionsMenu->isMenuClient(c)) {
        m_userActionsMenu->close();
    }

    if (client_keys_client == c)
        setupWindowShortcutDone(false);
    if (!c->shortcut().isEmpty()) {
        c->setShortcut(QString());   // Remove from client_keys
        clientShortcutUpdated(c);   // Needed, since this is otherwise delayed by setShortcut() and wouldn't run
    }

    Q_ASSERT(clients.contains(c));
    // TODO: if marked client is removed, notify the marked list
    clients.removeAll(c);
    m_allClients.removeAll(c);
    markXStackingOrderAsDirty();
    attention_chain.removeAll(c);
    Group* group = findGroup(c->window());
    if (group != nullptr)
        group->lostLeader();

    if (c == most_recently_raised)
        most_recently_raised = nullptr;
    should_get_focus.removeAll(c);
    if (c == active_client)
        active_client = nullptr;
    if (c == last_active_client)
        last_active_client = nullptr;
    if (c == delayfocus_client)
        cancelDelayFocus();

    emit clientRemoved(c);

    updateStackingOrder(true);
    updateClientArea();
    updateTabbox();
}

void Workspace::removeUnmanaged(Unmanaged* c)
{
    Q_ASSERT(m_unmanaged.contains(c));
    m_unmanaged.removeAll(c);
    emit unmanagedRemoved(c);
    markXStackingOrderAsDirty();
}

void Workspace::addDeleted(Deleted* c, Toplevel *orig)
{
    Q_ASSERT(!deleted.contains(c));
    deleted.append(c);
    const int unconstraintedIndex = unconstrained_stacking_order.indexOf(orig);
    if (unconstraintedIndex != -1) {
        unconstrained_stacking_order.replace(unconstraintedIndex, c);
    } else {
        unconstrained_stacking_order.append(c);
    }
    const int index = stacking_order.indexOf(orig);
    if (index != -1) {
        stacking_order.replace(index, c);
    } else {
        stacking_order.append(c);
    }
    markXStackingOrderAsDirty();
    connect(c, &Deleted::needsRepaint, m_compositor, &Compositor::scheduleRepaint);
}

void Workspace::removeDeleted(Deleted* c)
{
    Q_ASSERT(deleted.contains(c));
    emit deletedRemoved(c);
    deleted.removeAll(c);
    unconstrained_stacking_order.removeAll(c);
    stacking_order.removeAll(c);
    markXStackingOrderAsDirty();
    if (!c->wasClient()) {
        return;
    }
    if (X11Compositor *compositor = X11Compositor::self()) {
        compositor->updateClientCompositeBlocking();
    }
}

void Workspace::addShellClient(AbstractClient *client)
{
    setupClientConnections(client);
    client->updateDecoration(false);
    updateClientLayer(client);

    if (client->isPlaceable()) {
        const QRect area = clientArea(PlacementArea, Screens::self()->current(), client->desktop());
        bool placementDone = false;
        if (client->isInitialPositionSet()) {
            placementDone = true;
        }
        if (client->isFullScreen()) {
            placementDone = true;
        }
        if (client->maximizeMode() == MaximizeMode::MaximizeFull) {
            placementDone = true;
        }
        if (client->rules()->checkPosition(invalidPoint, true) != invalidPoint) {
            placementDone = true;
        }
        if (!placementDone) {
            client->placeIn(area);
        }
    }
    m_allClients.append(client);
    if (!unconstrained_stacking_order.contains(client)) {
        unconstrained_stacking_order.append(client); // Raise if it hasn't got any stacking position yet
    }
    if (!stacking_order.contains(client)) { // It'll be updated later, and updateToolWindows() requires
        stacking_order.append(client);      // client to be in stacking_order
    }

    markXStackingOrderAsDirty();
    updateStackingOrder(true);
    updateClientArea();
    if (client->wantsInput() && !client->isMinimized()) {
        activateClient(client);
    }
    updateTabbox();
    connect(client, &AbstractClient::windowShown, this, [this, client] {
        updateClientLayer(client);
        markXStackingOrderAsDirty();
        updateStackingOrder(true);
        updateClientArea();
        if (client->wantsInput()) {
            activateClient(client);
        }
    });
    connect(client, &AbstractClient::windowHidden, this, [this] {
        // TODO: update tabbox if it's displayed
        markXStackingOrderAsDirty();
        updateStackingOrder(true);
        updateClientArea();
    });
    emit clientAdded(client);
}

void Workspace::removeShellClient(AbstractClient *client)
{
    clientHidden(client);
    m_allClients.removeAll(client);
    if (client == most_recently_raised) {
        most_recently_raised = nullptr;
    }
    if (client == delayfocus_client) {
        cancelDelayFocus();
    }
    if (client == active_client) {
        active_client = nullptr;
    }
    if (client == last_active_client) {
        last_active_client = nullptr;
    }
    if (client_keys_client == client) {
        setupWindowShortcutDone(false);
    }
    if (!client->shortcut().isEmpty()) {
        client->setShortcut(QString());   // Remove from client_keys
    }
    emit clientRemoved(client);
    markXStackingOrderAsDirty();
    updateStackingOrder(true);
    updateClientArea();
    updateTabbox();
}

void Workspace::updateToolWindows(bool also_hide)
{
    // TODO: What if Client's transiency/group changes? should this be called too? (I'm paranoid, am I not?)
    if (!options->isHideUtilityWindowsForInactive()) {
        for (auto it = clients.constBegin(); it != clients.constEnd(); ++it)
            (*it)->hideClient(false);
        return;
    }
    const Group* group = nullptr;
    auto client = active_client;
    // Go up in transiency hiearchy, if the top is found, only tool transients for the top mainwindow
    // will be shown; if a group transient is group, all tools in the group will be shown
    while (client != nullptr) {
        if (!client->isTransient())
            break;
        if (client->groupTransient()) {
            group = client->group();
            break;
        }
        client = client->transientFor();
    }
    // Use stacking order only to reduce flicker, it doesn't matter if block_stacking_updates == 0,
    // I.e. if it's not up to date

    // SELI TODO: But maybe it should - what if a new client has been added that's not in stacking order yet?
    QVector<AbstractClient*> to_show, to_hide;
    for (auto it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        auto c = qobject_cast<AbstractClient*>(*it);
        if (!c) {
            continue;
        }
        if (c->isUtility() || c->isMenu() || c->isToolbar()) {
            bool show = true;
            if (!c->isTransient()) {
                if (!c->group() || c->group()->members().count() == 1)   // Has its own group, keep always visible
                    show = true;
                else if (client != nullptr && c->group() == client->group())
                    show = true;
                else
                    show = false;
            } else {
                if (group != nullptr && c->group() == group)
                    show = true;
                else if (client != nullptr && client->hasTransient(c, true))
                    show = true;
                else
                    show = false;
            }
            if (!show && also_hide) {
                const auto mainclients = c->mainClients();
                // Don't hide utility windows which are standalone(?) or
                // have e.g. kicker as mainwindow
                if (mainclients.isEmpty())
                    show = true;
                for (auto it2 = mainclients.constBegin();
                        it2 != mainclients.constEnd();
                        ++it2) {
                    if ((*it2)->isSpecialWindow())
                        show = true;
                }
                if (!show)
                    to_hide.append(c);
            }
            if (show)
                to_show.append(c);
        }
    } // First show new ones, then hide
    for (int i = to_show.size() - 1;
            i >= 0;
            --i)  // From topmost
        // TODO: Since this is in stacking order, the order of taskbar entries changes :(
        to_show.at(i)->hideClient(false);
    if (also_hide) {
        for (auto it = to_hide.constBegin();
                it != to_hide.constEnd();
                ++it)  // From bottommost
            (*it)->hideClient(true);
        updateToolWindowsTimer.stop();
    } else // setActiveClient() is after called with NULL client, quickly followed
        // by setting a new client, which would result in flickering
        resetUpdateToolWindowsTimer();
}


void Workspace::resetUpdateToolWindowsTimer()
{
    updateToolWindowsTimer.start(200);
}

void Workspace::slotUpdateToolWindows()
{
    updateToolWindows(true);
}

void Workspace::slotReloadConfig()
{
    reconfigure();
}

void Workspace::reconfigure()
{
    reconfigureTimer.start(200);
}

/**
 * Reread settings
 */

void Workspace::slotReconfigure()
{
    qCDebug(KWIN_CORE) << "Workspace::slotReconfigure()";
    reconfigureTimer.stop();

    bool borderlessMaximizedWindows = options->borderlessMaximizedWindows();

    kwinApp()->config()->reparseConfiguration();
    options->updateSettings();

    emit configChanged();
    m_userActionsMenu->discard();
    updateToolWindows(true);

    RuleBook::self()->load();
    for (AbstractClient *client : m_allClients) {
        if (client->supportsWindowRules()) {
            client->evaluateWindowRules();
            RuleBook::self()->discardUsed(client, false);
        }
    }

    if (borderlessMaximizedWindows != options->borderlessMaximizedWindows() &&
            !options->borderlessMaximizedWindows()) {
        // in case borderless maximized windows option changed and new option
        // is to have borders, we need to unset the borders for all maximized windows
        for (auto it = m_allClients.begin();
                it != m_allClients.end();
                ++it) {
            if ((*it)->maximizeMode() == MaximizeFull)
                (*it)->checkNoBorder();
        }
    }
}

void Workspace::slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop)
{
    closeActivePopup();
    ++block_focus;
    StackingUpdatesBlocker blocker(this);
    updateClientVisibilityOnDesktopChange(newDesktop);
    // Restore the focus on this desktop
    --block_focus;

    activateClientOnNewDesktop(newDesktop);
    emit currentDesktopChanged(oldDesktop, movingClient);
}

void Workspace::updateClientVisibilityOnDesktopChange(uint newDesktop)
{
    for (auto it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        X11Client *c = qobject_cast<X11Client *>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnDesktop(newDesktop) && c != movingClient && c->isOnCurrentActivity()) {
            (c)->updateVisibility();
        }
    }
    // Now propagate the change, after hiding, before showing
    if (rootInfo()) {
        rootInfo()->setCurrentDesktop(VirtualDesktopManager::self()->current());
    }

    if (movingClient && !movingClient->isOnDesktop(newDesktop)) {
        movingClient->setDesktop(newDesktop);
    }

    for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
        X11Client *c = qobject_cast<X11Client *>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnDesktop(newDesktop) && c->isOnCurrentActivity())
            c->updateVisibility();
    }
    if (showingDesktop())   // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);
}

void Workspace::activateClientOnNewDesktop(uint desktop)
{
    AbstractClient* c = nullptr;
    if (options->focusPolicyIsReasonable()) {
        c = findClientToActivateOnDesktop(desktop);
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop())
        c = active_client;

    if (!c)
        c = findDesktop(true, desktop);

    if (c != active_client)
        setActiveClient(nullptr);

    if (c)
        requestFocus(c);
    else
        focusToNull();
}

AbstractClient *Workspace::findClientToActivateOnDesktop(uint desktop)
{
    if (movingClient != nullptr && active_client == movingClient &&
        FocusChain::self()->contains(active_client, desktop) &&
        active_client->isShown(true) && active_client->isOnCurrentDesktop()) {
        // A requestFocus call will fail, as the client is already active
        return active_client;
    }
    // from actiavtion.cpp
    if (options->isNextFocusPrefersMouse()) {
        auto it = stackingOrder().constEnd();
        while (it != stackingOrder().constBegin()) {
            X11Client *client = qobject_cast<X11Client *>(*(--it));
            if (!client) {
                continue;
            }

            if (!(client->isShown(false) && client->isOnDesktop(desktop) &&
                client->isOnCurrentActivity() && client->isOnActiveScreen()))
                continue;

            if (client->frameGeometry().contains(Cursors::self()->mouse()->pos())) {
                if (!client->isDesktop())
                    return client;
            break; // unconditional break  - we do not pass the focus to some client below an unusable one
            }
        }
    }
    return FocusChain::self()->getForActivation(desktop);
}

/**
 * Updates the current activity when it changes
 * do *not* call this directly; it does not set the activity.
 *
 * Shows/Hides windows according to the stacking order
 */

void Workspace::updateCurrentActivity(const QString &new_activity)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    //closeActivePopup();
    ++block_focus;
    // TODO: Q_ASSERT( block_stacking_updates == 0 ); // Make sure stacking_order is up to date
    StackingUpdatesBlocker blocker(this);

    // Optimized Desktop switching: unmapping done from back to front
    // mapping done from front to back => less exposure events
    //Notify::raise((Notify::Event) (Notify::DesktopChange+new_desktop));

    for (auto it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        X11Client *c = qobject_cast<X11Client *>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnActivity(new_activity) && c != movingClient && c->isOnCurrentDesktop()) {
            c->updateVisibility();
        }
    }

    // Now propagate the change, after hiding, before showing
    //rootInfo->setCurrentDesktop( currentDesktop() );

    /* TODO someday enable dragging windows to other activities
    if ( movingClient && !movingClient->isOnDesktop( new_desktop ))
        {
        movingClient->setDesktop( new_desktop );
        */

    for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
        X11Client *c = qobject_cast<X11Client *>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnActivity(new_activity))
            c->updateVisibility();
    }

    //FIXME not sure if I should do this either
    if (showingDesktop())   // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);

    // Restore the focus on this desktop
    --block_focus;
    AbstractClient* c = nullptr;

    //FIXME below here is a lot of focuschain stuff, probably all wrong now
    if (options->focusPolicyIsReasonable()) {
        // Search in focus chain
        c = FocusChain::self()->getForActivation(VirtualDesktopManager::self()->current());
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop() && active_client->isOnCurrentActivity())
        c = active_client;

    if (!c)
        c = findDesktop(true, VirtualDesktopManager::self()->current());

    if (c != active_client)
        setActiveClient(nullptr);

    if (c)
        requestFocus(c);
    else
        focusToNull();

    // Not for the very first time, only if something changed and there are more than 1 desktops

    //if ( effects != NULL && old_desktop != 0 && old_desktop != new_desktop )
    //    static_cast<EffectsHandlerImpl*>( effects )->desktopChanged( old_desktop );
    if (compositing() && m_compositor)
        m_compositor->addRepaintFull();
#else
    Q_UNUSED(new_activity)
#endif
}

void Workspace::slotDesktopCountChanged(uint previousCount, uint newCount)
{
    Q_UNUSED(previousCount)
    Placement::self()->reinitCascading(0);

    resetClientAreas(newCount);
}

void Workspace::resetClientAreas(uint desktopCount)
{
    // Make it +1, so that it can be accessed as [1..numberofdesktops]
    workarea.clear();
    workarea.resize(desktopCount + 1);
    restrictedmovearea.clear();
    restrictedmovearea.resize(desktopCount + 1);
    screenarea.clear();

    updateClientArea(true);
}

void Workspace::selectWmInputEventMask()
{
    uint32_t presentMask = 0;
    Xcb::WindowAttributes attr(rootWindow());
    if (!attr.isNull()) {
        presentMask = attr->your_event_mask;
    }

    Xcb::selectInput(rootWindow(),
                     presentMask |
                     XCB_EVENT_MASK_KEY_PRESS |
                     XCB_EVENT_MASK_PROPERTY_CHANGE |
                     XCB_EVENT_MASK_COLOR_MAP_CHANGE |
                     XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                     XCB_EVENT_MASK_FOCUS_CHANGE | // For NotifyDetailNone
                     XCB_EVENT_MASK_EXPOSURE
    );
}

/**
 * Sends client \a c to desktop \a desk.
 *
 * Takes care of transients as well.
 */
void Workspace::sendClientToDesktop(AbstractClient* c, int desk, bool dont_activate)
{
    if ((desk < 1 && desk != NET::OnAllDesktops) || desk > static_cast<int>(VirtualDesktopManager::self()->count()))
        return;
    int old_desktop = c->desktop();
    bool was_on_desktop = c->isOnDesktop(desk) || c->isOnAllDesktops();
    c->setDesktop(desk);
    if (c->desktop() != desk)   // No change or desktop forced
        return;
    desk = c->desktop(); // Client did range checking

    if (c->isOnDesktop(VirtualDesktopManager::self()->current())) {
        if (c->wantsTabFocus() && options->focusPolicyIsReasonable() &&
                !was_on_desktop && // for stickyness changes
                !dont_activate)
            requestFocus(c);
        else
            restackClientUnderActive(c);
    } else
        raiseClient(c);

    c->checkWorkspacePosition( QRect(), old_desktop );

    auto transients_stacking_order = ensureStackingOrder(c->transients());
    for (auto it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        sendClientToDesktop(*it, desk, dont_activate);
    updateClientArea();
}

/**
 * checks whether the X Window with the input focus is on our X11 screen
 * if the window cannot be determined or inspected, resturn depends on whether there's actually
 * more than one screen
 *
 * this is NOT in any way related to XRandR multiscreen
 *
 */
extern bool is_multihead; // main.cpp
bool Workspace::isOnCurrentHead()
{
    if (!is_multihead) {
        return true;
    }

    Xcb::CurrentInput currentInput;
    if (currentInput.window() == XCB_WINDOW_NONE) {
        return !is_multihead;
    }

    Xcb::WindowGeometry geometry(currentInput.window());
    if (geometry.isNull()) { // should not happen
        return !is_multihead;
    }

    return rootWindow() == geometry->root;
}

void Workspace::sendClientToScreen(AbstractClient* c, int screen)
{
    c->sendToScreen(screen);
}

/**
 * Delayed focus functions
 */
void Workspace::delayFocus()
{
    requestFocus(delayfocus_client);
    cancelDelayFocus();
}

void Workspace::requestDelayFocus(AbstractClient* c)
{
    delayfocus_client = c;
    delete delayFocusTimer;
    delayFocusTimer = new QTimer(this);
    connect(delayFocusTimer, SIGNAL(timeout()), this, SLOT(delayFocus()));
    delayFocusTimer->setSingleShot(true);
    delayFocusTimer->start(options->delayFocusInterval());
}

void Workspace::cancelDelayFocus()
{
    delete delayFocusTimer;
    delayFocusTimer = nullptr;
}

bool Workspace::checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data)
{
    return m_startup->checkStartup(w, id, data) == KStartupInfo::Match;
}

/**
 * Puts the focus on a dummy window
 * Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull()
{
    if (m_nullFocus) {
        m_nullFocus->focus();
    }
}

void Workspace::setShowingDesktop(bool showing)
{
    const bool changed = showing != showing_desktop;
    if (rootInfo() && changed) {
        rootInfo()->setShowingDesktop(showing);
    }
    showing_desktop = showing;

    AbstractClient *topDesk = nullptr;

    { // for the blocker RAII
    StackingUpdatesBlocker blocker(this); // updateLayer & lowerClient would invalidate stacking_order
    for (int i = stacking_order.count() - 1; i > -1; --i) {
        AbstractClient *c = qobject_cast<AbstractClient*>(stacking_order.at(i));
        if (c && c->isOnCurrentDesktop()) {
            if (c->isDock()) {
                c->updateLayer();
            } else if (c->isDesktop() && c->isShown(true)) {
                c->updateLayer();
                lowerClient(c);
                if (!topDesk)
                    topDesk = c;
                if (auto group = c->group()) {
                    foreach (X11Client *cm, group->members()) {
                        cm->updateLayer();
                    }
                }
            }
        }
    }
    } // ~StackingUpdatesBlocker

    if (showing_desktop && topDesk) {
        requestFocus(topDesk);
    } else if (!showing_desktop && changed) {
        const auto client = FocusChain::self()->getForActivation(VirtualDesktopManager::self()->current());
        if (client) {
            activateClient(client);
        }
    }
    if (changed)
        emit showingDesktopChanged(showing);
}

void Workspace::disableGlobalShortcutsForClient(bool disable)
{
    if (global_shortcuts_disabled_for_client == disable)
        return;
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/kglobalaccel"),
                                                          QStringLiteral("org.kde.KGlobalAccel"),
                                                          QStringLiteral("blockGlobalShortcuts"));
    message.setArguments(QList<QVariant>() << disable);
    QDBusConnection::sessionBus().asyncCall(message);

    global_shortcuts_disabled_for_client = disable;
    // Update also Meta+LMB actions etc.
    for (auto it = clients.constBegin();
            it != clients.constEnd();
            ++it)
        (*it)->updateMouseGrab();
}

QString Workspace::supportInformation() const
{
    QString support;
    const QString yes = QStringLiteral("yes\n");
    const QString no = QStringLiteral("no\n");

    support.append(ki18nc("Introductory text shown in the support information.",
        "KWin Support Information:\n"
        "The following information should be used when requesting support on e.g. https://forum.kde.org.\n"
        "It provides information about the currently running instance, which options are used,\n"
        "what OpenGL driver and which effects are running.\n"
        "Please post the information provided underneath this introductory text to a paste bin service\n"
        "like https://paste.kde.org instead of pasting into support threads.\n").toString());
    support.append(QStringLiteral("\n==========================\n\n"));
    // all following strings are intended for support. They need to be pasted to e.g forums.kde.org
    // it is expected that the support will happen in English language or that the people providing
    // help understand English. Because of that all texts are not translated
    support.append(QStringLiteral("Version\n"));
    support.append(QStringLiteral("=======\n"));
    support.append(QStringLiteral("KWin version: "));
    support.append(QStringLiteral(KWIN_VERSION_STRING));
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt Version: "));
    support.append(QString::fromUtf8(qVersion()));
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt compile version: %1\n").arg(QStringLiteral(QT_VERSION_STR)));
    support.append(QStringLiteral("XCB compile version: %1\n\n").arg(QStringLiteral(XCB_VERSION_STRING)));
    support.append(QStringLiteral("Operation Mode: "));
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        support.append(QStringLiteral("X11 only"));
        break;
    case Application::OperationModeWaylandOnly:
        support.append(QStringLiteral("Wayland Only"));
        break;
    case Application::OperationModeXwayland:
        support.append(QStringLiteral("Xwayland"));
        break;
    }
    support.append(QStringLiteral("\n\n"));

    support.append(QStringLiteral("Build Options\n"));
    support.append(QStringLiteral("=============\n"));

    support.append(QStringLiteral("KWIN_BUILD_DECORATIONS: "));
#ifdef KWIN_BUILD_DECORATIONS
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("KWIN_BUILD_TABBOX: "));
#ifdef KWIN_BUILD_TABBOX
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("KWIN_BUILD_ACTIVITIES: "));
#ifdef KWIN_BUILD_ACTIVITIES
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_DRM: "));
#if HAVE_DRM
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_GBM: "));
#if HAVE_GBM
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_EGL_STREAMS: "));
#if HAVE_EGL_STREAMS
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_X11_XCB: "));
#if HAVE_X11_XCB
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_EPOXY_GLX: "));
#if HAVE_EPOXY_GLX
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_WAYLAND_EGL: "));
#if HAVE_WAYLAND_EGL
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("\n"));

    if (auto c = kwinApp()->x11Connection()) {
        support.append(QStringLiteral("X11\n"));
        support.append(QStringLiteral("===\n"));
        auto x11setup = xcb_get_setup(c);
        support.append(QStringLiteral("Vendor: %1\n").arg(QString::fromUtf8(QByteArray::fromRawData(xcb_setup_vendor(x11setup), xcb_setup_vendor_length(x11setup)))));
        support.append(QStringLiteral("Vendor Release: %1\n").arg(x11setup->release_number));
        support.append(QStringLiteral("Protocol Version/Revision: %1/%2\n").arg(x11setup->protocol_major_version).arg(x11setup->protocol_minor_version));
        const auto extensions = Xcb::Extensions::self()->extensions();
        for (const auto &e : extensions) {
            support.append(QStringLiteral("%1: %2; Version: 0x%3\n").arg(QString::fromUtf8(e.name))
                                                                    .arg(e.present ? yes.trimmed() : no.trimmed())
                                                                    .arg(QString::number(e.version, 16)));
        }
        support.append(QStringLiteral("\n"));
    }

    if (auto bridge = Decoration::DecorationBridge::self()) {
        support.append(QStringLiteral("Decoration\n"));
        support.append(QStringLiteral("==========\n"));
        support.append(bridge->supportInformation());
        support.append(QStringLiteral("\n"));
    }
    support.append(QStringLiteral("Platform\n"));
    support.append(QStringLiteral("==========\n"));
    support.append(kwinApp()->platform()->supportInformation());
    support.append(QStringLiteral("\n"));

    support.append(QStringLiteral("Options\n"));
    support.append(QStringLiteral("=======\n"));
    const QMetaObject *metaOptions = options->metaObject();
    auto printProperty = [] (const QVariant &variant) {
        if (variant.type() == QVariant::Size) {
            const QSize &s = variant.toSize();
            return QStringLiteral("%1x%2").arg(QString::number(s.width())).arg(QString::number(s.height()));
        }
        if (QLatin1String(variant.typeName()) == QLatin1String("KWin::OpenGLPlatformInterface") ||
                QLatin1String(variant.typeName()) == QLatin1String("KWin::Options::WindowOperation")) {
            return QString::number(variant.toInt());
        }
        return variant.toString();
    };
    for (int i=0; i<metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name()).arg(printProperty(options->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreen Edges\n"));
    support.append(QStringLiteral(  "============\n"));
    const QMetaObject *metaScreenEdges = ScreenEdges::self()->metaObject();
    for (int i=0; i<metaScreenEdges->propertyCount(); ++i) {
        const QMetaProperty property = metaScreenEdges->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name()).arg(printProperty(ScreenEdges::self()->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreens\n"));
    support.append(QStringLiteral(  "=======\n"));
    support.append(QStringLiteral("Multi-Head: "));
    if (is_multihead) {
        support.append(QStringLiteral("yes\n"));
        support.append(QStringLiteral("Head: %1\n").arg(screen_number));
    } else {
        support.append(QStringLiteral("no\n"));
    }
    support.append(QStringLiteral("Active screen follows mouse: "));
    if (screens()->isCurrentFollowsMouse())
        support.append(QStringLiteral(" yes\n"));
    else
        support.append(QStringLiteral(" no\n"));
    support.append(QStringLiteral("Number of Screens: %1\n\n").arg(screens()->count()));
    for (int i=0; i<screens()->count(); ++i) {
        const QRect geo = screens()->geometry(i);
        support.append(QStringLiteral("Screen %1:\n").arg(i));
        support.append(QStringLiteral("---------\n"));
        support.append(QStringLiteral("Name: %1\n").arg(screens()->name(i)));
        support.append(QStringLiteral("Geometry: %1,%2,%3x%4\n")
                              .arg(geo.x())
                              .arg(geo.y())
                              .arg(geo.width())
                              .arg(geo.height()));
        support.append(QStringLiteral("Scale: %1\n").arg(screens()->scale(i)));
        support.append(QStringLiteral("Refresh Rate: %1\n\n").arg(screens()->refreshRate(i)));
    }
    support.append(QStringLiteral("\nCompositing\n"));
    support.append(QStringLiteral(  "===========\n"));
    if (effects) {
        support.append(QStringLiteral("Compositing is active\n"));
        switch (effects->compositingType()) {
        case OpenGL2Compositing:
        case OpenGLCompositing: {
            GLPlatform *platform = GLPlatform::instance();
            if (platform->isGLES()) {
                support.append(QStringLiteral("Compositing Type: OpenGL ES 2.0\n"));
            } else {
                support.append(QStringLiteral("Compositing Type: OpenGL\n"));
            }
            support.append(QStringLiteral("OpenGL vendor string: ") +   QString::fromUtf8(platform->glVendorString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL renderer string: ") + QString::fromUtf8(platform->glRendererString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL version string: ") +  QString::fromUtf8(platform->glVersionString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL platform interface: "));
            switch (platform->platformInterface()) {
            case GlxPlatformInterface:
                support.append(QStringLiteral("GLX"));
                break;
            case EglPlatformInterface:
                support.append(QStringLiteral("EGL"));
                break;
            default:
                support.append(QStringLiteral("UNKNOWN"));
            }
            support.append(QStringLiteral("\n"));

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL))
                support.append(QStringLiteral("OpenGL shading language version string: ") + QString::fromUtf8(platform->glShadingLanguageVersionString()) + QStringLiteral("\n"));

            support.append(QStringLiteral("Driver: ") + GLPlatform::driverToString(platform->driver()) + QStringLiteral("\n"));
            if (!platform->isMesaDriver())
                support.append(QStringLiteral("Driver version: ") + GLPlatform::versionToString(platform->driverVersion()) + QStringLiteral("\n"));

            support.append(QStringLiteral("GPU class: ") + GLPlatform::chipClassToString(platform->chipClass()) + QStringLiteral("\n"));

            support.append(QStringLiteral("OpenGL version: ") + GLPlatform::versionToString(platform->glVersion()) + QStringLiteral("\n"));

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL))
                support.append(QStringLiteral("GLSL version: ") + GLPlatform::versionToString(platform->glslVersion()) + QStringLiteral("\n"));

            if (platform->isMesaDriver())
                support.append(QStringLiteral("Mesa version: ") + GLPlatform::versionToString(platform->mesaVersion()) + QStringLiteral("\n"));
            if (platform->serverVersion() > 0)
                support.append(QStringLiteral("X server version: ") + GLPlatform::versionToString(platform->serverVersion()) + QStringLiteral("\n"));
            if (platform->kernelVersion() > 0)
                support.append(QStringLiteral("Linux kernel version: ") + GLPlatform::versionToString(platform->kernelVersion()) + QStringLiteral("\n"));

            support.append(QStringLiteral("Direct rendering: "));
            support.append(QStringLiteral("Requires strict binding: "));
            if (!platform->isLooseBinding()) {
                support.append(QStringLiteral("yes\n"));
            } else {
                support.append(QStringLiteral("no\n"));
            }
            support.append(QStringLiteral("GLSL shaders: "));
            if (platform->supports(GLSL)) {
                if (platform->supports(LimitedGLSL)) {
                    support.append(QStringLiteral(" limited\n"));
                } else {
                    support.append(QStringLiteral(" yes\n"));
                }
            } else {
                support.append(QStringLiteral(" no\n"));
            }
            support.append(QStringLiteral("Texture NPOT support: "));
            if (platform->supports(TextureNPOT)) {
                if (platform->supports(LimitedNPOT)) {
                    support.append(QStringLiteral(" limited\n"));
                } else {
                    support.append(QStringLiteral(" yes\n"));
                }
            } else {
                support.append(QStringLiteral(" no\n"));
            }
            support.append(QStringLiteral("Virtual Machine: "));
            if (platform->isVirtualMachine()) {
                support.append(QStringLiteral(" yes\n"));
            } else {
                support.append(QStringLiteral(" no\n"));
            }

            support.append(QStringLiteral("OpenGL 2 Shaders are used\n"));
            support.append(QStringLiteral("Painting blocks for vertical retrace: "));
            if (m_compositor->scene()->blocksForRetrace())
                support.append(QStringLiteral(" yes\n"));
            else
                support.append(QStringLiteral(" no\n"));
            break;
        }
        case XRenderCompositing:
            support.append(QStringLiteral("Compositing Type: XRender\n"));
            break;
        case QPainterCompositing:
            support.append("Compositing Type: QPainter\n");
            break;
        case NoCompositing:
        default:
            support.append(QStringLiteral("Something is really broken, neither OpenGL nor XRender is used"));
        }
        support.append(QStringLiteral("\nLoaded Effects:\n"));
        support.append(QStringLiteral(  "---------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nCurrently Active Effects:\n"));
        support.append(QStringLiteral(  "-------------------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->activeEffects()) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nEffect Settings:\n"));
        support.append(QStringLiteral(  "----------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(static_cast<EffectsHandlerImpl*>(effects)->supportInformation(effect));
            support.append(QStringLiteral("\n"));
        }
    } else {
        support.append(QStringLiteral("Compositing is not active\n"));
    }
    return support;
}

X11Client *Workspace::findClient(std::function<bool (const X11Client *)> func) const
{
    if (X11Client *ret = Toplevel::findInList(clients, func)) {
        return ret;
    }
    return nullptr;
}

AbstractClient *Workspace::findAbstractClient(std::function<bool (const AbstractClient*)> func) const
{
    if (AbstractClient *ret = Toplevel::findInList(m_allClients, func)) {
        return ret;
    }
    if (InternalClient *ret = Toplevel::findInList(m_internalClients, func)) {
        return ret;
    }
    return nullptr;
}

Unmanaged *Workspace::findUnmanaged(std::function<bool (const Unmanaged*)> func) const
{
    return Toplevel::findInList(m_unmanaged, func);
}

Unmanaged *Workspace::findUnmanaged(xcb_window_t w) const
{
    return findUnmanaged([w](const Unmanaged *u) {
        return u->window() == w;
    });
}

X11Client *Workspace::findClient(Predicate predicate, xcb_window_t w) const
{
    switch (predicate) {
    case Predicate::WindowMatch:
        return findClient([w](const X11Client *c) {
            return c->window() == w;
        });
    case Predicate::WrapperIdMatch:
        return findClient([w](const X11Client *c) {
            return c->wrapperId() == w;
        });
    case Predicate::FrameIdMatch:
        return findClient([w](const X11Client *c) {
            return c->frameId() == w;
        });
    case Predicate::InputIdMatch:
        return findClient([w](const X11Client *c) {
            return c->inputId() == w;
        });
    }
    return nullptr;
}

Toplevel *Workspace::findToplevel(std::function<bool (const Toplevel*)> func) const
{
    if (auto *ret = Toplevel::findInList(m_allClients, func)) {
        return ret;
    }
    if (Unmanaged *ret = Toplevel::findInList(m_unmanaged, func)) {
        return ret;
    }
    if (InternalClient *ret = Toplevel::findInList(m_internalClients, func)) {
        return ret;
    }
    return nullptr;
}

Toplevel *Workspace::findToplevel(const QUuid &internalId) const
{
    return findToplevel([internalId] (const KWin::Toplevel* l) -> bool {
        return internalId == l->internalId();
    });
}

void Workspace::forEachToplevel(std::function<void (Toplevel *)> func)
{
    std::for_each(m_allClients.constBegin(), m_allClients.constEnd(), func);
    std::for_each(deleted.constBegin(), deleted.constEnd(), func);
    std::for_each(m_unmanaged.constBegin(), m_unmanaged.constEnd(), func);
    std::for_each(m_internalClients.constBegin(), m_internalClients.constEnd(), func);
}

bool Workspace::hasClient(const AbstractClient *c)
{
    if (auto cc = dynamic_cast<const X11Client *>(c)) {
        return hasClient(cc);
    } else {
        return findAbstractClient([c](const AbstractClient *test) {
            return test == c;
        }) != nullptr;
    }
    return false;
}

void Workspace::forEachAbstractClient(std::function< void (AbstractClient*) > func)
{
    std::for_each(m_allClients.constBegin(), m_allClients.constEnd(), func);
    std::for_each(m_internalClients.constBegin(), m_internalClients.constEnd(), func);
}

Toplevel *Workspace::findInternal(QWindow *w) const
{
    if (!w) {
        return nullptr;
    }
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        return findUnmanaged(w->winId());
    }
    for (InternalClient *client : m_internalClients) {
        if (client->internalWindow() == w) {
            return client;
        }
    }
    return nullptr;
}

bool Workspace::compositing() const
{
    return m_compositor && m_compositor->scene();
}

void Workspace::markXStackingOrderAsDirty()
{
    m_xStackingDirty = true;
    if (kwinApp()->x11Connection() && !kwinApp()->isClosingX11Connection()) {
        m_xStackingQueryTree.reset(new Xcb::Tree(kwinApp()->x11RootWindow()));
    }
}

void Workspace::setWasUserInteraction()
{
    if (was_user_interaction) {
        return;
    }
    was_user_interaction = true;
    // might be called from within the filter, so delay till we now the filter returned
    QTimer::singleShot(0, this,
        [this] {
            m_wasUserInteractionFilter.reset();
        }
    );
}

void Workspace::updateTabbox()
{
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox->isDisplayed()) {
        tabBox->reset(true);
    }
#endif
}

void Workspace::addInternalClient(InternalClient *client)
{
    m_internalClients.append(client);

    setupClientConnections(client);
    client->updateLayer();

    if (client->isPlaceable()) {
        const QRect area = clientArea(PlacementArea, screens()->current(), client->desktop());
        client->placeIn(area);
    }

    markXStackingOrderAsDirty();
    updateStackingOrder(true);
    updateClientArea();

    emit internalClientAdded(client);
}

void Workspace::removeInternalClient(InternalClient *client)
{
    m_internalClients.removeOne(client);

    markXStackingOrderAsDirty();
    updateStackingOrder(true);
    updateClientArea();

    emit internalClientRemoved(client);
}

Group* Workspace::findGroup(xcb_window_t leader) const
{
    Q_ASSERT(leader != XCB_WINDOW_NONE);
    for (auto it = groups.constBegin();
            it != groups.constEnd();
            ++it)
        if ((*it)->leader() == leader)
            return *it;
    return nullptr;
}

// Client is group transient, but has no group set. Try to find
// group with windows with the same client leader.
Group* Workspace::findClientLeaderGroup(const X11Client *c) const
{
    Group* ret = nullptr;
    for (auto it = clients.constBegin();
            it != clients.constEnd();
            ++it) {
        if (*it == c)
            continue;
        if ((*it)->wmClientLeader() == c->wmClientLeader()) {
            if (ret == nullptr || ret == (*it)->group())
                ret = (*it)->group();
            else {
                // There are already two groups with the same client leader.
                // This most probably means the app uses group transients without
                // setting group for its windows. Merging the two groups is a bad
                // hack, but there's no really good solution for this case.
                QList<X11Client *> old_group = (*it)->group()->members();
                // old_group autodeletes when being empty
                for (int pos = 0;
                        pos < old_group.count();
                        ++pos) {
                    X11Client *tmp = old_group[ pos ];
                    if (tmp != c)
                        tmp->changeClientLeaderGroup(ret);
                }
            }
        }
    }
    return ret;
}

void Workspace::updateMinimizedOfTransients(AbstractClient* c)
{
    // if mainwindow is minimized or shaded, minimize transients too
    if (c->isMinimized()) {
        for (auto it = c->transients().constBegin();
                it != c->transients().constEnd();
                ++it) {
            if ((*it)->isModal())
                continue; // there's no reason to hide modal dialogs with the main client
            // but to keep them to eg. watch progress or whatever
            if (!(*it)->isMinimized()) {
                (*it)->minimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (c->isModal()) { // if a modal dialog is minimized, minimize its mainwindow too
            foreach (AbstractClient * c2, c->mainClients())
            c2->minimize();
        }
    } else {
        // else unmiminize the transients
        for (auto it = c->transients().constBegin();
                it != c->transients().constEnd();
                ++it) {
            if ((*it)->isMinimized()) {
                (*it)->unminimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (c->isModal()) {
            foreach (AbstractClient * c2, c->mainClients())
            c2->unminimize();
        }
    }
}


/**
 * Sets the client \a c's transient windows' on_all_desktops property to \a on_all_desktops.
 */
void Workspace::updateOnAllDesktopsOfTransients(AbstractClient* c)
{
    for (auto it = c->transients().constBegin();
            it != c->transients().constEnd();
            ++it) {
        if ((*it)->isOnAllDesktops() != c->isOnAllDesktops())
            (*it)->setOnAllDesktops(c->isOnAllDesktops());
    }
}

// A new window has been mapped. Check if it's not a mainwindow for some already existing transient window.
void Workspace::checkTransients(xcb_window_t w)
{
    for (auto it = clients.constBegin();
            it != clients.constEnd();
            ++it)
        (*it)->checkTransient(w);
}

/**
 * Resizes the workspace after an XRANDR screen size change
 */
void Workspace::desktopResized()
{
    QRect geom = screens()->geometry();
    if (rootInfo()) {
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
        rootInfo()->setDesktopGeometry(desktop_geometry);
    }

    updateClientArea();
    saveOldScreenSizes(); // after updateClientArea(), so that one still uses the previous one

    // TODO: emit a signal instead and remove the deep function calls into edges and effects
    ScreenEdges::self()->recreateEdges();

    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->desktopResized(geom.size());
    }
}

void Workspace::saveOldScreenSizes()
{
    olddisplaysize = screens()->displaySize();
    oldscreensizes.clear();
    for( int i = 0;
         i < screens()->count();
         ++i )
        oldscreensizes.append( screens()->geometry( i ));
}

/**
 * Whether or not the window has a strut that expands through the invisible area of
 * an xinerama setup where the monitors are not the same resolution.
 */
static bool hasOffscreenXineramaStrut(AbstractClient *client)
{
    // Get strut as a QRegion
    QRegion region;
    region += client->strutRect(StrutAreaTop);
    region += client->strutRect(StrutAreaRight);
    region += client->strutRect(StrutAreaBottom);
    region += client->strutRect(StrutAreaLeft);

    // Remove all visible areas so that only the invisible remain
    for (int i = 0; i < screens()->count(); i ++) {
        region -= screens()->geometry(i);
    }

    // If there's anything left then we have an offscreen strut
    return !region.isEmpty();
}

QRect Workspace::adjustClientArea(AbstractClient *client, const QRect &area) const
{
    QRect adjustedArea = area;

    QRect strutLeft = client->strutRect(StrutAreaLeft);
    QRect strutRight = client->strutRect(StrutAreaRight);
    QRect strutTop = client->strutRect(StrutAreaTop);
    QRect strutBottom = client->strutRect(StrutAreaBottom);

    QRect screenArea = clientArea(ScreenArea, client);
    // HACK: workarea handling is not xinerama aware, so if this strut
    // reserves place at a xinerama edge that's inside the virtual screen,
    // ignore the strut for workspace setting.
    if (area == QRect(QPoint(0, 0), screens()->displaySize())) {
        if (strutLeft.left() < screenArea.left()) {
            strutLeft = QRect();
        }
        if (strutRight.right() > screenArea.right()) {
            strutRight = QRect();
        }
        if (strutTop.top() < screenArea.top()) {
            strutTop = QRect();
        }
        if (strutBottom.bottom() < screenArea.bottom()) {
            strutBottom = QRect();
        }
    }

    // Handle struts at xinerama edges that are inside the virtual screen.
    // They're given in virtual screen coordinates, make them affect only
    // their xinerama screen.
    strutLeft.setLeft(qMax(strutLeft.left(), screenArea.left()));
    strutRight.setRight(qMin(strutRight.right(), screenArea.right()));
    strutTop.setTop(qMax(strutTop.top(), screenArea.top()));
    strutBottom.setBottom(qMin(strutBottom.bottom(), screenArea.bottom()));

    if (strutLeft.intersects(area)) {
        adjustedArea.setLeft(strutLeft.right() + 1);
    }
    if (strutRight.intersects(area)) {
        adjustedArea.setRight(strutRight.left() - 1);
    }
    if (strutTop.intersects(area)) {
        adjustedArea.setTop(strutTop.bottom() + 1);
    }
    if (strutBottom.intersects(area)) {
        adjustedArea.setBottom(strutBottom.top() - 1);
    }

    return adjustedArea;
}

/**
 * Updates the current client areas according to the current clients.
 *
 * If the area changes or force is @c true, the new areas are propagated to the world.
 *
 * The client area is the area that is available for clients (that
 * which is not taken by windows like panels, the top-of-screen menu
 * etc).
 *
 * @see clientArea()
 */
void Workspace::updateClientArea(bool force)
{
    const Screens *s = Screens::self();
    int nscreens = s->count();
    const int numberOfDesktops = VirtualDesktopManager::self()->count();
    QVector< QRect > new_wareas(numberOfDesktops + 1);
    QVector< StrutRects > new_rmoveareas(numberOfDesktops + 1);
    QVector< QVector< QRect > > new_sareas(numberOfDesktops + 1);
    QVector< QRect > screens(nscreens);
    QRect desktopArea;
    for (int i = 0; i < nscreens; i++) {
        desktopArea |= s->geometry(i);
    }
    for (int iS = 0;
            iS < nscreens;
            iS ++) {
        screens [iS] = s->geometry(iS);
    }
    for (int i = 1;
            i <= numberOfDesktops;
            ++i) {
        new_wareas[ i ] = desktopArea;
        new_sareas[ i ].resize(nscreens);
        for (int iS = 0;
                iS < nscreens;
                iS ++)
            new_sareas[ i ][ iS ] = screens[ iS ];
    }
    for (AbstractClient *client : qAsConst(m_allClients)) {
        if (!client->hasStrut()) {
            continue;
        }
        QRect r = adjustClientArea(client, desktopArea);
        // sanity check that a strut doesn't exclude a complete screen geometry
        // this is a violation to EWMH, as KWin just ignores the strut
        for (int i = 0; i < Screens::self()->count(); i++) {
            if (!r.intersects(Screens::self()->geometry(i))) {
                qCDebug(KWIN_CORE) << "Adjusted client area would exclude a complete screen, ignore";
                r = desktopArea;
                break;
            }
        }
        StrutRects strutRegion = client->strutRects();
        const QRect clientsScreenRect = KWin::screens()->geometry(client->screen());
        for (auto strut = strutRegion.begin(); strut != strutRegion.end(); strut++) {
            *strut = StrutRect((*strut).intersected(clientsScreenRect), (*strut).area());
        }

        // Ignore offscreen xinerama struts. These interfere with the larger monitors on the setup
        // and should be ignored so that applications that use the work area to work out where
        // windows can go can use the entire visible area of the larger monitors.
        // This goes against the EWMH description of the work area but it is a toss up between
        // having unusable sections of the screen (Which can be quite large with newer monitors)
        // or having some content appear offscreen (Relatively rare compared to other).
        bool hasOffscreenStrut = hasOffscreenXineramaStrut(client);

        if (client->isOnAllDesktops()) {
            for (int i = 1;
                    i <= numberOfDesktops;
                    ++i) {
                if (!hasOffscreenStrut)
                    new_wareas[ i ] = new_wareas[ i ].intersected(r);
                new_rmoveareas[ i ] += strutRegion;
                for (int iS = 0;
                        iS < nscreens;
                        iS ++) {
                    const auto geo = new_sareas[ i ][ iS ].intersected(
                                                adjustClientArea(client, screens[ iS ]));
                    // ignore the geometry if it results in the screen getting removed completely
                    if (!geo.isEmpty()) {
                        new_sareas[ i ][ iS ] = geo;
                    }
                }
            }
        } else {
            if (!hasOffscreenStrut)
                new_wareas[client->desktop()] = new_wareas[client->desktop()].intersected(r);
            new_rmoveareas[client->desktop()] += strutRegion;
            for (int iS = 0;
                    iS < nscreens;
                    iS ++) {
//                            qDebug() << "adjusting new_sarea: " << screens[ iS ];
                const auto geo = new_sareas[client->desktop()][ iS ].intersected(
                      adjustClientArea(client, screens[ iS ]));
                // ignore the geometry if it results in the screen getting removed completely
                if (!geo.isEmpty()) {
                    new_sareas[client->desktop()][ iS ] = geo;
                }
            }
        }
    }
#if 0
    for (int i = 1;
            i <= numberOfDesktops();
            ++i) {
        for (int iS = 0;
                iS < nscreens;
                iS ++)
            qCDebug(KWIN_CORE) << "new_sarea: " << new_sareas[ i ][ iS ];
    }
#endif

    bool changed = force;

    if (screenarea.isEmpty())
        changed = true;

    for (int i = 1;
            !changed && i <= numberOfDesktops;
            ++i) {
        if (workarea[ i ] != new_wareas[ i ])
            changed = true;
        if (restrictedmovearea[ i ] != new_rmoveareas[ i ])
            changed = true;
        if (screenarea[ i ].size() != new_sareas[ i ].size())
            changed = true;
        for (int iS = 0;
                !changed && iS < nscreens;
                iS ++)
            if (new_sareas[ i ][ iS ] != screenarea [ i ][ iS ])
                changed = true;
    }

    if (changed) {
        workarea = new_wareas;
        oldrestrictedmovearea = restrictedmovearea;
        restrictedmovearea = new_rmoveareas;
        screenarea = new_sareas;
        if (rootInfo()) {
            NETRect r;
            for (int i = 1; i <= numberOfDesktops; i++) {
                r.pos.x = workarea[ i ].x();
                r.pos.y = workarea[ i ].y();
                r.size.width = workarea[ i ].width();
                r.size.height = workarea[ i ].height();
                rootInfo()->setWorkArea(i, r);
            }
        }

        for (auto it = m_allClients.constBegin();
                it != m_allClients.constEnd();
                ++it) {
            (*it)->checkWorkspacePosition();
        }

        oldrestrictedmovearea.clear(); // reset, no longer valid or needed
    }
}

void Workspace::updateClientArea()
{
    updateClientArea(false);
}


/**
 * Returns the area available for clients. This is the desktop
 * geometry minus windows on the dock. Placement algorithms should
 * refer to this rather than Screens::geometry.
 */
QRect Workspace::clientArea(clientAreaOption opt, int screen, int desktop) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    if (screen == -1)
        screen = screens()->current();
    const QSize displaySize = screens()->displaySize();

    QRect sarea, warea;

    if (is_multihead) {
        sarea = (!screenarea.isEmpty()
                   && screen < screenarea[ desktop ].size()) // screens may be missing during KWin initialization or screen config changes
                  ? screenarea[ desktop ][ screen_number ]
                  : screens()->geometry(screen_number);
        warea = workarea[ desktop ].isNull()
                ? screens()->geometry(screen_number)
                : workarea[ desktop ];
    } else {
        sarea = (!screenarea.isEmpty()
                && screen < screenarea[ desktop ].size()) // screens may be missing during KWin initialization or screen config changes
                ? screenarea[ desktop ][ screen ]
                : screens()->geometry(screen);
        warea = workarea[ desktop ].isNull()
                ? QRect(0, 0, displaySize.width(), displaySize.height())
                : workarea[ desktop ];
    }

    switch(opt) {
    case MaximizeArea:
    case PlacementArea:
            return sarea;
    case MaximizeFullArea:
    case FullScreenArea:
    case MovementArea:
    case ScreenArea:
        if (is_multihead)
            return screens()->geometry(screen_number);
        else
            return screens()->geometry(screen);
    case WorkArea:
        if (is_multihead)
            return sarea;
        else
            return warea;
    case FullArea:
        return QRect(0, 0, displaySize.width(), displaySize.height());
    }
    abort();
}


QRect Workspace::clientArea(clientAreaOption opt, const QPoint& p, int desktop) const
{
    return clientArea(opt, screens()->number(p), desktop);
}

QRect Workspace::clientArea(clientAreaOption opt, const AbstractClient* c) const
{
    return clientArea(opt, c->frameGeometry().center(), c->desktop());
}

QRegion Workspace::restrictedMoveArea(int desktop, StrutAreas areas) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    QRegion region;
    foreach (const StrutRect & rect, restrictedmovearea[desktop])
    if (areas & rect.area())
        region += rect;
    return region;
}

bool Workspace::inUpdateClientArea() const
{
    return !oldrestrictedmovearea.isEmpty();
}

QRegion Workspace::previousRestrictedMoveArea(int desktop, StrutAreas areas) const
{
    if (desktop == NETWinInfo::OnAllDesktops || desktop == 0)
        desktop = VirtualDesktopManager::self()->current();
    QRegion region;
    foreach (const StrutRect & rect, oldrestrictedmovearea.at(desktop))
    if (areas & rect.area())
        region += rect;
    return region;
}

QVector< QRect > Workspace::previousScreenSizes() const
{
    return oldscreensizes;
}

int Workspace::oldDisplayWidth() const
{
    return olddisplaysize.width();
}

int Workspace::oldDisplayHeight() const
{
    return olddisplaysize.height();
}

/**
 * Client \a c is moved around to position \a pos. This gives the
 * workspace the opportunity to interveniate and to implement
 * snap-to-windows functionality.
 *
 * The parameter \a snapAdjust is a multiplier used to calculate the
 * effective snap zones. When 1.0, it means that the snap zones will be
 * used without change.
 */
QPoint Workspace::adjustClientPosition(AbstractClient* c, QPoint pos, bool unrestricted, double snapAdjust)
{
    QSize borderSnapZone(options->borderSnapZone(), options->borderSnapZone());
    QRect maxRect;
    int guideMaximized = MaximizeRestore;
    if (c->maximizeMode() != MaximizeRestore) {
        maxRect = clientArea(MaximizeArea, pos + c->rect().center(), c->desktop());
        QRect geo = c->frameGeometry();
        if (c->maximizeMode() & MaximizeHorizontal && (geo.x() == maxRect.left() || geo.right() == maxRect.right())) {
            guideMaximized |= MaximizeHorizontal;
            borderSnapZone.setWidth(qMax(borderSnapZone.width() + 2, maxRect.width() / 16));
        }
        if (c->maximizeMode() & MaximizeVertical && (geo.y() == maxRect.top() || geo.bottom() == maxRect.bottom())) {
            guideMaximized |= MaximizeVertical;
            borderSnapZone.setHeight(qMax(borderSnapZone.height() + 2, maxRect.height() / 16));
        }
    }

    if (options->windowSnapZone() || !borderSnapZone.isNull() || options->centerSnapZone()) {

        const bool sOWO = options->isSnapOnlyWhenOverlapping();
        const int screen = screens()->number(pos + c->rect().center());
        if (maxRect.isNull())
            maxRect = clientArea(MovementArea, screen, c->desktop());
        const int xmin = maxRect.left();
        const int xmax = maxRect.right() + 1;             //desk size
        const int ymin = maxRect.top();
        const int ymax = maxRect.bottom() + 1;

        const int cx(pos.x());
        const int cy(pos.y());
        const int cw(c->width());
        const int ch(c->height());
        const int rx(cx + cw);
        const int ry(cy + ch);               //these don't change

        int nx(cx), ny(cy);                         //buffers
        int deltaX(xmax);
        int deltaY(ymax);   //minimum distance to other clients

        int lx, ly, lrx, lry; //coords and size for the comparison client, l

        // border snap
        const int snapX = borderSnapZone.width() * snapAdjust; //snap trigger
        const int snapY = borderSnapZone.height() * snapAdjust;
        if (snapX || snapY) {
            QRect geo = c->frameGeometry();
            QMargins frameMargins = c->frameMargins();

            // snap to titlebar / snap to window borders on inner screen edges
            AbstractClient::Position titlePos = c->titlebarPosition();
            if (frameMargins.left() && (titlePos == AbstractClient::PositionLeft || (c->maximizeMode() & MaximizeHorizontal) ||
                                        screens()->intersecting(geo.translated(maxRect.x() - (frameMargins.left() + geo.x()), 0)) > 1)) {
                frameMargins.setLeft(0);
            }
            if (frameMargins.right() && (titlePos == AbstractClient::PositionRight || (c->maximizeMode() & MaximizeHorizontal) ||
                                         screens()->intersecting(geo.translated(maxRect.right() + frameMargins.right() - geo.right(), 0)) > 1)) {
                frameMargins.setRight(0);
            }
            if (frameMargins.top() && (titlePos == AbstractClient::PositionTop || (c->maximizeMode() & MaximizeVertical) ||
                                       screens()->intersecting(geo.translated(0, maxRect.y() - (frameMargins.top() + geo.y()))) > 1)) {
                frameMargins.setTop(0);
            }
            if (frameMargins.bottom() && (titlePos == AbstractClient::PositionBottom || (c->maximizeMode() & MaximizeVertical) ||
                                          screens()->intersecting(geo.translated(0, maxRect.bottom() + frameMargins.bottom() - geo.bottom())) > 1)) {
                frameMargins.setBottom(0);
            }
            if ((sOWO ? (cx < xmin) : true) && (qAbs(xmin - cx) < snapX)) {
                deltaX = xmin - cx;
                nx = xmin - frameMargins.left();
            }
            if ((sOWO ? (rx > xmax) : true) && (qAbs(rx - xmax) < snapX) && (qAbs(xmax - rx) < deltaX)) {
                deltaX = rx - xmax;
                nx = xmax - cw + frameMargins.right();
            }

            if ((sOWO ? (cy < ymin) : true) && (qAbs(ymin - cy) < snapY)) {
                deltaY = ymin - cy;
                ny = ymin - frameMargins.top();
            }
            if ((sOWO ? (ry > ymax) : true) && (qAbs(ry - ymax) < snapY) && (qAbs(ymax - ry) < deltaY)) {
                deltaY = ry - ymax;
                ny = ymax - ch + frameMargins.bottom();
            }
        }

        // windows snap
        int snap = options->windowSnapZone() * snapAdjust;
        if (snap) {
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l) == c)
                    continue;
                if ((*l)->isMinimized())
                    continue; // is minimized
                if (!(*l)->isShown(false))
                    continue;
                if (!((*l)->isOnDesktop(c->desktop()) || c->isOnDesktop((*l)->desktop())))
                    continue; // wrong virtual desktop
                if (!(*l)->isOnCurrentActivity())
                    continue; // wrong activity
                if ((*l)->isDesktop() || (*l)->isSplash())
                    continue;

                lx = (*l)->x();
                ly = (*l)->y();
                lrx = lx + (*l)->width();
                lry = ly + (*l)->height();

                if (!(guideMaximized & MaximizeHorizontal) &&
                    (((cy <= lry) && (cy  >= ly)) || ((ry >= ly) && (ry  <= lry)) || ((cy <= ly) && (ry >= lry)))) {
                    if ((sOWO ? (cx < lrx) : true) && (qAbs(lrx - cx) < snap) && (qAbs(lrx - cx) < deltaX)) {
                        deltaX = qAbs(lrx - cx);
                        nx = lrx;
                    }
                    if ((sOWO ? (rx > lx) : true) && (qAbs(rx - lx) < snap) && (qAbs(rx - lx) < deltaX)) {
                        deltaX = qAbs(rx - lx);
                        nx = lx - cw;
                    }
                }

                if (!(guideMaximized & MaximizeVertical) &&
                    (((cx <= lrx) && (cx  >= lx)) || ((rx >= lx) && (rx  <= lrx)) || ((cx <= lx) && (rx >= lrx)))) {
                    if ((sOWO ? (cy < lry) : true) && (qAbs(lry - cy) < snap) && (qAbs(lry - cy) < deltaY)) {
                        deltaY = qAbs(lry - cy);
                        ny = lry;
                    }
                    //if ( (qAbs( ry-ly ) < snap) && (qAbs( ry - ly ) < deltaY ))
                    if ((sOWO ? (ry > ly) : true) && (qAbs(ry - ly) < snap) && (qAbs(ry - ly) < deltaY)) {
                        deltaY = qAbs(ry - ly);
                        ny = ly - ch;
                    }
                }

                // Corner snapping
                if (!(guideMaximized & MaximizeVertical) && (nx == lrx || nx + cw == lx)) {
                    if ((sOWO ? (ry > lry) : true) && (qAbs(lry - ry) < snap) && (qAbs(lry - ry) < deltaY)) {
                        deltaY = qAbs(lry - ry);
                        ny = lry - ch;
                    }
                    if ((sOWO ? (cy < ly) : true) && (qAbs(cy - ly) < snap) && (qAbs(cy - ly) < deltaY)) {
                        deltaY = qAbs(cy - ly);
                        ny = ly;
                    }
                }
                if (!(guideMaximized & MaximizeHorizontal) && (ny == lry || ny + ch == ly)) {
                    if ((sOWO ? (rx > lrx) : true) && (qAbs(lrx - rx) < snap) && (qAbs(lrx - rx) < deltaX)) {
                        deltaX = qAbs(lrx - rx);
                        nx = lrx - cw;
                    }
                    if ((sOWO ? (cx < lx) : true) && (qAbs(cx - lx) < snap) && (qAbs(cx - lx) < deltaX)) {
                        deltaX = qAbs(cx - lx);
                        nx = lx;
                    }
                }
            }
        }

        // center snap
        snap = options->centerSnapZone() * snapAdjust; //snap trigger
        if (snap) {
            int diffX = qAbs((xmin + xmax) / 2 - (cx + cw / 2));
            int diffY = qAbs((ymin + ymax) / 2 - (cy + ch / 2));
            if (diffX < snap && diffY < snap && diffX < deltaX && diffY < deltaY) {
                // Snap to center of screen
                nx = (xmin + xmax) / 2 - cw / 2;
                ny = (ymin + ymax) / 2 - ch / 2;
            } else if (options->borderSnapZone()) {
                // Enhance border snap
                if ((nx == xmin || nx == xmax - cw) && diffY < snap && diffY < deltaY) {
                    // Snap to vertical center on screen edge
                    ny = (ymin + ymax) / 2 - ch / 2;
                } else if (((unrestricted ? ny == ymin : ny <= ymin) || ny == ymax - ch) &&
                          diffX < snap && diffX < deltaX) {
                    // Snap to horizontal center on screen edge
                    nx = (xmin + xmax) / 2 - cw / 2;
                }
            }
        }

        pos = QPoint(nx, ny);
    }
    return pos;
}

QRect Workspace::adjustClientSize(AbstractClient* c, QRect moveResizeGeom, int mode)
{
    //adapted from adjustClientPosition on 29May2004
    //this function is called when resizing a window and will modify
    //the new dimensions to snap to other windows/borders if appropriate
    if (options->windowSnapZone() || options->borderSnapZone()) {  // || options->centerSnapZone )
        const bool sOWO = options->isSnapOnlyWhenOverlapping();

        const QRect maxRect = clientArea(MovementArea, c->rect().center(), c->desktop());
        const int xmin = maxRect.left();
        const int xmax = maxRect.right();               //desk size
        const int ymin = maxRect.top();
        const int ymax = maxRect.bottom();

        const int cx(moveResizeGeom.left());
        const int cy(moveResizeGeom.top());
        const int rx(moveResizeGeom.right());
        const int ry(moveResizeGeom.bottom());

        int newcx(cx), newcy(cy);                         //buffers
        int newrx(rx), newry(ry);
        int deltaX(xmax);
        int deltaY(ymax);   //minimum distance to other clients

        int lx, ly, lrx, lry; //coords and size for the comparison client, l

        // border snap
        int snap = options->borderSnapZone(); //snap trigger
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);

#define SNAP_BORDER_TOP \
    if ((sOWO?(newcy<ymin):true) && (qAbs(ymin-newcy)<deltaY)) \
    { \
        deltaY = qAbs(ymin-newcy); \
        newcy = ymin; \
    }

#define SNAP_BORDER_BOTTOM \
    if ((sOWO?(newry>ymax):true) && (qAbs(ymax-newry)<deltaY)) \
    { \
        deltaY = qAbs(ymax-newcy); \
        newry = ymax; \
    }

#define SNAP_BORDER_LEFT \
    if ((sOWO?(newcx<xmin):true) && (qAbs(xmin-newcx)<deltaX)) \
    { \
        deltaX = qAbs(xmin-newcx); \
        newcx = xmin; \
    }

#define SNAP_BORDER_RIGHT \
    if ((sOWO?(newrx>xmax):true) && (qAbs(xmax-newrx)<deltaX)) \
    { \
        deltaX = qAbs(xmax-newrx); \
        newrx = xmax; \
    }
            switch(mode) {
            case AbstractClient::PositionBottomRight:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_RIGHT
                break;
            case AbstractClient::PositionRight:
                SNAP_BORDER_RIGHT
                break;
            case AbstractClient::PositionBottom:
                SNAP_BORDER_BOTTOM
                break;
            case AbstractClient::PositionTopLeft:
                SNAP_BORDER_TOP
                SNAP_BORDER_LEFT
                break;
            case AbstractClient::PositionLeft:
                SNAP_BORDER_LEFT
                break;
            case AbstractClient::PositionTop:
                SNAP_BORDER_TOP
                break;
            case AbstractClient::PositionTopRight:
                SNAP_BORDER_TOP
                SNAP_BORDER_RIGHT
                break;
            case AbstractClient::PositionBottomLeft:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_LEFT
                break;
            default:
                abort();
                break;
            }


        }

        // windows snap
        snap = options->windowSnapZone();
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l)->isOnDesktop(VirtualDesktopManager::self()->current()) &&
                        !(*l)->isMinimized()
                        && (*l) != c) {
                    lx = (*l)->x() - 1;
                    ly = (*l)->y() - 1;
                    lrx = (*l)->x() + (*l)->width();
                    lry = (*l)->y() + (*l)->height();

#define WITHIN_HEIGHT ((( newcy <= lry ) && ( newcy  >= ly  ))  || \
                       (( newry >= ly  ) && ( newry  <= lry ))  || \
                       (( newcy <= ly  ) && ( newry >= lry  )) )

#define WITHIN_WIDTH  ( (( cx <= lrx ) && ( cx  >= lx  ))  || \
                        (( rx >= lx  ) && ( rx  <= lrx ))  || \
                        (( cx <= lx  ) && ( rx >= lrx  )) )

#define SNAP_WINDOW_TOP  if ( (sOWO?(newcy<lry):true) \
                              && WITHIN_WIDTH  \
                              && (qAbs( lry - newcy ) < deltaY) ) {  \
    deltaY = qAbs( lry - newcy ); \
    newcy=lry; \
}

#define SNAP_WINDOW_BOTTOM  if ( (sOWO?(newry>ly):true)  \
                                 && WITHIN_WIDTH  \
                                 && (qAbs( ly - newry ) < deltaY) ) {  \
    deltaY = qAbs( ly - newry );  \
    newry=ly;  \
}

#define SNAP_WINDOW_LEFT  if ( (sOWO?(newcx<lrx):true)  \
                               && WITHIN_HEIGHT  \
                               && (qAbs( lrx - newcx ) < deltaX)) {  \
    deltaX = qAbs( lrx - newcx );  \
    newcx=lrx;  \
}

#define SNAP_WINDOW_RIGHT  if ( (sOWO?(newrx>lx):true)  \
                                && WITHIN_HEIGHT  \
                                && (qAbs( lx - newrx ) < deltaX))  \
{  \
    deltaX = qAbs( lx - newrx );  \
    newrx=lx;  \
}

#define SNAP_WINDOW_C_TOP  if ( (sOWO?(newcy<ly):true)  \
                                && (newcx == lrx || newrx == lx)  \
                                && qAbs(ly-newcy) < deltaY ) {  \
    deltaY = qAbs( ly - newcy + 1 ); \
    newcy = ly + 1; \
}

#define SNAP_WINDOW_C_BOTTOM  if ( (sOWO?(newry>lry):true)  \
                                   && (newcx == lrx || newrx == lx)  \
                                   && qAbs(lry-newry) < deltaY ) {  \
    deltaY = qAbs( lry - newry - 1 ); \
    newry = lry - 1; \
}

#define SNAP_WINDOW_C_LEFT  if ( (sOWO?(newcx<lx):true)  \
                                 && (newcy == lry || newry == ly)  \
                                 && qAbs(lx-newcx) < deltaX ) {  \
    deltaX = qAbs( lx - newcx + 1 ); \
    newcx = lx + 1; \
}

#define SNAP_WINDOW_C_RIGHT  if ( (sOWO?(newrx>lrx):true)  \
                                  && (newcy == lry || newry == ly)  \
                                  && qAbs(lrx-newrx) < deltaX ) {  \
    deltaX = qAbs( lrx - newrx - 1 ); \
    newrx = lrx - 1; \
}

                    switch(mode) {
                    case AbstractClient::PositionBottomRight:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case AbstractClient::PositionRight:
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case AbstractClient::PositionBottom:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_C_BOTTOM
                        break;
                    case AbstractClient::PositionTopLeft:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_LEFT
                        break;
                    case AbstractClient::PositionLeft:
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_LEFT
                        break;
                    case AbstractClient::PositionTop:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_C_TOP
                        break;
                    case AbstractClient::PositionTopRight:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case AbstractClient::PositionBottomLeft:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_LEFT
                        break;
                    default:
                        abort();
                        break;
                    }
                }
            }
        }

        // center snap
        //snap = options->centerSnapZone;
        //if (snap)
        //    {
        //    // Don't resize snap to center as it interferes too much
        //    // There are two ways of implementing this if wanted:
        //    // 1) Snap only to the same points that the move snap does, and
        //    // 2) Snap to the horizontal and vertical center lines of the screen
        //    }

        moveResizeGeom = QRect(QPoint(newcx, newcy), QPoint(newrx, newry));
    }
    return moveResizeGeom;
}

/**
 * Marks the client as being moved or resized by the user.
 */
void Workspace::setMoveResizeClient(AbstractClient *c)
{
    Q_ASSERT(!c || !movingClient); // Catch attempts to move a second
    // window while still moving the first one.
    movingClient = c;
    if (movingClient)
        ++block_focus;
    else
        --block_focus;
}

// When kwin crashes, windows will not be gravitated back to their original position
// and will remain offset by the size of the decoration. So when restarting, fix this
// (the property with the size of the frame remains on the window after the crash).
void Workspace::fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geometry)
{
    NETWinInfo i(connection(), w, rootWindow(), NET::WMFrameExtents, NET::Properties2());
    NETStrut frame = i.frameExtents();

    if (frame.left != 0 || frame.top != 0) {
        // left and top needed due to narrowing conversations restrictions in C++11
        const uint32_t left = frame.left;
        const uint32_t top = frame.top;
        const uint32_t values[] = { geometry->x - left, geometry->y - top };
        xcb_configure_window(connection(), w, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }
}

} // namespace
