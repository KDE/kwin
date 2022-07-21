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
#include "output.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "atoms.h"
#include "composite.h"
#include "contexts.h"
#include "cursor.h"
#include "dbusinterface.h"
#include "deleted.h"
#include "effects.h"
#include "focuschain.h"
#include "group.h"
#include "input.h"
#include "internalwindow.h"
#include "killwindow.h"
#include "moving_client_x11_filter.h"
#include "netinfo.h"
#include "outline.h"
#include "placement.h"
#include "platform.h"
#include "pluginmanager.h"
#include "rules.h"
#include "screenedge.h"
#include "screens.h"
#include "scripting/scripting.h"
#include "syncalarmx11filter.h"
#include "x11window.h"
#if KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "decorations/decorationbridge.h"
#include "main.h"
#include "unmanaged.h"
#include "useractions.h"
#include "utils/xcbutils.h"
#include "virtualdesktops.h"
#include "was_user_interaction_x11_filter.h"
#include "wayland_server.h"
#include "xwaylandwindow.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KStartupInfo>
// Qt
#include <QtConcurrentRun>
// xcb
#include <xcb/xinerama.h>

namespace KWin
{

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
    if (X11Window *c = dynamic_cast<X11Window *>(Workspace::self()->activeWindow())) {
        if (c->colormap() != XCB_COLORMAP_NONE) {
            cmap = c->colormap();
        }
    }
    if (cmap != m_installed) {
        xcb_install_colormap(kwinApp()->x11Connection(), cmap);
        m_installed = cmap;
    }
}

Workspace *Workspace::_self = nullptr;

Workspace::Workspace()
    : QObject(nullptr)
    // Unsorted
    , m_quickTileCombineTimer(nullptr)
    , active_popup(nullptr)
    , m_activePopupWindow(nullptr)
    , m_initialDesktop(1)
    , m_activeWindow(nullptr)
    , m_lastActiveWindow(nullptr)
    , m_moveResizeWindow(nullptr)
    , m_delayFocusWindow(nullptr)
    , force_restacking(false)
    , showing_desktop(false)
    , was_user_interaction(false)
    , block_focus(0)
    , m_userActionsMenu(new UserActionsMenu(this))
    , workspaceInit(true)
    , m_sessionManager(new SessionManager(this))
    , m_focusChain(std::make_unique<FocusChain>())
    , m_applicationMenu(std::make_unique<ApplicationMenu>())
{
    // If KWin was already running it saved its configuration after loosing the selection -> Reread
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QFuture<void> reparseConfigFuture = QtConcurrent::run(options, &Options::reparseConfiguration);
#else
    QFuture<void> reparseConfigFuture = QtConcurrent::run(&Options::reparseConfiguration, options);
#endif

    _self = this;

#if KWIN_BUILD_ACTIVITIES
    if (kwinApp()->usesKActivities()) {
        m_activities = std::make_unique<Activities>();
    }
    if (m_activities) {
        connect(m_activities.get(), &Activities::currentChanged, this, &Workspace::updateCurrentActivity);
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

    Screens::create(this);
    ScreenEdges::create(this);

    // VirtualDesktopManager needs to be created prior to init shortcuts
    // and prior to TabBox, due to TabBox connecting to signals
    // actual initialization happens in init()
    VirtualDesktopManager::create(this);
    // dbus interface
    new VirtualDesktopManagerDBusInterface(VirtualDesktopManager::self());

#if KWIN_BUILD_TABBOX
    // need to create the tabbox before compositing scene is setup
    TabBox::TabBox::create(this);
#endif

    if (!Compositor::self()) {
        Q_ASSERT(kwinApp()->operationMode() == Application::OperationMode::OperationModeX11);
        X11Compositor::create(this);
    }

    m_decorationBridge = std::make_unique<Decoration::DecorationBridge>();
    m_decorationBridge->init();
    connect(this, &Workspace::configChanged, m_decorationBridge.get(), &Decoration::DecorationBridge::reconfigure);

    new DBusInterface(this);
    Outline::create(this);

    initShortcuts();

    init();
}

void Workspace::init()
{
    KSharedConfigPtr config = kwinApp()->config();
    ScreenEdges *screenEdges = ScreenEdges::self();
    screenEdges->setConfig(config);
    screenEdges->init();
    connect(options, &Options::configChanged, screenEdges, &ScreenEdges::reconfigure);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::layoutChanged, screenEdges, &ScreenEdges::updateLayout);
    connect(this, &Workspace::windowActivated, screenEdges, &ScreenEdges::checkBlocking);

    connect(this, &Workspace::windowRemoved, m_focusChain.get(), &FocusChain::remove);
    connect(this, &Workspace::windowActivated, m_focusChain.get(), &FocusChain::setActiveWindow);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, m_focusChain.get(), [this]() {
        m_focusChain->setCurrentDesktop(VirtualDesktopManager::self()->currentDesktop());
    });
    connect(options, &Options::separateScreenFocusChanged, m_focusChain.get(), &FocusChain::setSeparateScreenFocus);
    m_focusChain->setSeparateScreenFocus(options->isSeparateScreenFocus());

    Platform *platform = kwinApp()->platform();
    connect(platform, &Platform::outputEnabled, this, &Workspace::slotOutputEnabled);
    connect(platform, &Platform::outputDisabled, this, &Workspace::slotOutputDisabled);

    const QVector<Output *> outputs = platform->outputs();
    for (Output *output : outputs) {
        if (output->isEnabled()) {
            slotOutputEnabled(output);
        }
    }

    Screens *screens = Screens::self();
    screens->init();

    // create VirtualDesktopManager and perform dependency injection
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, &VirtualDesktopManager::desktopCreated, this, &Workspace::slotDesktopAdded);
    connect(vds, &VirtualDesktopManager::desktopRemoved, this, &Workspace::slotDesktopRemoved);
    connect(vds, &VirtualDesktopManager::currentChanged, this, &Workspace::slotCurrentDesktopChanged);
    connect(vds, &VirtualDesktopManager::currentChanging, this, &Workspace::slotCurrentDesktopChanging);
    connect(vds, &VirtualDesktopManager::currentChangingCancelled, this, &Workspace::slotCurrentDesktopChangingCancelled);
    vds->setNavigationWrappingAround(options->isRollOverDesktops());
    connect(options, &Options::rollOverDesktopsChanged, vds, &VirtualDesktopManager::setNavigationWrappingAround);
    vds->setConfig(config);

    // Now we know how many desktops we'll have, thus we initialize the positioning object
    Placement::create(this);

    // positioning object needs to be created before the virtual desktops are loaded.
    vds->load();
    vds->updateLayout();
    // makes sure any autogenerated id is saved, necessary as in case of xwayland, load will be called 2 times
    //  load is needed to be called again when starting xwayalnd to sync to RootInfo, see BUG 385260
    vds->save();

    if (!VirtualDesktopManager::self()->setCurrent(m_initialDesktop)) {
        VirtualDesktopManager::self()->setCurrent(1);
    }

    reconfigureTimer.setSingleShot(true);
    updateToolWindowsTimer.setSingleShot(true);

    connect(&reconfigureTimer, &QTimer::timeout, this, &Workspace::slotReconfigure);
    connect(&updateToolWindowsTimer, &QTimer::timeout, this, &Workspace::slotUpdateToolWindows);

    // TODO: do we really need to reconfigure everything when fonts change?
    // maybe just reconfigure the decorations? Move this into libkdecoration?
    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/KDEPlatformTheme"),
                                          QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"),
                                          this, SLOT(reconfigure()));

    m_activeWindow = nullptr;

    // We want to have some xcb connection while tearing down X11 components. We don't really
    // care if the xcb connection is broken or has an error.
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &Workspace::initializeX11);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this, &Workspace::cleanupX11);
    initializeX11();

    Scripting::create(this);

    if (auto server = waylandServer()) {
        connect(server, &WaylandServer::windowAdded, this, &Workspace::addWaylandWindow);
        connect(server, &WaylandServer::windowRemoved, this, &Workspace::removeWaylandWindow);
    }

    // SELI TODO: This won't work with unreasonable focus policies,
    // and maybe in rare cases also if the selected window doesn't
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
    connect(this, &Workspace::windowActivated, m_colorMapper.data(), &ColorMapper::update);

    // Call this before XSelectInput() on the root window
    m_startup.reset(new KStartupInfo(
        KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this));

    // Select windowmanager privileges
    selectWmInputEventMask();

    // Compatibility
    int32_t data = 1;

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
    NETRootInfo client_info(kwinApp()->x11Connection(), NET::ActiveWindow | NET::CurrentDesktop);
    bool sessionRestored = false;
#ifndef QT_NO_SESSIONMANAGER
    sessionRestored = qApp->isSessionRestored();
#endif
    if (!sessionRestored) {
        m_initialDesktop = client_info.currentDesktop();
        vds->setCurrent(m_initialDesktop);
    }

    // TODO: better value
    rootInfo->setActiveWindow(XCB_WINDOW_NONE);
    focusToNull();

    if (!sessionRestored) {
        ++block_focus; // Because it will be set below
    }

    {
        // Begin updates blocker block
        StackingUpdatesBlocker blocker(this);

        Xcb::Tree tree(kwinApp()->x11RootWindow());
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
                if (attr->map_state == XCB_MAP_STATE_VIEWABLE && attr->_class != XCB_WINDOW_CLASS_INPUT_ONLY) {
                    // ### This will request the attributes again
                    createUnmanaged(wins[i]);
                }
            } else if (attr->map_state != XCB_MAP_STATE_UNMAPPED) {
                if (Application::wasCrash()) {
                    fixPositionAfterCrash(wins[i], windowGeometries.at(i).data());
                }

                // ### This will request the attributes again
                createX11Window(wins[i], true);
            }
        }

        // Propagate windows, will really happen at the end of the updates blocker block
        updateStackingOrder(true);

        saveOldScreenSizes();
        updateClientArea();

        // NETWM spec says we have to set it to (0,0) if we don't support it
        NETPoint *viewports = new NETPoint[VirtualDesktopManager::self()->count()];
        rootInfo->setDesktopViewport(VirtualDesktopManager::self()->count(), *viewports);
        delete[] viewports;

        NETSize desktop_geometry;
        desktop_geometry.width = m_geometry.width();
        desktop_geometry.height = m_geometry.height();
        rootInfo->setDesktopGeometry(desktop_geometry);
        setShowingDesktop(false);

    } // End updates blocker block

    // TODO: only on X11?
    Window *newActiveWindow = nullptr;
    if (!sessionRestored) {
        --block_focus;
        newActiveWindow = findClient(Predicate::WindowMatch, client_info.activeWindow());
    }
    if (newActiveWindow == nullptr && activeWindow() == nullptr && should_get_focus.count() == 0) {
        // No client activated in manage()
        if (newActiveWindow == nullptr) {
            newActiveWindow = topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop());
        }
        if (newActiveWindow == nullptr) {
            newActiveWindow = findDesktop(true, VirtualDesktopManager::self()->currentDesktop());
        }
    }
    if (newActiveWindow != nullptr) {
        activateWindow(newActiveWindow);
    }
}

void Workspace::cleanupX11()
{
    // We expect that other components will unregister their X11 event filters after the
    // connection to the X server has been lost.

    StackingUpdatesBlocker blocker(this);

    // Use stacking_order, so that kwin --replace keeps stacking order.
    const QList<X11Window *> orderedClients = ensureStackingOrder(m_x11Clients);
    for (X11Window *client : orderedClients) {
        client->releaseWindow(true);
        removeFromStack(client);
    }

    // We need a shadow copy because windows get removed as we go through them.
    const QList<Unmanaged *> unmanaged = m_unmanaged;
    for (Unmanaged *overrideRedirect : unmanaged) {
        overrideRedirect->release(ReleaseReason::KWinShutsDown);
        removeFromStack(overrideRedirect);
    }

    manual_overlays.clear();

    VirtualDesktopManager *desktopManager = VirtualDesktopManager::self();
    desktopManager->setRootInfo(nullptr);

    X11Window::cleanupX11();
    RootInfo::destroy();
    Xcb::Extensions::destroy();

    m_colorMapper.reset();
    m_movingClientFilter.reset();
    m_startup.reset();
    m_nullFocus.reset();
    m_syncAlarmFilter.reset();
    m_wasUserInteractionFilter.reset();
}

Workspace::~Workspace()
{
    blockStackingUpdates(true);

    cleanupX11();

    if (waylandServer()) {
        const QList<Window *> waylandWindows = waylandServer()->windows();
        for (Window *window : waylandWindows) {
            window->destroyWindow();
        }
    }

    // We need a shadow copy because windows get removed as we go through them.
    const QList<InternalWindow *> internalWindows = m_internalWindows;
    for (InternalWindow *window : internalWindows) {
        window->destroyWindow();
    }

    for (auto it = deleted.begin(); it != deleted.end();) {
        Q_EMIT deletedRemoved(*it);
        (*it)->finishCompositing();
        it = deleted.erase(it);
    }

    delete RuleBook::self();
    kwinApp()->config()->sync();

    delete Placement::self();
    delete m_windowKeysDialog;

    _self = nullptr;
}

void Workspace::setupWindowConnections(Window *window)
{
    connect(window, &Window::desktopPresenceChanged, this, &Workspace::desktopPresenceChanged);
    connect(window, &Window::minimizedChanged, this, std::bind(&Workspace::windowMinimizedChanged, this, window));
    connect(window, &Window::fullScreenChanged, ScreenEdges::self(), &ScreenEdges::checkBlocking);
}

void Workspace::constrain(Window *below, Window *above)
{
    if (below == above) {
        return;
    }

    QList<Constraint *> parents;
    QList<Constraint *> children;
    for (Constraint *constraint : qAsConst(m_constraints)) {
        if (constraint->below == below && constraint->above == above) {
            return;
        }
        if (constraint->below == above) {
            children << constraint;
        } else if (constraint->above == below) {
            parents << constraint;
        }
    }

    Constraint *constraint = new Constraint();
    constraint->parents = parents;
    constraint->below = below;
    constraint->above = above;
    constraint->children = children;
    m_constraints << constraint;

    for (Constraint *parent : qAsConst(parents)) {
        parent->children << constraint;
    }

    for (Constraint *child : qAsConst(children)) {
        child->parents << constraint;
    }

    updateStackingOrder();
}

void Workspace::unconstrain(Window *below, Window *above)
{
    Constraint *constraint = nullptr;
    for (int i = 0; i < m_constraints.count(); ++i) {
        if (m_constraints[i]->below == below && m_constraints[i]->above == above) {
            constraint = m_constraints.takeAt(i);
            break;
        }
    }

    if (!constraint) {
        return;
    }

    const QList<Constraint *> parents = constraint->parents;
    for (Constraint *parent : parents) {
        parent->children.removeOne(constraint);
    }

    const QList<Constraint *> children = constraint->children;
    for (Constraint *child : children) {
        child->parents.removeOne(constraint);
    }

    delete constraint;
    updateStackingOrder();
}

void Workspace::addToStack(Window *window)
{
    // If the stacking order of a window has been restored from the session, that
    // window will already be in the stack when Workspace::addX11Window() is called.
    if (!unconstrained_stacking_order.contains(window)) {
        unconstrained_stacking_order.append(window);
    }
    if (!stacking_order.contains(window)) {
        stacking_order.append(window);
    }
}

void Workspace::replaceInStack(Window *original, Deleted *deleted)
{
    const int unconstraintedIndex = unconstrained_stacking_order.indexOf(original);
    if (unconstraintedIndex != -1) {
        unconstrained_stacking_order.replace(unconstraintedIndex, deleted);
    } else {
        // This can be the case only if an override-redirect window is unmapped.
        unconstrained_stacking_order.append(deleted);
    }

    const int index = stacking_order.indexOf(original);
    if (index != -1) {
        stacking_order.replace(index, deleted);
    } else {
        // This can be the case only if an override-redirect window is unmapped.
        stacking_order.append(deleted);
    }

    for (Constraint *constraint : qAsConst(m_constraints)) {
        if (constraint->below == original) {
            constraint->below = deleted;
        } else if (constraint->above == original) {
            constraint->above = deleted;
        }
    }
}

void Workspace::removeFromStack(Window *window)
{
    unconstrained_stacking_order.removeAll(window);
    stacking_order.removeAll(window);

    for (int i = m_constraints.count() - 1; i >= 0; --i) {
        Constraint *constraint = m_constraints[i];
        const bool isBelow = (constraint->below == window);
        const bool isAbove = (constraint->above == window);
        if (!isBelow && !isAbove) {
            continue;
        }
        if (isBelow) {
            for (Constraint *child : qAsConst(constraint->children)) {
                child->parents.removeOne(constraint);
            }
        } else {
            for (Constraint *parent : qAsConst(constraint->parents)) {
                parent->children.removeOne(constraint);
            }
        }
        delete m_constraints.takeAt(i);
    }
}

X11Window *Workspace::createX11Window(xcb_window_t windowId, bool is_mapped)
{
    StackingUpdatesBlocker blocker(this);
    X11Window *window = nullptr;
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        window = new X11Window();
    } else {
        window = new XwaylandWindow();
    }
    setupWindowConnections(window);
    if (X11Compositor *compositor = X11Compositor::self()) {
        connect(window, &X11Window::blockingCompositingChanged, compositor, &X11Compositor::updateClientCompositeBlocking);
    }
    if (!window->manage(windowId, is_mapped)) {
        X11Window::deleteClient(window);
        return nullptr;
    }
    addX11Window(window);
    Q_EMIT windowAdded(window);
    return window;
}

Unmanaged *Workspace::createUnmanaged(xcb_window_t windowId)
{
    if (X11Compositor *compositor = X11Compositor::self()) {
        if (compositor->checkForOverlayWindow(windowId)) {
            return nullptr;
        }
    }
    Unmanaged *window = new Unmanaged();
    if (!window->track(windowId)) {
        Unmanaged::deleteUnmanaged(window);
        return nullptr;
    }
    addUnmanaged(window);
    Q_EMIT unmanagedAdded(window);
    return window;
}

void Workspace::addX11Window(X11Window *window)
{
    Group *grp = findGroup(window->window());
    if (grp != nullptr) {
        grp->gotLeader(window);
    }

    if (window->isDesktop()) {
        if (m_activeWindow == nullptr && should_get_focus.isEmpty() && window->isOnCurrentDesktop()) {
            requestFocus(window); // TODO: Make sure desktop is active after startup if there's no other window active
        }
    } else {
        m_focusChain->update(window, FocusChain::Update);
    }
    m_x11Clients.append(window);
    m_allClients.append(window);
    addToStack(window);
    updateClientArea(); // This cannot be in manage(), because the window got added only now
    window->updateLayer();
    if (window->isDesktop()) {
        raiseWindow(window);
        // If there's no active window, make this desktop the active one
        if (activeWindow() == nullptr && should_get_focus.count() == 0) {
            activateWindow(findDesktop(true, VirtualDesktopManager::self()->currentDesktop()));
        }
    }
    window->checkActiveModal();
    checkTransients(window->window()); // SELI TODO: Does this really belong here?
    updateStackingOrder(true); // Propagate new window
    if (window->isUtility() || window->isMenu() || window->isToolbar()) {
        updateToolWindows(true);
    }
    updateTabbox();
}

void Workspace::addUnmanaged(Unmanaged *window)
{
    m_unmanaged.append(window);
    addToStack(window);
}

/**
 * Destroys the window \a window
 */
void Workspace::removeX11Window(X11Window *window)
{
    Q_ASSERT(m_x11Clients.contains(window));
    // TODO: if marked window is removed, notify the marked list
    m_x11Clients.removeAll(window);
    Group *group = findGroup(window->window());
    if (group != nullptr) {
        group->lostLeader();
    }
    removeWindow(window);
}

void Workspace::removeUnmanaged(Unmanaged *window)
{
    Q_ASSERT(m_unmanaged.contains(window));
    m_unmanaged.removeAll(window);
    removeFromStack(window);
    Q_EMIT unmanagedRemoved(window);
}

void Workspace::addDeleted(Deleted *c, Window *orig)
{
    Q_ASSERT(!deleted.contains(c));
    deleted.append(c);
    replaceInStack(orig, c);
}

void Workspace::removeDeleted(Deleted *c)
{
    Q_ASSERT(deleted.contains(c));
    Q_EMIT deletedRemoved(c);
    deleted.removeAll(c);
    removeFromStack(c);
    if (!c->wasClient()) {
        return;
    }
    if (X11Compositor *compositor = X11Compositor::self()) {
        compositor->updateClientCompositeBlocking();
    }
}

void Workspace::addWaylandWindow(Window *window)
{
    setupWindowConnections(window);
    window->updateLayer();

    if (window->isPlaceable()) {
        const QRectF area = clientArea(PlacementArea, window, activeOutput());
        bool placementDone = false;
        if (window->isRequestedFullScreen()) {
            placementDone = true;
        }
        if (window->requestedMaximizeMode() == MaximizeMode::MaximizeFull) {
            placementDone = true;
        }
        if (window->rules()->checkPosition(invalidPoint, true) != invalidPoint) {
            placementDone = true;
        }
        if (!placementDone) {
            Placement::self()->place(window, area);
        }
    }
    m_allClients.append(window);
    addToStack(window);

    updateStackingOrder(true);
    updateClientArea();
    if (window->wantsInput() && !window->isMinimized()) {
        activateWindow(window);
    }
    updateTabbox();
    Q_EMIT windowAdded(window);
}

void Workspace::removeWaylandWindow(Window *window)
{
    windowHidden(window);
    removeWindow(window);
}

void Workspace::removeWindow(Window *window)
{
    if (window == m_activePopupWindow) {
        closeActivePopup();
    }
    if (m_userActionsMenu->isMenuWindow(window)) {
        m_userActionsMenu->close();
    }

    m_allClients.removeAll(window);
    if (window == m_delayFocusWindow) {
        cancelDelayFocus();
    }
    attention_chain.removeAll(window);
    should_get_focus.removeAll(window);
    if (window == m_activeWindow) {
        m_activeWindow = nullptr;
    }
    if (window == m_lastActiveWindow) {
        m_lastActiveWindow = nullptr;
    }
    if (m_windowKeysWindow == window) {
        setupWindowShortcutDone(false);
    }
    if (!window->shortcut().isEmpty()) {
        window->setShortcut(QString()); // Remove from client_keys
        windowShortcutUpdated(window); // Needed, since this is otherwise delayed by setShortcut() and wouldn't run
    }

    Q_EMIT windowRemoved(window);

    updateStackingOrder(true);
    updateClientArea();
    updateTabbox();
}

void Workspace::updateToolWindows(bool also_hide)
{
    // TODO: What if Client's transiency/group changes? should this be called too? (I'm paranoid, am I not?)
    if (!options->isHideUtilityWindowsForInactive()) {
        for (auto it = m_x11Clients.constBegin(); it != m_x11Clients.constEnd(); ++it) {
            (*it)->showClient();
        }
        return;
    }
    const Group *group = nullptr;
    auto window = m_activeWindow;
    // Go up in transiency hiearchy, if the top is found, only tool transients for the top mainwindow
    // will be shown; if a group transient is group, all tools in the group will be shown
    while (window != nullptr) {
        if (!window->isTransient()) {
            break;
        }
        if (window->groupTransient()) {
            group = window->group();
            break;
        }
        window = window->transientFor();
    }
    // Use stacking order only to reduce flicker, it doesn't matter if block_stacking_updates == 0,
    // I.e. if it's not up to date

    // SELI TODO: But maybe it should - what if a new window has been added that's not in stacking order yet?
    QVector<Window *> to_show, to_hide;
    for (auto it = stacking_order.constBegin(); it != stacking_order.constEnd(); ++it) {
        auto c = *it;
        if (!c->isClient()) {
            continue;
        }
        if (c->isUtility() || c->isMenu() || c->isToolbar()) {
            bool show = true;
            if (!c->isTransient()) {
                if (!c->group() || c->group()->members().count() == 1) { // Has its own group, keep always visible
                    show = true;
                } else if (window != nullptr && c->group() == window->group()) {
                    show = true;
                } else {
                    show = false;
                }
            } else {
                if (group != nullptr && c->group() == group) {
                    show = true;
                } else if (window != nullptr && window->hasTransient(c, true)) {
                    show = true;
                } else {
                    show = false;
                }
            }
            if (!show && also_hide) {
                const auto mainwindows = c->mainWindows();
                // Don't hide utility windows which are standalone(?) or
                // have e.g. kicker as mainwindow
                if (mainwindows.isEmpty()) {
                    show = true;
                }
                for (auto it2 = mainwindows.constBegin(); it2 != mainwindows.constEnd(); ++it2) {
                    if ((*it2)->isSpecialWindow()) {
                        show = true;
                    }
                }
                if (!show) {
                    to_hide.append(c);
                }
            }
            if (show) {
                to_show.append(c);
            }
        }
    } // First show new ones, then hide
    for (int i = to_show.size() - 1; i >= 0; --i) { // From topmost
        // TODO: Since this is in stacking order, the order of taskbar entries changes :(
        to_show.at(i)->showClient();
    }
    if (also_hide) {
        for (auto it = to_hide.constBegin(); it != to_hide.constEnd(); ++it) { // From bottommost
            (*it)->hideClient();
        }
        updateToolWindowsTimer.stop();
    } else { // setActiveWindow() is after called with NULL window, quickly followed
        // by setting a new window, which would result in flickering
        resetUpdateToolWindowsTimer();
    }
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
    contexts()->reconfigure();

    Q_EMIT configChanged();
    m_userActionsMenu->discard();
    updateToolWindows(true);

    RuleBook::self()->load();
    for (Window *window : qAsConst(m_allClients)) {
        if (window->supportsWindowRules()) {
            window->evaluateWindowRules();
            RuleBook::self()->discardUsed(window, false);
        }
    }

    if (borderlessMaximizedWindows != options->borderlessMaximizedWindows() && !options->borderlessMaximizedWindows()) {
        // in case borderless maximized windows option changed and new option
        // is to have borders, we need to unset the borders for all maximized windows
        for (auto it = m_allClients.cbegin(); it != m_allClients.cend(); ++it) {
            if ((*it)->maximizeMode() == MaximizeFull) {
                (*it)->checkNoBorder();
            }
        }
    }
}

void Workspace::slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop)
{
    closeActivePopup();
    ++block_focus;
    StackingUpdatesBlocker blocker(this);
    updateWindowVisibilityOnDesktopChange(VirtualDesktopManager::self()->desktopForX11Id(newDesktop));
    // Restore the focus on this desktop
    --block_focus;

    activateWindowOnNewDesktop(VirtualDesktopManager::self()->desktopForX11Id(newDesktop));
    Q_EMIT currentDesktopChanged(oldDesktop, m_moveResizeWindow);
}

void Workspace::slotCurrentDesktopChanging(uint currentDesktop, QPointF offset)
{
    closeActivePopup();
    Q_EMIT currentDesktopChanging(currentDesktop, offset, m_moveResizeWindow);
}

void Workspace::slotCurrentDesktopChangingCancelled()
{
    Q_EMIT currentDesktopChangingCancelled();
}

void Workspace::updateWindowVisibilityOnDesktopChange(VirtualDesktop *newDesktop)
{
    for (auto it = stacking_order.constBegin(); it != stacking_order.constEnd(); ++it) {
        X11Window *c = qobject_cast<X11Window *>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnDesktop(newDesktop) && c != m_moveResizeWindow && c->isOnCurrentActivity()) {
            (c)->updateVisibility();
        }
    }
    // Now propagate the change, after hiding, before showing
    if (rootInfo()) {
        rootInfo()->setCurrentDesktop(VirtualDesktopManager::self()->current());
    }

    if (m_moveResizeWindow && !m_moveResizeWindow->isOnDesktop(newDesktop)) {
        m_moveResizeWindow->setDesktops({newDesktop});
    }

    for (int i = stacking_order.size() - 1; i >= 0; --i) {
        X11Window *c = qobject_cast<X11Window *>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnDesktop(newDesktop) && c->isOnCurrentActivity()) {
            c->updateVisibility();
        }
    }
    if (showingDesktop()) { // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);
    }
}

void Workspace::activateWindowOnNewDesktop(VirtualDesktop *desktop)
{
    Window *window = nullptr;
    if (options->focusPolicyIsReasonable()) {
        window = findWindowToActivateOnDesktop(desktop);
    }
    // If "unreasonable focus policy" and m_activeWindow is on_all_desktops and
    // under mouse (Hence == old_active_window), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (m_activeWindow && m_activeWindow->isShown() && m_activeWindow->isOnCurrentDesktop()) {
        window = m_activeWindow;
    }

    if (!window) {
        window = findDesktop(true, desktop);
    }

    if (window != m_activeWindow) {
        setActiveWindow(nullptr);
    }

    if (window) {
        requestFocus(window);
    } else {
        focusToNull();
    }
}

Window *Workspace::findWindowToActivateOnDesktop(VirtualDesktop *desktop)
{
    if (m_moveResizeWindow != nullptr && m_activeWindow == m_moveResizeWindow && m_focusChain->contains(m_activeWindow, desktop) && m_activeWindow->isShown() && m_activeWindow->isOnCurrentDesktop()) {
        // A requestFocus call will fail, as the window is already active
        return m_activeWindow;
    }
    // from actiavtion.cpp
    if (options->isNextFocusPrefersMouse()) {
        auto it = stackingOrder().constEnd();
        while (it != stackingOrder().constBegin()) {
            auto window = *(--it);
            if (!window->isClient()) {
                continue;
            }

            if (!(!window->isShade() && window->isShown() && window->isOnDesktop(desktop) && window->isOnCurrentActivity() && window->isOnActiveOutput())) {
                continue;
            }

            // port to hit test
            if (window->frameGeometry().toRect().contains(Cursors::self()->mouse()->pos())) {
                if (!window->isDesktop()) {
                    return window;
                }
                break; // unconditional break  - we do not pass the focus to some window below an unusable one
            }
        }
    }
    return m_focusChain->getForActivation(desktop);
}

/**
 * Updates the current activity when it changes
 * do *not* call this directly; it does not set the activity.
 *
 * Shows/Hides windows according to the stacking order
 */

void Workspace::updateCurrentActivity(const QString &new_activity)
{
#if KWIN_BUILD_ACTIVITIES
    if (!m_activities) {
        return;
    }
    // closeActivePopup();
    ++block_focus;
    // TODO: Q_ASSERT( block_stacking_updates == 0 ); // Make sure stacking_order is up to date
    StackingUpdatesBlocker blocker(this);

    // Optimized Desktop switching: unmapping done from back to front
    // mapping done from front to back => less exposure events
    // Notify::raise((Notify::Event) (Notify::DesktopChange+new_desktop));

    for (auto it = stacking_order.constBegin(); it != stacking_order.constEnd(); ++it) {
        X11Window *window = qobject_cast<X11Window *>(*it);
        if (!window) {
            continue;
        }
        if (!window->isOnActivity(new_activity) && window != m_moveResizeWindow && window->isOnCurrentDesktop()) {
            window->updateVisibility();
        }
    }

    // Now propagate the change, after hiding, before showing
    // rootInfo->setCurrentDesktop( currentDesktop() );

    /* TODO someday enable dragging windows to other activities
    if ( m_moveResizeWindow && !m_moveResizeWindow->isOnDesktop( new_desktop ))
        {
        m_moveResizeWindow->setDesktop( new_desktop );
        */

    for (int i = stacking_order.size() - 1; i >= 0; --i) {
        X11Window *window = qobject_cast<X11Window *>(stacking_order.at(i));
        if (!window) {
            continue;
        }
        if (window->isOnActivity(new_activity)) {
            window->updateVisibility();
        }
    }

    // FIXME not sure if I should do this either
    if (showingDesktop()) { // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);
    }

    // Restore the focus on this desktop
    --block_focus;
    Window *window = nullptr;

    // FIXME below here is a lot of focuschain stuff, probably all wrong now
    //  Keep active window focused if it's on the new activity
    if (m_activeWindow && m_activeWindow->isShown() && m_activeWindow->isOnCurrentDesktop() && m_activeWindow->isOnCurrentActivity()) {
        window = m_activeWindow;
    } else if (options->focusPolicyIsReasonable()) {
        // Search in focus chain
        window = m_focusChain->getForActivation(VirtualDesktopManager::self()->currentDesktop());
    }

    if (!window) {
        window = findDesktop(true, VirtualDesktopManager::self()->currentDesktop());
    }

    if (window != m_activeWindow) {
        setActiveWindow(nullptr);
    }

    if (window) {
        requestFocus(window);
    } else {
        focusToNull();
    }

    Q_EMIT currentActivityChanged();
#else
    Q_UNUSED(new_activity)
#endif
}

Output *Workspace::outputAt(const QPointF &pos) const
{
    Output *bestOutput = nullptr;
    int minDistance = INT_MAX;
    for (Output *output : std::as_const(m_outputs)) {
        const QRect &geo = output->geometry();
        if (geo.contains(pos.toPoint())) {
            return output;
        }
        qreal distance = QPointF(geo.topLeft() - pos).manhattanLength();
        distance = std::min(distance, QPointF(geo.topRight() - pos).manhattanLength());
        distance = std::min(distance, QPointF(geo.bottomRight() - pos).manhattanLength());
        distance = std::min(distance, QPointF(geo.bottomLeft() - pos).manhattanLength());
        if (distance < minDistance) {
            minDistance = distance;
            bestOutput = output;
        }
    }
    return bestOutput;
}

void Workspace::slotOutputEnabled(Output *output)
{
    if (output->isNonDesktop()) {
        return;
    }
    if (!m_activeOutput) {
        m_activeOutput = output;
    }

    m_outputs.append(output);

    connect(output, &Output::geometryChanged, this, &Workspace::desktopResized);
    desktopResized();

    // Trigger a re-check of output-related rules on all windows
    for (Window *window : qAsConst(m_allClients)) {
        sendWindowToOutput(window, window->output());
    }

    Q_EMIT outputAdded(output);
}

void Workspace::slotOutputDisabled(Output *output)
{
    if (output->isNonDesktop()) {
        return;
    }

    m_outputs.removeOne(output);

    if (m_activeOutput == output) {
        m_activeOutput = outputAt(output->geometry().center());
    }

    disconnect(output, &Output::geometryChanged, this, &Workspace::desktopResized);
    desktopResized();

    const auto stack = stackingOrder();
    for (Window *window : stack) {
        if (window->output() == output) {
            window->setOutput(outputAt(window->frameGeometry().center()));
        }
    }

    Q_EMIT outputRemoved(output);
}

void Workspace::slotDesktopAdded(VirtualDesktop *desktop)
{
    m_focusChain->addDesktop(desktop);
    Placement::self()->reinitCascading(0);
    updateClientArea();
}

void Workspace::slotDesktopRemoved(VirtualDesktop *desktop)
{
    for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
        if (!(*it)->desktops().contains(desktop)) {
            continue;
        }
        if ((*it)->desktops().count() > 1) {
            (*it)->leaveDesktop(desktop);
        } else {
            sendWindowToDesktop(*it, qMin(desktop->x11DesktopNumber(), VirtualDesktopManager::self()->count()), true);
        }
    }

    updateClientArea();
    Placement::self()->reinitCascading(0);
    m_focusChain->removeDesktop(desktop);
}

void Workspace::selectWmInputEventMask()
{
    uint32_t presentMask = 0;
    Xcb::WindowAttributes attr(kwinApp()->x11RootWindow());
    if (!attr.isNull()) {
        presentMask = attr->your_event_mask;
    }

    const uint32_t wmMask = XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_PROPERTY_CHANGE
        | XCB_EVENT_MASK_COLOR_MAP_CHANGE
        | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
        | XCB_EVENT_MASK_FOCUS_CHANGE // For NotifyDetailNone
        | XCB_EVENT_MASK_EXPOSURE;

    Xcb::selectInput(kwinApp()->x11RootWindow(), presentMask | wmMask);
}

/**
 * Sends window \a window to desktop \a desk.
 *
 * Takes care of transients as well.
 */
void Workspace::sendWindowToDesktop(Window *window, int desk, bool dont_activate)
{
    if ((desk < 1 && desk != NET::OnAllDesktops) || desk > static_cast<int>(VirtualDesktopManager::self()->count())) {
        return;
    }
    int old_desktop = window->desktop();
    const bool wasOnCurrent = window->isOnCurrentDesktop();
    window->setDesktop(desk);
    if (window->desktop() != desk) { // No change or desktop forced
        return;
    }
    desk = window->desktop(); // Window did range checking

    if (window->isOnCurrentDesktop()) {
        if (window->wantsTabFocus() && options->focusPolicyIsReasonable() && !wasOnCurrent && // for stickyness changes
            !dont_activate) {
            requestFocus(window);
        } else {
            restackWindowUnderActive(window);
        }
    } else {
        raiseWindow(window);
    }

    window->checkWorkspacePosition(QRect(), VirtualDesktopManager::self()->desktopForX11Id(old_desktop));

    auto transients_stacking_order = ensureStackingOrder(window->transients());
    for (auto it = transients_stacking_order.constBegin(); it != transients_stacking_order.constEnd(); ++it) {
        sendWindowToDesktop(*it, desk, dont_activate);
    }
    updateClientArea();
}

void Workspace::sendWindowToOutput(Window *window, Output *output)
{
    window->sendToOutput(output);
}

/**
 * Delayed focus functions
 */
void Workspace::delayFocus()
{
    requestFocus(m_delayFocusWindow);
    cancelDelayFocus();
}

void Workspace::requestDelayFocus(Window *window)
{
    m_delayFocusWindow = window;
    delete delayFocusTimer;
    delayFocusTimer = new QTimer(this);
    connect(delayFocusTimer, &QTimer::timeout, this, &Workspace::delayFocus);
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
        should_get_focus.clear();
        m_nullFocus->focus();
    }
}

void Workspace::setShowingDesktop(bool showing, bool animated)
{
    const bool changed = showing != showing_desktop;
    if (rootInfo() && changed) {
        rootInfo()->setShowingDesktop(showing);
    }
    showing_desktop = showing;

    Window *topDesk = nullptr;

    { // for the blocker RAII
        StackingUpdatesBlocker blocker(this); // updateLayer & lowerWindow would invalidate stacking_order
        for (int i = stacking_order.count() - 1; i > -1; --i) {
            auto window = stacking_order.at(i);
            if (window->isClient() && window->isOnCurrentDesktop()) {
                if (window->isDock()) {
                    window->updateLayer();
                } else if (window->isDesktop() && window->isShown()) {
                    window->updateLayer();
                    lowerWindow(window);
                    if (!topDesk) {
                        topDesk = window;
                    }
                    if (auto group = window->group()) {
                        const auto members = group->members();
                        for (X11Window *cm : members) {
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
        const auto window = m_focusChain->getForActivation(VirtualDesktopManager::self()->currentDesktop());
        if (window) {
            activateWindow(window);
        }
    }
    if (changed) {
        Q_EMIT showingDesktopChanged(showing, animated);
    }
}

void Workspace::disableGlobalShortcutsForClient(bool disable)
{
    if (m_globalShortcutsDisabledForWindow == disable) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/kglobalaccel"),
                                                          QStringLiteral("org.kde.KGlobalAccel"),
                                                          QStringLiteral("blockGlobalShortcuts"));
    message.setArguments(QList<QVariant>() << disable);
    QDBusConnection::sessionBus().asyncCall(message);

    m_globalShortcutsDisabledForWindow = disable;
    // Update also Meta+LMB actions etc.
    for (auto it = m_x11Clients.constBegin(); it != m_x11Clients.constEnd(); ++it) {
        (*it)->updateMouseGrab();
    }
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
                          "like https://paste.kde.org instead of pasting into support threads.\n")
                       .toString());
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
    support.append(KWIN_BUILD_DECORATIONS ? yes : no);
    support.append(QStringLiteral("KWIN_BUILD_TABBOX: "));
    support.append(KWIN_BUILD_TABBOX ? yes : no);
    support.append(QStringLiteral("KWIN_BUILD_ACTIVITIES: "));
    support.append(KWIN_BUILD_ACTIVITIES ? yes : no);
    support.append(QStringLiteral("HAVE_X11_XCB: "));
    support.append(HAVE_X11_XCB ? yes : no);
    support.append(QStringLiteral("HAVE_EPOXY_GLX: "));
    support.append(HAVE_EPOXY_GLX ? yes : no);
    support.append(QStringLiteral("HAVE_WAYLAND_EGL: "));
    support.append(HAVE_WAYLAND_EGL ? yes : no);
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
            support.append(QStringLiteral("%1: %2; Version: 0x%3\n")
                               .arg(QString::fromUtf8(e.name), e.present ? yes.trimmed() : no.trimmed(), QString::number(e.version, 16)));
        }
        support.append(QStringLiteral("\n"));
    }

    if (m_decorationBridge) {
        support.append(QStringLiteral("Decoration\n"));
        support.append(QStringLiteral("==========\n"));
        support.append(m_decorationBridge->supportInformation());
        support.append(QStringLiteral("\n"));
    }
    support.append(QStringLiteral("Platform\n"));
    support.append(QStringLiteral("==========\n"));
    support.append(kwinApp()->platform()->supportInformation());
    support.append(QStringLiteral("\n"));

    const Cursor *cursor = Cursors::self()->mouse();
    support.append(QLatin1String("Cursor\n"));
    support.append(QLatin1String("======\n"));
    support.append(QLatin1String("themeName: ") + cursor->themeName() + QLatin1Char('\n'));
    support.append(QLatin1String("themeSize: ") + QString::number(cursor->themeSize()) + QLatin1Char('\n'));
    support.append(QLatin1Char('\n'));

    support.append(QStringLiteral("Options\n"));
    support.append(QStringLiteral("=======\n"));
    const QMetaObject *metaOptions = options->metaObject();
    auto printProperty = [](const QVariant &variant) {
        if (variant.type() == QVariant::Size) {
            const QSize &s = variant.toSize();
            return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
        }
        if (QLatin1String(variant.typeName()) == QLatin1String("KWin::OpenGLPlatformInterface") || QLatin1String(variant.typeName()) == QLatin1String("KWin::Options::WindowOperation")) {
            return QString::number(variant.toInt());
        }
        return variant.toString();
    };
    for (int i = 0; i < metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name(), printProperty(options->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreen Edges\n"));
    support.append(QStringLiteral("============\n"));
    const QMetaObject *metaScreenEdges = ScreenEdges::self()->metaObject();
    for (int i = 0; i < metaScreenEdges->propertyCount(); ++i) {
        const QMetaProperty property = metaScreenEdges->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name(), printProperty(ScreenEdges::self()->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreens\n"));
    support.append(QStringLiteral("=======\n"));
    support.append(QStringLiteral("Active screen follows mouse: "));
    if (options->activeMouseScreen()) {
        support.append(QStringLiteral(" yes\n"));
    } else {
        support.append(QStringLiteral(" no\n"));
    }
    const QVector<Output *> outputs = kwinApp()->platform()->outputs();
    support.append(QStringLiteral("Number of Screens: %1\n\n").arg(outputs.count()));
    for (int i = 0; i < outputs.count(); ++i) {
        const auto output = outputs[i];
        const QRect geo = outputs[i]->geometry();
        support.append(QStringLiteral("Screen %1:\n").arg(i));
        support.append(QStringLiteral("---------\n"));
        support.append(QStringLiteral("Name: %1\n").arg(output->name()));
        support.append(QStringLiteral("Enabled: %1\n").arg(output->isEnabled()));
        if (output->isEnabled()) {
            support.append(QStringLiteral("Geometry: %1,%2,%3x%4\n")
                               .arg(geo.x())
                               .arg(geo.y())
                               .arg(geo.width())
                               .arg(geo.height()));
            support.append(QStringLiteral("Scale: %1\n").arg(output->scale()));
            support.append(QStringLiteral("Refresh Rate: %1\n").arg(output->refreshRate()));
            QString vrr = QStringLiteral("incapable");
            if (output->capabilities() & Output::Capability::Vrr) {
                switch (output->vrrPolicy()) {
                case RenderLoop::VrrPolicy::Never:
                    vrr = QStringLiteral("never");
                    break;
                case RenderLoop::VrrPolicy::Always:
                    vrr = QStringLiteral("always");
                    break;
                case RenderLoop::VrrPolicy::Automatic:
                    vrr = QStringLiteral("automatic");
                    break;
                }
            }
            support.append(QStringLiteral("Adaptive Sync: %1\n").arg(vrr));
        }
    }
    support.append(QStringLiteral("\nCompositing\n"));
    support.append(QStringLiteral("===========\n"));
    if (effects) {
        support.append(QStringLiteral("Compositing is active\n"));
        switch (effects->compositingType()) {
        case OpenGLCompositing: {
            GLPlatform *platform = GLPlatform::instance();
            if (platform->isGLES()) {
                support.append(QStringLiteral("Compositing Type: OpenGL ES 2.0\n"));
            } else {
                support.append(QStringLiteral("Compositing Type: OpenGL\n"));
            }
            support.append(QStringLiteral("OpenGL vendor string: ") + QString::fromUtf8(platform->glVendorString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL renderer string: ") + QString::fromUtf8(platform->glRendererString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL version string: ") + QString::fromUtf8(platform->glVersionString()) + QStringLiteral("\n"));
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

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL)) {
                support.append(QStringLiteral("OpenGL shading language version string: ") + QString::fromUtf8(platform->glShadingLanguageVersionString()) + QStringLiteral("\n"));
            }

            support.append(QStringLiteral("Driver: ") + GLPlatform::driverToString(platform->driver()) + QStringLiteral("\n"));
            if (!platform->isMesaDriver()) {
                support.append(QStringLiteral("Driver version: ") + GLPlatform::versionToString(platform->driverVersion()) + QStringLiteral("\n"));
            }

            support.append(QStringLiteral("GPU class: ") + GLPlatform::chipClassToString(platform->chipClass()) + QStringLiteral("\n"));

            support.append(QStringLiteral("OpenGL version: ") + GLPlatform::versionToString(platform->glVersion()) + QStringLiteral("\n"));

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL)) {
                support.append(QStringLiteral("GLSL version: ") + GLPlatform::versionToString(platform->glslVersion()) + QStringLiteral("\n"));
            }

            if (platform->isMesaDriver()) {
                support.append(QStringLiteral("Mesa version: ") + GLPlatform::versionToString(platform->mesaVersion()) + QStringLiteral("\n"));
            }
            if (platform->serverVersion() > 0) {
                support.append(QStringLiteral("X server version: ") + GLPlatform::versionToString(platform->serverVersion()) + QStringLiteral("\n"));
            }
            if (platform->kernelVersion() > 0) {
                support.append(QStringLiteral("Linux kernel version: ") + GLPlatform::versionToString(platform->kernelVersion()) + QStringLiteral("\n"));
            }

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
            break;
        }
        case QPainterCompositing:
            support.append("Compositing Type: QPainter\n");
            break;
        case NoCompositing:
        default:
            support.append(QStringLiteral("Something is really broken, neither OpenGL nor QPainter is used"));
        }
        support.append(QStringLiteral("\nLoaded Effects:\n"));
        support.append(QStringLiteral("---------------\n"));
        const auto loadedEffects = static_cast<EffectsHandlerImpl *>(effects)->loadedEffects();
        for (const QString &effect : loadedEffects) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nCurrently Active Effects:\n"));
        support.append(QStringLiteral("-------------------------\n"));
        const auto activeEffects = static_cast<EffectsHandlerImpl *>(effects)->activeEffects();
        for (const QString &effect : activeEffects) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nEffect Settings:\n"));
        support.append(QStringLiteral("----------------\n"));
        for (const QString &effect : loadedEffects) {
            support.append(static_cast<EffectsHandlerImpl *>(effects)->supportInformation(effect));
            support.append(QStringLiteral("\n"));
        }
        support.append(QLatin1String("\nLoaded Plugins:\n"));
        support.append(QLatin1String("---------------\n"));
        QStringList loadedPlugins = kwinApp()->pluginManager()->loadedPlugins();
        loadedPlugins.sort();
        for (const QString &plugin : qAsConst(loadedPlugins)) {
            support.append(plugin + QLatin1Char('\n'));
        }
        support.append(QLatin1String("\nAvailable Plugins:\n"));
        support.append(QLatin1String("------------------\n"));
        QStringList availablePlugins = kwinApp()->pluginManager()->availablePlugins();
        availablePlugins.sort();
        for (const QString &plugin : qAsConst(availablePlugins)) {
            support.append(plugin + QLatin1Char('\n'));
        }
    } else {
        support.append(QStringLiteral("Compositing is not active\n"));
    }
    return support;
}

X11Window *Workspace::findClient(std::function<bool(const X11Window *)> func) const
{
    if (X11Window *ret = Window::findInList(m_x11Clients, func)) {
        return ret;
    }
    return nullptr;
}

Window *Workspace::findAbstractClient(std::function<bool(const Window *)> func) const
{
    if (Window *ret = Window::findInList(m_allClients, func)) {
        return ret;
    }
    if (InternalWindow *ret = Window::findInList(m_internalWindows, func)) {
        return ret;
    }
    return nullptr;
}

Unmanaged *Workspace::findUnmanaged(std::function<bool(const Unmanaged *)> func) const
{
    return Window::findInList(m_unmanaged, func);
}

Unmanaged *Workspace::findUnmanaged(xcb_window_t w) const
{
    return findUnmanaged([w](const Unmanaged *u) {
        return u->window() == w;
    });
}

X11Window *Workspace::findClient(Predicate predicate, xcb_window_t w) const
{
    switch (predicate) {
    case Predicate::WindowMatch:
        return findClient([w](const X11Window *c) {
            return c->window() == w;
        });
    case Predicate::WrapperIdMatch:
        return findClient([w](const X11Window *c) {
            return c->wrapperId() == w;
        });
    case Predicate::FrameIdMatch:
        return findClient([w](const X11Window *c) {
            return c->frameId() == w;
        });
    case Predicate::InputIdMatch:
        return findClient([w](const X11Window *c) {
            return c->inputId() == w;
        });
    }
    return nullptr;
}

Window *Workspace::findToplevel(std::function<bool(const Window *)> func) const
{
    if (auto *ret = Window::findInList(m_allClients, func)) {
        return ret;
    }
    if (Unmanaged *ret = Window::findInList(m_unmanaged, func)) {
        return ret;
    }
    if (InternalWindow *ret = Window::findInList(m_internalWindows, func)) {
        return ret;
    }
    return nullptr;
}

Window *Workspace::findToplevel(const QUuid &internalId) const
{
    return findToplevel([internalId](const KWin::Window *l) -> bool {
        return internalId == l->internalId();
    });
}

void Workspace::forEachToplevel(std::function<void(Window *)> func)
{
    std::for_each(m_allClients.constBegin(), m_allClients.constEnd(), func);
    std::for_each(deleted.constBegin(), deleted.constEnd(), func);
    std::for_each(m_unmanaged.constBegin(), m_unmanaged.constEnd(), func);
    std::for_each(m_internalWindows.constBegin(), m_internalWindows.constEnd(), func);
}

bool Workspace::hasWindow(const Window *c)
{
    return findAbstractClient([&c](const Window *test) {
               return test == c;
           })
        != nullptr;
}

void Workspace::forEachAbstractClient(std::function<void(Window *)> func)
{
    std::for_each(m_allClients.constBegin(), m_allClients.constEnd(), func);
    std::for_each(m_internalWindows.constBegin(), m_internalWindows.constEnd(), func);
}

Window *Workspace::findInternal(QWindow *w) const
{
    if (!w) {
        return nullptr;
    }
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        return findUnmanaged(w->winId());
    }
    for (InternalWindow *window : m_internalWindows) {
        if (window->handle() == w) {
            return window;
        }
    }
    return nullptr;
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
                       });
}

void Workspace::updateTabbox()
{
#if KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox->isDisplayed()) {
        tabBox->reset(true);
    }
#endif
}

void Workspace::addInternalWindow(InternalWindow *window)
{
    m_internalWindows.append(window);
    addToStack(window);

    setupWindowConnections(window);
    window->updateLayer();

    if (window->isPlaceable()) {
        const QRectF area = clientArea(PlacementArea, window, workspace()->activeOutput());
        Placement::self()->place(window, area);
    }

    updateStackingOrder(true);
    updateClientArea();

    Q_EMIT internalWindowAdded(window);
}

void Workspace::removeInternalWindow(InternalWindow *window)
{
    m_internalWindows.removeOne(window);

    updateStackingOrder();
    updateClientArea();

    Q_EMIT internalWindowRemoved(window);
}

void Workspace::setInitialDesktop(int desktop)
{
    m_initialDesktop = desktop;
}

Group *Workspace::findGroup(xcb_window_t leader) const
{
    Q_ASSERT(leader != XCB_WINDOW_NONE);
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if ((*it)->leader() == leader) {
            return *it;
        }
    }
    return nullptr;
}

// Window is group transient, but has no group set. Try to find
// group with windows with the same client leader.
Group *Workspace::findClientLeaderGroup(const X11Window *window) const
{
    Group *ret = nullptr;
    for (auto it = m_x11Clients.constBegin(); it != m_x11Clients.constEnd(); ++it) {
        if (*it == window) {
            continue;
        }
        if ((*it)->wmClientLeader() == window->wmClientLeader()) {
            if (ret == nullptr || ret == (*it)->group()) {
                ret = (*it)->group();
            } else {
                // There are already two groups with the same client leader.
                // This most probably means the app uses group transients without
                // setting group for its windows. Merging the two groups is a bad
                // hack, but there's no really good solution for this case.
                QList<X11Window *> old_group = (*it)->group()->members();
                // old_group autodeletes when being empty
                for (int pos = 0; pos < old_group.count(); ++pos) {
                    X11Window *tmp = old_group[pos];
                    if (tmp != window) {
                        tmp->changeClientLeaderGroup(ret);
                    }
                }
            }
        }
    }
    return ret;
}

void Workspace::updateMinimizedOfTransients(Window *window)
{
    // if mainwindow is minimized or shaded, minimize transients too
    if (window->isMinimized()) {
        for (auto it = window->transients().constBegin(); it != window->transients().constEnd(); ++it) {
            if ((*it)->isModal()) {
                continue; // there's no reason to hide modal dialogs with the main window
            }
            // but to keep them to eg. watch progress or whatever
            if (!(*it)->isMinimized()) {
                (*it)->minimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (window->isModal()) { // if a modal dialog is minimized, minimize its mainwindow too
            const auto windows = window->mainWindows();
            for (Window *main : qAsConst(windows)) {
                main->minimize();
            }
        }
    } else {
        // else unmiminize the transients
        for (auto it = window->transients().constBegin(); it != window->transients().constEnd(); ++it) {
            if ((*it)->isMinimized()) {
                (*it)->unminimize();
                updateMinimizedOfTransients((*it));
            }
        }
        if (window->isModal()) {
            const auto windows = window->mainWindows();
            for (Window *main : qAsConst(windows)) {
                main->unminimize();
            }
        }
    }
}

/**
 * Sets the \a window's transient windows' on_all_desktops property to \a on_all_desktops.
 */
void Workspace::updateOnAllDesktopsOfTransients(Window *window)
{
    for (auto it = window->transients().constBegin(); it != window->transients().constEnd(); ++it) {
        if ((*it)->isOnAllDesktops() != window->isOnAllDesktops()) {
            (*it)->setOnAllDesktops(window->isOnAllDesktops());
        }
    }
}

// A new window has been mapped. Check if it's not a mainwindow for some already existing transient window.
void Workspace::checkTransients(xcb_window_t w)
{
    for (auto it = m_x11Clients.constBegin(); it != m_x11Clients.constEnd(); ++it) {
        (*it)->checkTransient(w);
    }
}

/**
 * Resizes the workspace after an XRANDR screen size change
 */
void Workspace::desktopResized()
{
    const QRect oldGeometry = m_geometry;
    m_geometry = QRect();
    for (const Output *output : std::as_const(m_outputs)) {
        m_geometry = m_geometry.united(output->geometry());
    }

    if (rootInfo()) {
        NETSize desktop_geometry;
        desktop_geometry.width = Xcb::toXNative(m_geometry.width());
        desktop_geometry.height = Xcb::toXNative(m_geometry.height());
        rootInfo()->setDesktopGeometry(desktop_geometry);
    }

    updateClientArea();
    saveOldScreenSizes(); // after updateClientArea(), so that one still uses the previous one

    // TODO: emit a signal instead and remove the deep function calls into edges and effects
    ScreenEdges::self()->recreateEdges();

    if (m_geometry != oldGeometry) {
        Q_EMIT geometryChanged();
    }
}

void Workspace::saveOldScreenSizes()
{
    olddisplaysize = m_geometry.size();
    m_oldScreenGeometries.clear();

    for (const Output *output : std::as_const(m_outputs)) {
        m_oldScreenGeometries.insert(output, output->geometry());
    }
}

/**
 * Whether or not the window has a strut that expands through the invisible area of
 * an xinerama setup where the monitors are not the same resolution.
 */
static bool hasOffscreenXineramaStrut(Window *window)
{
    // Get strut as a QRegion
    QRegion region;
    region += window->strutRect(StrutAreaTop);
    region += window->strutRect(StrutAreaRight);
    region += window->strutRect(StrutAreaBottom);
    region += window->strutRect(StrutAreaLeft);

    // Remove all visible areas so that only the invisible remain
    const auto outputs = workspace()->outputs();
    for (const Output *output : outputs) {
        region -= output->geometry();
    }

    // If there's anything left then we have an offscreen strut
    return !region.isEmpty();
}

QRectF Workspace::adjustClientArea(Window *window, const QRectF &area) const
{
    QRectF adjustedArea = area;

    QRectF strutLeft = window->strutRect(StrutAreaLeft);
    QRectF strutRight = window->strutRect(StrutAreaRight);
    QRectF strutTop = window->strutRect(StrutAreaTop);
    QRectF strutBottom = window->strutRect(StrutAreaBottom);

    QRectF screenArea = clientArea(ScreenArea, window);
    // HACK: workarea handling is not xinerama aware, so if this strut
    // reserves place at a xinerama edge that's inside the virtual screen,
    // ignore the strut for workspace setting.
    if (area == QRect(QPoint(0, 0), m_geometry.size())) {
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
        adjustedArea.setLeft(strutLeft.right());
    }
    if (strutRight.intersects(area)) {
        adjustedArea.setRight(strutRight.left());
    }
    if (strutTop.intersects(area)) {
        adjustedArea.setTop(strutTop.bottom());
    }
    if (strutBottom.intersects(area)) {
        adjustedArea.setBottom(strutBottom.top());
    }

    return adjustedArea;
}

/**
 * Updates the current client areas according to the current windows.
 *
 * The client area is the area that is available for windows (that
 * which is not taken by windows like panels, the top-of-screen menu
 * etc).
 *
 * @see clientArea()
 */
void Workspace::updateClientArea()
{
    const QVector<VirtualDesktop *> desktops = VirtualDesktopManager::self()->desktops();

    QHash<const VirtualDesktop *, QRectF> workAreas;
    QHash<const VirtualDesktop *, StrutRects> restrictedAreas;
    QHash<const VirtualDesktop *, QHash<const Output *, QRectF>> screenAreas;

    for (const VirtualDesktop *desktop : desktops) {
        workAreas[desktop] = m_geometry;

        for (const Output *output : std::as_const(m_outputs)) {
            screenAreas[desktop][output] = output->geometry();
        }
    }

    for (Window *window : qAsConst(m_allClients)) {
        if (!window->hasStrut()) {
            continue;
        }
        QRectF r = adjustClientArea(window, m_geometry);

        // This happens sometimes when the workspace size changes and the
        // struted windows haven't repositioned yet
        if (!r.isValid()) {
            continue;
        }
        // sanity check that a strut doesn't exclude a complete screen geometry
        // this is a violation to EWMH, as KWin just ignores the strut
        for (const Output *output : std::as_const(m_outputs)) {
            if (!r.intersects(output->geometry())) {
                qCDebug(KWIN_CORE) << "Adjusted client area would exclude a complete screen, ignore";
                r = m_geometry;
                break;
            }
        }
        StrutRects strutRegion = window->strutRects();
        const QRect clientsScreenRect = window->output()->geometry();
        for (auto strut = strutRegion.begin(); strut != strutRegion.end(); strut++) {
            *strut = StrutRect((*strut).intersected(clientsScreenRect), (*strut).area());
        }

        // Ignore offscreen xinerama struts. These interfere with the larger monitors on the setup
        // and should be ignored so that applications that use the work area to work out where
        // windows can go can use the entire visible area of the larger monitors.
        // This goes against the EWMH description of the work area but it is a toss up between
        // having unusable sections of the screen (Which can be quite large with newer monitors)
        // or having some content appear offscreen (Relatively rare compared to other).
        bool hasOffscreenStrut = hasOffscreenXineramaStrut(window);

        const auto vds = window->isOnAllDesktops() ? desktops : window->desktops();
        for (VirtualDesktop *vd : vds) {
            if (!hasOffscreenStrut) {
                workAreas[vd] &= r;
            }
            restrictedAreas[vd] += strutRegion;
            for (Output *output : std::as_const(m_outputs)) {
                const auto geo = screenAreas[vd][output].intersected(adjustClientArea(window, output->geometry()));
                // ignore the geometry if it results in the screen getting removed completely
                if (!geo.isEmpty()) {
                    screenAreas[vd][output] = geo;
                }
            }
        }
    }

    if (m_workAreas != workAreas || m_restrictedAreas != restrictedAreas || m_screenAreas != screenAreas) {
        m_workAreas = workAreas;
        m_screenAreas = screenAreas;

        m_inUpdateClientArea = true;
        m_oldRestrictedAreas = m_restrictedAreas;
        m_restrictedAreas = restrictedAreas;

        if (rootInfo()) {
            for (VirtualDesktop *desktop : desktops) {
                const QRectF &workArea = m_workAreas[desktop];
                NETRect r(Xcb::toXNative(workArea));
                rootInfo()->setWorkArea(desktop->x11DesktopNumber(), r);
            }
        }

        for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
            (*it)->checkWorkspacePosition();
        }

        m_oldRestrictedAreas.clear(); // reset, no longer valid or needed
        m_inUpdateClientArea = false;
    }
}

/**
 * Returns the area available for windows. This is the desktop
 * geometry minus windows on the dock. Placement algorithms should
 * refer to this rather than Screens::geometry.
 */
QRectF Workspace::clientArea(clientAreaOption opt, const Output *output, const VirtualDesktop *desktop) const
{
    switch (opt) {
    case MaximizeArea:
    case PlacementArea:
        if (auto desktopIt = m_screenAreas.constFind(desktop); desktopIt != m_screenAreas.constEnd()) {
            if (auto outputIt = desktopIt->constFind(output); outputIt != desktopIt->constEnd()) {
                return *outputIt;
            }
        }
        return output->geometry();
    case MaximizeFullArea:
    case FullScreenArea:
    case MovementArea:
    case ScreenArea:
        return output->geometry();
    case WorkArea:
        return m_workAreas.value(desktop, m_geometry);
    case FullArea:
        return m_geometry;
    default:
        Q_UNREACHABLE();
    }
}

QRectF Workspace::clientArea(clientAreaOption opt, const Window *window) const
{
    return clientArea(opt, window, window->output());
}

QRectF Workspace::clientArea(clientAreaOption opt, const Window *window, const Output *output) const
{
    const VirtualDesktop *desktop;
    if (window->isOnCurrentDesktop()) {
        desktop = VirtualDesktopManager::self()->currentDesktop();
    } else {
        desktop = window->desktops().constLast();
    }

    return clientArea(opt, output, desktop);
}

QRectF Workspace::clientArea(clientAreaOption opt, const Window *window, const QPointF &pos) const
{
    return clientArea(opt, window, outputAt(pos));
}

QRect Workspace::geometry() const
{
    return m_geometry;
}

static QRegion strutsToRegion(StrutAreas areas, const StrutRects &strut)
{
    QRegion region;
    for (const StrutRect &rect : strut) {
        if (areas & rect.area()) {
            region += rect;
        }
    }
    return region;
}

QRegion Workspace::restrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas) const
{
    return strutsToRegion(areas, m_restrictedAreas[desktop]);
}

bool Workspace::inUpdateClientArea() const
{
    return m_inUpdateClientArea;
}

QRegion Workspace::previousRestrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas) const
{
    return strutsToRegion(areas, m_oldRestrictedAreas[desktop]);
}

QHash<const Output *, QRect> Workspace::previousScreenSizes() const
{
    return m_oldScreenGeometries;
}

int Workspace::oldDisplayWidth() const
{
    return olddisplaysize.width();
}

int Workspace::oldDisplayHeight() const
{
    return olddisplaysize.height();
}

Output *Workspace::xineramaIndexToOutput(int index) const
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return nullptr;
    }

    const ScopedCPointer<xcb_xinerama_is_active_reply_t> active{xcb_xinerama_is_active_reply(connection, xcb_xinerama_is_active(connection), nullptr)};
    if (!active || !active->state) {
        return nullptr;
    }

    const ScopedCPointer<xcb_xinerama_query_screens_reply_t> screens(xcb_xinerama_query_screens_reply(connection, xcb_xinerama_query_screens(connection), nullptr));
    if (!screens) {
        return nullptr;
    }

    const int infoCount = xcb_xinerama_query_screens_screen_info_length(screens.data());
    if (index >= infoCount) {
        return nullptr;
    }

    const xcb_xinerama_screen_info_t *infos = xcb_xinerama_query_screens_screen_info(screens.data());
    const QRect needle(infos[index].x_org, infos[index].y_org, infos[index].width, infos[index].height);

    for (Output *output : std::as_const(m_outputs)) {
        if (Xcb::toXNative(output->geometry()) == needle) {
            return output;
        }
    }

    return nullptr;
}

Output *Workspace::activeOutput() const
{
    if (options->activeMouseScreen()) {
        return outputAt(Cursors::self()->mouse()->pos());
    }

    if (m_activeWindow && !m_activeWindow->isOnOutput(m_activeOutput)) {
        return m_activeWindow->output();
    }

    return m_activeOutput;
}

void Workspace::setActiveOutput(Output *output)
{
    m_activeOutput = output;
}

void Workspace::setActiveOutput(const QPointF &pos)
{
    setActiveOutput(outputAt(pos));
}

/**
 * \a window is moved around to position \a pos. This gives the
 * workspace the opportunity to interveniate and to implement
 * snap-to-windows functionality.
 *
 * The parameter \a snapAdjust is a multiplier used to calculate the
 * effective snap zones. When 1.0, it means that the snap zones will be
 * used without change.
 */
QPointF Workspace::adjustWindowPosition(Window *window, QPointF pos, bool unrestricted, double snapAdjust)
{
    QSizeF borderSnapZone(options->borderSnapZone(), options->borderSnapZone());
    QRectF maxRect;
    int guideMaximized = MaximizeRestore;
    if (window->maximizeMode() != MaximizeRestore) {
        maxRect = clientArea(MaximizeArea, window, pos + window->rect().center());
        QRectF geo = window->frameGeometry();
        if (window->maximizeMode() & MaximizeHorizontal && (geo.x() == maxRect.left() || geo.right() == maxRect.right())) {
            guideMaximized |= MaximizeHorizontal;
            borderSnapZone.setWidth(qMax(borderSnapZone.width() + 2, maxRect.width() / 16));
        }
        if (window->maximizeMode() & MaximizeVertical && (geo.y() == maxRect.top() || geo.bottom() == maxRect.bottom())) {
            guideMaximized |= MaximizeVertical;
            borderSnapZone.setHeight(qMax(borderSnapZone.height() + 2, maxRect.height() / 16));
        }
    }

    if (options->windowSnapZone() || !borderSnapZone.isNull() || options->centerSnapZone()) {

        const bool snappingToCenter = (options->centerSnapZone() * snapAdjust);
        const bool sOWO = options->isSnapOnlyWhenOverlapping();
        const Output *output = outputAt(pos + window->rect().center());
        if (maxRect.isNull()) {
            maxRect = clientArea(MaximizeArea, window, output);
        }
        const int xmin = maxRect.left();
        const int xmax = maxRect.right(); // desk size
        const int ymin = maxRect.top();
        const int ymax = maxRect.bottom();

        const int cx(pos.x());
        const int cy(pos.y());
        const int cw(window->width());
        const int ch(window->height());
        const int rx(cx + cw);
        const int ry(cy + ch); // these don't change

        int nx(cx), ny(cy); // buffers
        int deltaX(xmax);
        int deltaY(ymax); // minimum distance to other windows

        int lx, ly, lrx, lry; // coords and size for the comparison window, l

        // border snap
        const int snapX = borderSnapZone.width() * snapAdjust; // snap trigger
        const int snapY = borderSnapZone.height() * snapAdjust;
        if (snapX || snapY) {
            if ((sOWO ? (cx < xmin) : true) && (qAbs(xmin - cx) < snapX)) {
                deltaX = xmin - cx;
                nx = xmin;
            }
            if ((sOWO ? (rx > xmax) : true) && (qAbs(rx - xmax) < snapX) && (qAbs(xmax - rx) < deltaX)) {
                deltaX = rx - xmax;
                nx = xmax - cw;
            }

            if ((sOWO ? (cy < ymin) : true) && (qAbs(ymin - cy) < snapY)) {
                deltaY = ymin - cy;
                ny = ymin;
            }
            if ((sOWO ? (ry > ymax) : true) && (qAbs(ry - ymax) < snapY) && (qAbs(ymax - ry) < deltaY)) {
                deltaY = ry - ymax;
                ny = ymax - ch;
            }
        }

        // windows snap
        int snap = options->windowSnapZone() * snapAdjust;
        if (snap) {
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l) == window) {
                    continue;
                }
                if ((*l)->isMinimized() || (*l)->isShade()) {
                    continue;
                }
                if (!(*l)->isShown()) {
                    continue;
                }
                if (!(*l)->isOnCurrentDesktop()) {
                    continue; // wrong virtual desktop
                }
                if (!(*l)->isOnCurrentActivity()) {
                    continue; // wrong activity
                }
                if ((*l)->isDesktop() || (*l)->isSplash() || (*l)->isNotification() || (*l)->isCriticalNotification() || (*l)->isOnScreenDisplay() || (*l)->isAppletPopup()) {
                    continue;
                }

                lx = (*l)->x();
                ly = (*l)->y();
                lrx = lx + (*l)->width();
                lry = ly + (*l)->height();

                if (!(guideMaximized & MaximizeHorizontal) && (((cy <= lry) && (cy >= ly)) || ((ry >= ly) && (ry <= lry)) || ((cy <= ly) && (ry >= lry)))) {
                    if ((sOWO ? (cx < lrx) : true) && (qAbs(lrx - cx) < snap) && (qAbs(lrx - cx) < deltaX)) {
                        deltaX = qAbs(lrx - cx);
                        nx = lrx;
                    }
                    if ((sOWO ? (rx > lx) : true) && (qAbs(rx - lx) < snap) && (qAbs(rx - lx) < deltaX)) {
                        deltaX = qAbs(rx - lx);
                        nx = lx - cw;
                    }
                }

                if (!(guideMaximized & MaximizeVertical) && (((cx <= lrx) && (cx >= lx)) || ((rx >= lx) && (rx <= lrx)) || ((cx <= lx) && (rx >= lrx)))) {
                    if ((sOWO ? (cy < lry) : true) && (qAbs(lry - cy) < snap) && (qAbs(lry - cy) < deltaY)) {
                        deltaY = qAbs(lry - cy);
                        ny = lry;
                    }
                    // if ( (qAbs( ry-ly ) < snap) && (qAbs( ry - ly ) < deltaY ))
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
        if (snappingToCenter) { // snap trigger
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
                } else if (((unrestricted ? ny == ymin : ny <= ymin) || ny == ymax - ch) && diffX < snap && diffX < deltaX) {
                    // Snap to horizontal center on screen edge
                    nx = (xmin + xmax) / 2 - cw / 2;
                }
            }
        }

        pos = QPoint(nx, ny);
    }
    return pos;
}

QRectF Workspace::adjustWindowSize(Window *window, QRectF moveResizeGeom, Gravity gravity)
{
    // adapted from adjustWindowPosition on 29May2004
    // this function is called when resizing a window and will modify
    // the new dimensions to snap to other windows/borders if appropriate
    if (options->windowSnapZone() || options->borderSnapZone()) { // || options->centerSnapZone )
        const bool sOWO = options->isSnapOnlyWhenOverlapping();

        const QRectF maxRect = clientArea(MovementArea, window, window->rect().center());
        const qreal xmin = maxRect.left();
        const qreal xmax = maxRect.right(); // desk size
        const qreal ymin = maxRect.top();
        const qreal ymax = maxRect.bottom();

        const qreal cx(moveResizeGeom.left());
        const qreal cy(moveResizeGeom.top());
        const qreal rx(moveResizeGeom.right());
        const qreal ry(moveResizeGeom.bottom());

        qreal newcx(cx), newcy(cy); // buffers
        qreal newrx(rx), newry(ry);
        qreal deltaX(xmax);
        qreal deltaY(ymax); // minimum distance to other windows

        qreal lx, ly, lrx, lry; // coords and size for the comparison window, l

        // border snap
        int snap = options->borderSnapZone(); // snap trigger
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);

#define SNAP_BORDER_TOP                                                    \
    if ((sOWO ? (newcy < ymin) : true) && (qAbs(ymin - newcy) < deltaY)) { \
        deltaY = qAbs(ymin - newcy);                                       \
        newcy = ymin;                                                      \
    }

#define SNAP_BORDER_BOTTOM                                                 \
    if ((sOWO ? (newry > ymax) : true) && (qAbs(ymax - newry) < deltaY)) { \
        deltaY = qAbs(ymax - newcy);                                       \
        newry = ymax;                                                      \
    }

#define SNAP_BORDER_LEFT                                                   \
    if ((sOWO ? (newcx < xmin) : true) && (qAbs(xmin - newcx) < deltaX)) { \
        deltaX = qAbs(xmin - newcx);                                       \
        newcx = xmin;                                                      \
    }

#define SNAP_BORDER_RIGHT                                                  \
    if ((sOWO ? (newrx > xmax) : true) && (qAbs(xmax - newrx) < deltaX)) { \
        deltaX = qAbs(xmax - newrx);                                       \
        newrx = xmax;                                                      \
    }
            switch (gravity) {
            case Gravity::BottomRight:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_RIGHT
                break;
            case Gravity::Right:
                SNAP_BORDER_RIGHT
                break;
            case Gravity::Bottom:
                SNAP_BORDER_BOTTOM
                break;
            case Gravity::TopLeft:
                SNAP_BORDER_TOP
                SNAP_BORDER_LEFT
                break;
            case Gravity::Left:
                SNAP_BORDER_LEFT
                break;
            case Gravity::Top:
                SNAP_BORDER_TOP
                break;
            case Gravity::TopRight:
                SNAP_BORDER_TOP
                SNAP_BORDER_RIGHT
                break;
            case Gravity::BottomLeft:
                SNAP_BORDER_BOTTOM
                SNAP_BORDER_LEFT
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
        }

        // windows snap
        snap = options->windowSnapZone();
        if (snap) {
            deltaX = int(snap);
            deltaY = int(snap);
            for (auto l = m_allClients.constBegin(); l != m_allClients.constEnd(); ++l) {
                if ((*l)->isOnCurrentDesktop() && !(*l)->isMinimized()
                    && (*l) != window) {
                    lx = (*l)->x();
                    ly = (*l)->y();
                    lrx = (*l)->x() + (*l)->width();
                    lry = (*l)->y() + (*l)->height();

#define WITHIN_HEIGHT (((newcy <= lry) && (newcy >= ly)) || ((newry >= ly) && (newry <= lry)) || ((newcy <= ly) && (newry >= lry)))

#define WITHIN_WIDTH (((cx <= lrx) && (cx >= lx)) || ((rx >= lx) && (rx <= lrx)) || ((cx <= lx) && (rx >= lrx)))

#define SNAP_WINDOW_TOP                    \
    if ((sOWO ? (newcy < lry) : true)      \
        && WITHIN_WIDTH                    \
        && (qAbs(lry - newcy) < deltaY)) { \
        deltaY = qAbs(lry - newcy);        \
        newcy = lry;                       \
    }

#define SNAP_WINDOW_BOTTOM                \
    if ((sOWO ? (newry > ly) : true)      \
        && WITHIN_WIDTH                   \
        && (qAbs(ly - newry) < deltaY)) { \
        deltaY = qAbs(ly - newry);        \
        newry = ly;                       \
    }

#define SNAP_WINDOW_LEFT                   \
    if ((sOWO ? (newcx < lrx) : true)      \
        && WITHIN_HEIGHT                   \
        && (qAbs(lrx - newcx) < deltaX)) { \
        deltaX = qAbs(lrx - newcx);        \
        newcx = lrx;                       \
    }

#define SNAP_WINDOW_RIGHT                 \
    if ((sOWO ? (newrx > lx) : true)      \
        && WITHIN_HEIGHT                  \
        && (qAbs(lx - newrx) < deltaX)) { \
        deltaX = qAbs(lx - newrx);        \
        newrx = lx;                       \
    }

#define SNAP_WINDOW_C_TOP                \
    if ((sOWO ? (newcy < ly) : true)     \
        && (newcx == lrx || newrx == lx) \
        && qAbs(ly - newcy) < deltaY) {  \
        deltaY = qAbs(ly - newcy);       \
        newcy = ly;                      \
    }

#define SNAP_WINDOW_C_BOTTOM             \
    if ((sOWO ? (newry > lry) : true)    \
        && (newcx == lrx || newrx == lx) \
        && qAbs(lry - newry) < deltaY) { \
        deltaY = qAbs(lry - newry);      \
        newry = lry;                     \
    }

#define SNAP_WINDOW_C_LEFT               \
    if ((sOWO ? (newcx < lx) : true)     \
        && (newcy == lry || newry == ly) \
        && qAbs(lx - newcx) < deltaX) {  \
        deltaX = qAbs(lx - newcx);       \
        newcx = lx;                      \
    }

#define SNAP_WINDOW_C_RIGHT              \
    if ((sOWO ? (newrx > lrx) : true)    \
        && (newcy == lry || newry == ly) \
        && qAbs(lrx - newrx) < deltaX) { \
        deltaX = qAbs(lrx - newrx);      \
        newrx = lrx;                     \
    }

                    switch (gravity) {
                    case Gravity::BottomRight:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Gravity::Right:
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Gravity::Bottom:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_C_BOTTOM
                        break;
                    case Gravity::TopLeft:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_LEFT
                        break;
                    case Gravity::Left:
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_LEFT
                        break;
                    case Gravity::Top:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_C_TOP
                        break;
                    case Gravity::TopRight:
                        SNAP_WINDOW_TOP
                        SNAP_WINDOW_RIGHT
                        SNAP_WINDOW_C_TOP
                        SNAP_WINDOW_C_RIGHT
                        break;
                    case Gravity::BottomLeft:
                        SNAP_WINDOW_BOTTOM
                        SNAP_WINDOW_LEFT
                        SNAP_WINDOW_C_BOTTOM
                        SNAP_WINDOW_C_LEFT
                        break;
                    default:
                        Q_UNREACHABLE();
                        break;
                    }
                }
            }
        }

        // center snap
        // snap = options->centerSnapZone;
        // if (snap)
        //    {
        //    // Don't resize snap to center as it interferes too much
        //    // There are two ways of implementing this if wanted:
        //    // 1) Snap only to the same points that the move snap does, and
        //    // 2) Snap to the horizontal and vertical center lines of the screen
        //    }

        moveResizeGeom = QRectF(QPointF(newcx, newcy), QPointF(newrx, newry));
    }
    return moveResizeGeom;
}

/**
 * Marks the window as being moved or resized by the user.
 */
void Workspace::setMoveResizeWindow(Window *window)
{
    Q_ASSERT(!window || !m_moveResizeWindow); // Catch attempts to move a second
    // window while still moving the first one.
    m_moveResizeWindow = window;
    if (m_moveResizeWindow) {
        ++block_focus;
    } else {
        --block_focus;
    }
}

// When kwin crashes, windows will not be gravitated back to their original position
// and will remain offset by the size of the decoration. So when restarting, fix this
// (the property with the size of the frame remains on the window after the crash).
void Workspace::fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geometry)
{
    NETWinInfo i(kwinApp()->x11Connection(), w, kwinApp()->x11RootWindow(), NET::WMFrameExtents, NET::Properties2());
    NETStrut frame = i.frameExtents();

    if (frame.left != 0 || frame.top != 0) {
        // left and top needed due to narrowing conversations restrictions in C++11
        const uint32_t left = frame.left;
        const uint32_t top = frame.top;
        const uint32_t values[] = {Xcb::toXNative(geometry->x - left), Xcb::toXNative(geometry->y - top)};
        xcb_configure_window(kwinApp()->x11Connection(), w, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }
}

FocusChain *Workspace::focusChain() const
{
    return m_focusChain.get();
}

ApplicationMenu *Workspace::applicationMenu() const
{
    return m_applicationMenu.get();
}

Decoration::DecorationBridge *Workspace::decorationBridge() const
{
    return m_decorationBridge.get();
}

#if KWIN_BUILD_ACTIVITIES
Activities *Workspace::activities() const
{
    return m_activities.get();
}
#endif

} // namespace
