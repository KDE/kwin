/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "workspace.h"
// kwin libs
#include "opengl/glplatform.h"
// kwin
#include "core/output.h"
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "cursor.h"
#include "dbusinterface.h"
#include "effect/effecthandler.h"
#include "focuschain.h"
#include "input.h"
#include "internalwindow.h"
#include "killwindow.h"
#include "outline.h"
#include "placement.h"
#include "pluginmanager.h"
#include "pointer_input.h"
#include "rules.h"
#include "screenedge.h"
#include "scripting/scripting.h"
#include "syncalarmx11filter.h"
#include "tiles/tilemanager.h"
#include "window.h"
#if KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "compositor.h"
#include "core/session.h"
#include "decorations/decorationbridge.h"
#include "dpmsinputeventfilter.h"
#include "lidswitchtracker.h"
#include "main.h"
#include "opengl/eglcontext.h"
#include "outputconfigurationstore.h"
#include "placeholderinputeventfilter.h"
#include "placeholderoutput.h"
#include "placementtracker.h"
#include "pointer_input.h"
#include "scene/workspacescene.h"
#include "tabletmodemanager.h"
#include "tiles/tilemanager.h"
#include "useractions.h"
#include "utils/envvar.h"
#include "utils/kernel.h"
#include "utils/orientationsensor.h"
#include "virtualdesktops.h"
#include "wayland/externalbrightness_v1.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#if KWIN_BUILD_X11
#include "atoms.h"
#include "core/brightnessdevice.h"
#include "group.h"
#include "netinfo.h"
#include "utils/xcbutils.h"
#include "x11window.h"
#include <KStartupInfo>
#endif
// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QMetaProperty>
#include <ranges>

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

Workspace *Workspace::_self = nullptr;

Workspace::Workspace()
    : QObject(nullptr)
    // Unsorted
    , active_popup(nullptr)
    , m_activePopupWindow(nullptr)
    , m_activeWindow(nullptr)
    , m_lastActiveWindow(nullptr)
    , m_moveResizeWindow(nullptr)
    , m_delayFocusWindow(nullptr)
    , showing_desktop(false)
    , was_user_interaction(false)
    , block_focus(0)
    , m_userActionsMenu(new UserActionsMenu(this))
    , m_sessionManager(new SessionManager(this))
    , m_focusChain(std::make_unique<FocusChain>())
    , m_applicationMenu(std::make_unique<ApplicationMenu>())
    , m_placementTracker(std::make_unique<PlacementTracker>(this))
    , m_lidSwitchTracker(std::make_unique<LidSwitchTracker>())
    , m_orientationSensor(std::make_unique<OrientationSensor>())
{
    _self = this;

#if KWIN_BUILD_ACTIVITIES
    if (kwinApp()->usesKActivities()) {
        m_activities = std::make_unique<Activities>();
    }
    if (m_activities) {
        connect(m_activities.get(), &Activities::currentChanged, this, &Workspace::updateCurrentActivity);
    }
#endif

    delayFocusTimer = nullptr;

    m_rulebook = std::make_unique<RuleBook>();
    m_rulebook->load();

    m_screenEdges = std::make_unique<ScreenEdges>();

    // VirtualDesktopManager needs to be created prior to init shortcuts
    // and prior to TabBox, due to TabBox connecting to signals
    // actual initialization happens in init()
    VirtualDesktopManager::create(this);
    // dbus interface
    new VirtualDesktopManagerDBusInterface(VirtualDesktopManager::self());

#if KWIN_BUILD_ACTIVITIES
    if (m_activities) {
        connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, this, [this](VirtualDesktop *previous, VirtualDesktop *current) {
            m_activities->notifyCurrentDesktopChanged(current);
        });
    }
#endif

#if KWIN_BUILD_TABBOX
    // need to create the tabbox before compositing scene is setup
    m_tabbox = std::make_unique<TabBox::TabBox>();
#endif

    m_decorationBridge = std::make_unique<Decoration::DecorationBridge>();
    m_decorationBridge->init();
    connect(this, &Workspace::configChanged, m_decorationBridge.get(), &Decoration::DecorationBridge::reconfigure);

    new DBusInterface(this);
    m_outline = std::make_unique<Outline>();

    m_dpmsTimer.setSingleShot(true);
    connect(&m_dpmsTimer, &QTimer::timeout, this, [this]() {
        m_dpms = DpmsState::Off;
        // applyOutputConfiguration sets the correct value
        OutputConfiguration cfg;
        applyOutputConfiguration(cfg);
        // NOTE this assumes that the displays are turned off immediately in applyOutputConfiguration
        m_sleepInhibitor.reset();
    });

    initShortcuts();

    init();
}

void Workspace::init()
{
    KSharedConfigPtr config = kwinApp()->config();
    m_screenEdges->setConfig(config);
    m_screenEdges->init();
    connect(options, &Options::configChanged, m_screenEdges.get(), &ScreenEdges::reconfigure);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::layoutChanged, m_screenEdges.get(), &ScreenEdges::updateLayout);
    connect(this, &Workspace::windowActivated, m_screenEdges.get(), &ScreenEdges::checkBlocking);

    connect(this, &Workspace::windowRemoved, m_focusChain.get(), &FocusChain::remove);
    connect(this, &Workspace::windowActivated, m_focusChain.get(), &FocusChain::setActiveWindow);
    connect(VirtualDesktopManager::self(), &VirtualDesktopManager::currentChanged, m_focusChain.get(), [this]() {
        m_focusChain->setCurrentDesktop(VirtualDesktopManager::self()->currentDesktop());
    });
    connect(options, &Options::separateScreenFocusChanged, m_focusChain.get(), &FocusChain::setSeparateScreenFocus);
    m_focusChain->setSeparateScreenFocus(options->isSeparateScreenFocus());

    // create VirtualDesktopManager and perform dependency injection
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, &VirtualDesktopManager::desktopAdded, this, &Workspace::slotDesktopAdded);
    connect(vds, &VirtualDesktopManager::desktopRemoved, this, &Workspace::slotDesktopRemoved);
    connect(vds, &VirtualDesktopManager::currentChanged, this, &Workspace::slotCurrentDesktopChanged);
    connect(vds, &VirtualDesktopManager::currentChanging, this, &Workspace::slotCurrentDesktopChanging);
    connect(vds, &VirtualDesktopManager::currentChangingCancelled, this, &Workspace::slotCurrentDesktopChangingCancelled);
    vds->setNavigationWrappingAround(options->isRollOverDesktops());
    connect(options, &Options::rollOverDesktopsChanged, vds, &VirtualDesktopManager::setNavigationWrappingAround);
    vds->setConfig(config);

    // Now we know how many desktops we'll have, thus we initialize the positioning object
    m_placement = std::make_unique<Placement>();

    // positioning object needs to be created before the virtual desktops are loaded.
    vds->load();
    vds->updateLayout();
    // makes sure any autogenerated id is saved, necessary as in case of xwayland, load will be called 2 times
    //  load is needed to be called again when starting xwayalnd to sync to RootInfo, see BUG 385260
    vds->save();

    m_outputConfigStore = std::make_unique<OutputConfigurationStore>();

    const auto applySensorChanges = [this]() {
        m_orientationSensor->setEnabled(m_outputConfigStore->isAutoRotateActive(kwinApp()->outputBackend()->outputs(), kwinApp()->tabletModeManager()->effectiveTabletMode()));
        auto opt = m_outputConfigStore->queryConfig(kwinApp()->outputBackend()->outputs(), m_lidSwitchTracker->isLidClosed(), m_orientationSensor->reading(), kwinApp()->tabletModeManager()->effectiveTabletMode());
        if (opt) {
            auto &[config, type] = *opt;
            applyOutputConfiguration(config);
        }
    };
    connect(m_lidSwitchTracker.get(), &LidSwitchTracker::lidStateChanged, this, applySensorChanges);
    // NOTE that enabling or disabling the orientation sensor can trigger the orientation to change immediately.
    // As we do enable or disable it in applySensorChanges, it must be done asynchronously / with a queued connection!
    connect(m_orientationSensor.get(), &OrientationSensor::orientationChanged, this, applySensorChanges, Qt::QueuedConnection);
    connect(kwinApp()->tabletModeManager(), &TabletModeManager::tabletModeChanged, this, applySensorChanges);
    m_orientationSensor->setEnabled(m_outputConfigStore->isAutoRotateActive(kwinApp()->outputBackend()->outputs(), kwinApp()->tabletModeManager()->effectiveTabletMode()));
    connect(m_orientationSensor.get(), &OrientationSensor::availableChanged, this, [this]() {
        const auto outputs = kwinApp()->outputBackend()->outputs();
        for (const auto output : outputs) {
            output->setAutoRotateAvailable(m_orientationSensor->isAvailable());
        }
    });

    slotOutputBackendOutputsQueried();
    connect(kwinApp()->outputBackend(), &OutputBackend::outputsQueried, this, &Workspace::slotOutputBackendOutputsQueried);

    reconfigureTimer.setSingleShot(true);
    m_rearrangeTimer.setSingleShot(true);

    connect(&reconfigureTimer, &QTimer::timeout, this, &Workspace::slotReconfigure);
    connect(&m_rearrangeTimer, &QTimer::timeout, this, &Workspace::rearrange);

    // TODO: do we really need to reconfigure everything when fonts change?
    // maybe just reconfigure the decorations? Move this into libkdecoration?
    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/KDEPlatformTheme"),
                                          QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"),
                                          this, SLOT(reconfigure()));

    m_activeWindow = nullptr;

#if KWIN_BUILD_X11
    // We want to have some xcb connection while tearing down X11 components. We don't really
    // care if the xcb connection is broken or has an error.
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &Workspace::initializeX11);
    connect(kwinApp(), &Application::x11ConnectionAboutToBeDestroyed, this, &Workspace::cleanupX11);
    initializeX11();
#endif

    Scripting::create(this);

    connect(waylandServer(), &WaylandServer::windowAdded, this, &Workspace::addWaylandWindow);
    connect(waylandServer(), &WaylandServer::windowRemoved, this, &Workspace::removeWaylandWindow);

    // broadcast that Workspace is ready, but first process all events.
    QMetaObject::invokeMethod(this, &Workspace::workspaceInitialized, Qt::QueuedConnection);

    // TODO: ungrabXServer()

    connect(this, &Workspace::windowAdded, m_placementTracker.get(), &PlacementTracker::add);
    connect(this, &Workspace::windowRemoved, m_placementTracker.get(), &PlacementTracker::remove);
    m_placementTracker->init(outputLayoutId());

    connect(waylandServer()->externalBrightness(), &ExternalBrightnessV1::devicesChanged, this, &Workspace::updateOutputConfiguration);

    m_kdeglobalsWatcher = KConfigWatcher::create(kwinApp()->kdeglobals());
    connect(m_kdeglobalsWatcher.get(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
        if (group.name() == "KScreen" && names.contains(QByteArrayLiteral("XwaylandClientsScale"))) {
            updateXwaylandScale();
        }
    });

#if KWIN_BUILD_SCREENLOCKER
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::locked, this, &Workspace::slotEndInteractiveMoveResize);
#endif
}

QString Workspace::outputLayoutId() const
{
    QStringList hashes;
    for (const auto &output : std::as_const(m_outputs)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (output->backendOutput()->edid().isValid()) {
            hash.addData(output->backendOutput()->edid().raw());
        } else {
            hash.addData(output->name().toLatin1());
        }
        const auto geometry = output->geometry();
        hash.addData(reinterpret_cast<const char *>(&geometry), sizeof(geometry));
        hashes.push_back(QString::fromLatin1(hash.result().toHex()));
    }
    std::sort(hashes.begin(), hashes.end());
    const auto hash = QCryptographicHash::hash(hashes.join(QString()).toLatin1(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

#if KWIN_BUILD_X11
void Workspace::initializeX11()
{
    if (!kwinApp()->x11Connection()) {
        return;
    }

    atoms->retrieveHelpers();

    // first initialize the extensions
    Xcb::Extensions::self();

    // Call this before XSelectInput() on the root window
    m_startup = std::make_unique<KStartupInfo>(KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this);

    // Select windowmanager privileges
    selectWmInputEventMask();

    if (Xcb::Extensions::self()->isSyncAvailable()) {
        m_syncAlarmFilter = std::make_unique<SyncAlarmX11Filter>();
    }

    const uint32_t nullFocusValues[] = {true};
    m_nullFocus = std::make_unique<Xcb::Window>(QRect(-1, -1, 1, 1), XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, nullFocusValues);
    m_nullFocus->map();

    RootInfo *rootInfo = RootInfo::create();
    rootInfo->activate();

    const auto vds = VirtualDesktopManager::self();
    vds->setRootInfo(rootInfo);

    // NETWM spec says we have to set it to (0,0) if we don't support it
    NETPoint *viewports = new NETPoint[VirtualDesktopManager::self()->count()];
    rootInfo->setDesktopViewport(VirtualDesktopManager::self()->count(), *viewports);
    delete[] viewports;

    NETSize desktop_geometry;
    desktop_geometry.width = m_geometry.width();
    desktop_geometry.height = m_geometry.height();
    rootInfo->setDesktopGeometry(desktop_geometry);
    rootInfo->setActiveWindow(XCB_WINDOW_NONE);

    // Focus the null window, technically it is not required but we do it anyway just to be consistent.
    focusToNull();
}

void Workspace::cleanupX11()
{
    // We expect that other components will unregister their X11 event filters after the
    // connection to the X server has been lost.

    StackingUpdatesBlocker blocker(this);

    // Use stacking_order, so that kwin --replace keeps stacking order.
    const auto stack = stacking_order;
    for (Window *window : stack) {
        if (auto x11 = qobject_cast<X11Window *>(window); x11 && !x11->isDeleted()) {
            x11->releaseWindow(true);
            removeFromStack(window);
        }
    }

    manual_overlays.clear();

    VirtualDesktopManager *desktopManager = VirtualDesktopManager::self();
    desktopManager->setRootInfo(nullptr);

    RootInfo::destroy();
    Xcb::Extensions::destroy();

    m_startup.reset();
    m_nullFocus.reset();
    m_syncAlarmFilter.reset();
}
#endif

Workspace::~Workspace()
{
    blockStackingUpdates(true);

#if KWIN_BUILD_X11
    cleanupX11();
#endif

    while (!waylandServer()->windows().isEmpty()) {
        waylandServer()->windows()[0]->destroyWindow();
    }

    while (!m_windows.isEmpty()) {
        m_windows[0]->destroyWindow();
    }

    m_rulebook.reset();
    kwinApp()->config()->sync();

    m_placement.reset();
    delete m_windowKeysDialog;

    if (m_placeholderOutput) {
        m_placeholderOutput->unref();
    }
    m_tileManagers.clear();

    for (LogicalOutput *output : std::as_const(m_outputs)) {
        Q_EMIT outputRemoved(output);
        output->backendOutput()->unref();
        output->unref();
    }

    _self = nullptr;
}

OutputConfigurationError Workspace::applyOutputConfiguration(OutputConfiguration &config)
{
    assignBrightnessDevices(config);

    m_outputConfigStore->applyMirroring(config, kwinApp()->outputBackend()->outputs());
    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    for (BackendOutput *output : allOutputs) {
        config.changeSet(output)->dpmsMode = m_dpms == DpmsState::Off ? BackendOutput::DpmsMode::Off : BackendOutput::DpmsMode::On;
    }
    auto error = kwinApp()->outputBackend()->applyOutputChanges(config);
    if (error != OutputConfigurationError::None) {
        return error;
    }
    updateOutputs();
    m_outputConfigStore->storeConfig(kwinApp()->outputBackend()->outputs(), m_lidSwitchTracker->isLidClosed(), config);
    m_orientationSensor->setEnabled(m_outputConfigStore->isAutoRotateActive(kwinApp()->outputBackend()->outputs(), kwinApp()->tabletModeManager()->effectiveTabletMode()));

    updateXwaylandScale();

    for (LogicalOutput *output : std::as_const(m_outputs)) {
        output->backendOutput()->renderLoop()->scheduleRepaint();
    }

    return OutputConfigurationError::None;
}

void Workspace::requestDpmsState(DpmsState state)
{
    const bool requestOn = state == DpmsState::On;
    const bool isOn = m_dpms == DpmsState::On;
    if (requestOn == isOn) {
        return;
    }
    if (state == DpmsState::Off) {
        state = DpmsState::AboutToTurnOff;
    }
    m_dpms = state;

    // See kscreen.kcfg
    const auto animationTime = std::chrono::milliseconds(KSharedConfig::openConfig()->group(QStringLiteral("Effect-Kscreen")).readEntry("Duration", 250));

    // the config can be empty, it gets adjusted in applyOutputConfiguration
    OutputConfiguration cfg;
    if (m_dpms == DpmsState::On) {
        applyOutputConfiguration(cfg);
        m_dpmsFilter.reset();
        m_dpmsTimer.stop();
    } else {
        applyOutputConfiguration(cfg);
        m_dpmsFilter = std::make_unique<DpmsInputEventFilter>();
        input()->installInputEventFilter(m_dpmsFilter.get());
        m_dpmsTimer.start(animationTime);
        // TODO only do this if sleep is actually requested
        m_sleepInhibitor = kwinApp()->outputBackend()->session()->delaySleep("dpms animation");
    }
    // When dpms mode for display changes, we need to trigger checking if dpms mode should be enabled/disabled.
    m_orientationSensor->setEnabled(m_outputConfigStore->isAutoRotateActive(kwinApp()->outputBackend()->outputs(), kwinApp()->tabletModeManager()->effectiveTabletMode()));

    Q_EMIT dpmsStateChanged(animationTime);
}

Workspace::DpmsState Workspace::dpmsState() const
{
    return m_dpms;
}

void Workspace::updateXwaylandScale()
{
    KConfigGroup kscreenGroup = kwinApp()->kdeglobals()->group(QStringLiteral("KScreen"));
    const bool xwaylandClientsScale = kscreenGroup.readEntry("XwaylandClientsScale", true);
    if (xwaylandClientsScale && !m_outputs.isEmpty()) {
        double maxScale = 0;
        for (LogicalOutput *output : m_outputs) {
            maxScale = std::max(maxScale, output->scale());
        }
        kwinApp()->setXwaylandScale(maxScale);
    } else {
        kwinApp()->setXwaylandScale(1);
    }
}

void Workspace::updateOutputConfiguration()
{
    const auto outputs = kwinApp()->outputBackend()->outputs();
    if (outputs.empty()) {
        // nothing to do
        updateOutputOrder();
        return;
    }

    const bool alreadyHaveEnabledOutputs = std::ranges::any_of(outputs, [](BackendOutput *o) {
        return o->isEnabled();
    });

    QList<BackendOutput *> toEnable = outputs;
    OutputConfigurationError error = OutputConfigurationError::None;
    do {
        auto opt = m_outputConfigStore->queryConfig(toEnable, m_lidSwitchTracker->isLidClosed(), m_orientationSensor->reading(), kwinApp()->tabletModeManager()->effectiveTabletMode());
        if (!opt) {
            return;
        }
        auto &[cfg, type] = *opt;

        for (const auto &output : outputs) {
            if (!toEnable.contains(output)) {
                cfg.changeSet(output)->enabled = false;
            }
        }

        error = applyOutputConfiguration(cfg);
        switch (error) {
        case OutputConfigurationError::None:
            if (type == OutputConfigurationStore::ConfigType::Generated) {
                const bool hasInternal = std::any_of(outputs.begin(), outputs.end(), [](BackendOutput *o) {
                    return o->isInternal();
                });
                if (hasInternal && outputs.size() == 2) {
                    // show the OSD with output configuration presets
                    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kscreen.osdService"),
                                                                          QStringLiteral("/org/kde/kscreen/osdService"),
                                                                          QStringLiteral("org.kde.kscreen.osdService"),
                                                                          QStringLiteral("showActionSelector"));
                    QDBusConnection::sessionBus().asyncCall(message);
                }
            }
            return;
        case OutputConfigurationError::Unknown:
        case OutputConfigurationError::TooManyEnabledOutputs:
        case OutputConfigurationError::Timeout:
            if (alreadyHaveEnabledOutputs) {
                // just keeping the old output configuration is preferable
                break;
            }
            toEnable.removeLast();
            break;
        }
    } while (error == OutputConfigurationError::TooManyEnabledOutputs && !toEnable.isEmpty() && !alreadyHaveEnabledOutputs);

    qCCritical(KWIN_CORE, "Applying output configuration failed!");
    // Update the output order to a fallback list, to avoid dangling pointers
    updateOutputOrder();
    // If applying the output configuration failed, brightness devices weren't assigned either.
    // To prevent dangling pointers, unset removed brightness brightness devices here
    const auto devices = waylandServer()->externalBrightness()->devices();
    for (BackendOutput *output : outputs) {
        if (output->brightnessDevice() && !devices.contains(output->brightnessDevice())) {
            output->unsetBrightnessDevice();
        }
    }
}

void Workspace::setupWindowConnections(Window *window)
{
    connect(window, &Window::minimizedChanged, this, std::bind(&Workspace::windowMinimizedChanged, this, window));
    connect(window, &Window::fullScreenChanged, m_screenEdges.get(), &ScreenEdges::checkBlocking);
}

void Workspace::constrain(Window *below, Window *above)
{
    if (below == above) {
        return;
    }

    QList<Constraint *> parents;
    QList<Constraint *> children;
    for (Constraint *constraint : std::as_const(m_constraints)) {
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

    for (Constraint *parent : std::as_const(parents)) {
        parent->children << constraint;
    }

    for (Constraint *child : std::as_const(children)) {
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
            for (Constraint *child : std::as_const(constraint->children)) {
                child->parents.removeOne(constraint);
            }
        } else {
            for (Constraint *parent : std::as_const(constraint->parents)) {
                parent->children.removeOne(constraint);
            }
        }
        delete m_constraints.takeAt(i);
    }
}

#if KWIN_BUILD_X11
X11Window *Workspace::createX11Window(xcb_window_t windowId, bool is_mapped)
{
    StackingUpdatesBlocker blocker(this);
    X11Window *window = new X11Window();
    setupWindowConnections(window);
    if (!window->manage(windowId, is_mapped)) {
        X11Window::deleteClient(window);
        return nullptr;
    }
    addX11Window(window);
    Q_EMIT windowAdded(window);
    return window;
}

X11Window *Workspace::createUnmanaged(xcb_window_t windowId)
{
    X11Window *window = new X11Window();
    if (!window->track(windowId)) {
        X11Window::deleteClient(window);
        return nullptr;
    }
    addUnmanaged(window);
    return window;
}

void Workspace::addX11Window(X11Window *window)
{
    if (showingDesktop() && breaksShowingDesktop(window)) {
        setShowingDesktop(false);
    }

    Group *grp = findGroup(window->window());
    if (grp != nullptr) {
        grp->gotLeader(window);
    }

    m_focusChain->update(window, FocusChain::Update);
    Q_ASSERT(!m_windows.contains(window));
    m_windows.append(window);
    addToStack(window);
    window->updateLayer();
    window->checkActiveModal();
    checkTransients(window->window()); // SELI TODO: Does this really belong here?
    updateStackingOrder(true); // Propagatem new window
    updateTabbox();
}

void Workspace::addUnmanaged(X11Window *window)
{
    Q_ASSERT(!m_windows.contains(window));
    m_windows.append(window);
    addToStack(window);
    updateXStackingOrder();
    updateStackingOrder(true);
    Q_EMIT windowAdded(window);
}

/**
 * Destroys the window \a window
 */
void Workspace::removeX11Window(X11Window *window)
{
    Q_ASSERT(m_windows.contains(window));
    Group *group = findGroup(window->window());
    if (group != nullptr) {
        group->lostLeader();
    }
    removeWindow(window);
}

void Workspace::removeUnmanaged(X11Window *window)
{
    Q_ASSERT(m_windows.contains(window));
    m_windows.removeOne(window);
    Q_EMIT windowRemoved(window);
}
#endif

void Workspace::addDeleted(Window *c)
{
    Q_ASSERT(!deleted.contains(c));
    deleted.append(c);
}

void Workspace::removeDeleted(Window *c)
{
    Q_ASSERT(deleted.contains(c));
    Q_EMIT deletedRemoved(c);
    deleted.removeAll(c);
    removeFromStack(c);
}

void Workspace::addWaylandWindow(Window *window)
{
    if (showingDesktop() && breaksShowingDesktop(window)) {
        setShowingDesktop(false);
    }

    setupWindowConnections(window);
    window->updateLayer();

    if (window->isPlaceable() && !window->isPlaced()) {
        const QRectF area = clientArea(PlacementArea, window, activeOutput());
        if (const auto placement = m_placement->place(window, area)) {
            window->place(*placement);
        }
    }
    Q_ASSERT(!m_windows.contains(window));
    m_windows.append(window);
    addToStack(window);

    const bool shouldActivate = window->wantsInput() && !window->isPopupWindow()
        && ((mayActivate(window, window->activationToken()) && !window->isDesktop())
            // focus stealing prevention "low" should always activate new windows
            || (!window->isDesktop() && options->focusStealingPreventionLevel() <= FocusStealingPreventionLevel::Low)
            // If there's no active window, make this desktop the active one.
            || !activeWindow());
    if (!window->isMinimized() && !shouldActivate && !window->isPopupWindow()) {
        // This window won't be activated, so move it out of the way
        // of the active window
        restackWindowUnderActive(window);
    }

    updateStackingOrder(true);
    if (window->hasStrut()) {
        rearrange();
    }
    if (!window->isMinimized() && shouldActivate) {
        activateWindow(window);
    }
    updateTabbox();
    Q_EMIT windowAdded(window);
}

void Workspace::removeWaylandWindow(Window *window)
{
    activateNextWindow(window);
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

    m_windows.removeAll(window);
    if (window == m_delayFocusWindow) {
        cancelDelayFocus();
    }
    attention_chain.removeAll(window);
    if (window == m_activeWindow) {
        m_activeWindow = nullptr;
    }
    if (window == m_lastActiveWindow) {
        m_lastActiveWindow = nullptr;
    }
    if (m_windowKeysWindow == window) {
        setupWindowShortcutDone(false);
    }
    if (window->hasStrut()) {
        scheduleRearrange();
    }

    Q_EMIT windowRemoved(window);

    updateStackingOrder(true);
    updateTabbox();
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

    Q_EMIT configChanged();
    m_userActionsMenu->discard();

    m_rulebook->load();
    for (Window *window : std::as_const(m_windows)) {
        if (window->supportsWindowRules()) {
            window->evaluateWindowRules();
            m_rulebook->discardUsed(window, false);
        }
    }

    if (borderlessMaximizedWindows != options->borderlessMaximizedWindows() && !options->borderlessMaximizedWindows()) {
        // in case borderless maximized windows option changed and new option
        // is to have borders, we need to unset the borders for all maximized windows
        for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
            if ((*it)->maximizeMode() == MaximizeFull) {
                (*it)->setNoBorder(false);
            }
        }
    }
}

void Workspace::slotCurrentDesktopChanged(VirtualDesktop *oldDesktop, VirtualDesktop *newDesktop)
{
    updateWindowVisibilityAndActivateOnDesktopChange(newDesktop);
    Q_EMIT currentDesktopChanged(oldDesktop, m_moveResizeWindow);
}

void Workspace::slotCurrentDesktopChanging(VirtualDesktop *currentDesktop, QPointF offset)
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
#if KWIN_BUILD_X11
    for (auto it = stacking_order.constBegin(); it != stacking_order.constEnd(); ++it) {
        X11Window *c = qobject_cast<X11Window *>(*it);
        if (!c) {
            continue;
        }
        if (!(c->isOnDesktop(newDesktop) && c->isOnCurrentActivity()) && c != m_moveResizeWindow) {
            (c)->updateVisibility();
        }
    }
    // Now propagate the change, after hiding, before showing
    if (rootInfo()) {
        rootInfo()->setCurrentDesktop(VirtualDesktopManager::self()->current());
    }
#endif

    // FIXME: Keep Move/Resize window across activities

    if (m_moveResizeWindow && !m_moveResizeWindow->isOnDesktop(newDesktop)) {
        m_moveResizeWindow->setDesktops({newDesktop});
    }

#if KWIN_BUILD_X11
    for (int i = stacking_order.size() - 1; i >= 0; --i) {
        X11Window *c = qobject_cast<X11Window *>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnDesktop(newDesktop) && c->isOnCurrentActivity()) {
            c->updateVisibility();
        }
    }
#endif
    if (showingDesktop()) { // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);
    }
}

void Workspace::updateWindowVisibilityAndActivateOnDesktopChange(VirtualDesktop *newDesktop)
{
    closeActivePopup();
    ++block_focus;
    StackingUpdatesBlocker blocker(this);
    updateWindowVisibilityOnDesktopChange(newDesktop);
    // Restore the focus on this desktop
    --block_focus;

    for (Window *window : std::as_const(m_windows)) {
        if (!window->isOnDesktop(newDesktop)) {
            continue;
        }

        Tile *tile = nullptr;
        for (const auto &[output, manager] : m_tileManagers) {
            if (Tile *candidate = manager->tileForWindow(window, newDesktop)) {
                tile = candidate;
            }
        }

        window->requestTile(tile);
    }

    activateWindowOnDesktop(newDesktop);
}

void Workspace::activateWindowOnDesktop(VirtualDesktop *desktop)
{
    Window *window = nullptr;
    if (options->focusPolicyIsReasonable()) {
        window = findWindowToActivateOnDesktop(desktop);
    }
    // If "unreasonable focus policy" and m_activeWindow is on_all_desktops and
    // under mouse (Hence == old_active_window), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (m_activeWindow && m_activeWindow->isShown() && m_activeWindow->isOnCurrentDesktop() && m_activeWindow->isOnCurrentActivity()) {
        window = m_activeWindow;
    }

    if (!window) {
        window = findDesktop(desktop, activeOutput());
    }

    if (window) {
        if (requestFocus(window)) {
            return;
        }
    }

    resetFocus();
}

Window *Workspace::findWindowToActivateOnDesktop(VirtualDesktop *desktop)
{
    if (m_moveResizeWindow != nullptr && m_activeWindow == m_moveResizeWindow && m_focusChain->contains(m_activeWindow, desktop) && m_activeWindow->isShown() && m_activeWindow->isOnCurrentDesktop() && m_activeWindow->isOnCurrentActivity()) {
        // A requestFocus call will fail, as the window is already active
        return m_activeWindow;
    }
    // from activation.cpp
    if (options->isNextFocusPrefersMouse()) {
        auto it = stackingOrder().constEnd();
        while (it != stackingOrder().constBegin()) {
            auto window = *(--it);
            if (!window->isClient()) {
                continue;
            }

            if (!(window->isShown() && window->isOnDesktop(desktop) && window->isOnCurrentActivity() && window->isOnActiveOutput())) {
                continue;
            }

            if (exclusiveContains(window->frameGeometry(), Cursors::self()->mouse()->pos())) {
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

    updateWindowVisibilityAndActivateOnDesktopChange(VirtualDesktopManager::self()->currentDesktop());

    Q_EMIT currentActivityChanged();
#endif
}

LogicalOutput *Workspace::outputAt(const QPointF &pos) const
{
    LogicalOutput *bestOutput = nullptr;
    qreal minDistance;

    for (LogicalOutput *output : std::as_const(m_outputs)) {
        const QRectF geo = output->geometry();

        const QPointF closestPoint(std::clamp(pos.x(), geo.x(), geo.x() + geo.width() - 1),
                                   std::clamp(pos.y(), geo.y(), geo.y() + geo.height() - 1));

        const QPointF ray = closestPoint - pos;
        const qreal distance = ray.x() * ray.x() + ray.y() * ray.y();
        if (!bestOutput || distance < minDistance) {
            minDistance = distance;
            bestOutput = output;
        }
    }
    return bestOutput;
}

LogicalOutput *Workspace::findOutput(const QString &name) const
{
    for (LogicalOutput *output : std::as_const(m_outputs)) {
        if (output->name() == name) {
            return output;
        }
    }
    return nullptr;
}

LogicalOutput *Workspace::findOutput(LogicalOutput *reference, Direction direction, bool wrapAround) const
{
    QList<LogicalOutput *> relevantOutputs;
    std::copy_if(m_outputs.begin(), m_outputs.end(), std::back_inserter(relevantOutputs), [reference, direction](LogicalOutput *output) {
        switch (direction) {
        case DirectionEast:
        case DirectionWest:
            // filter for outputs on same horizontal line
            return output->geometry().top() <= reference->geometry().bottom() && output->geometry().bottom() >= reference->geometry().top();
        case DirectionSouth:
        case DirectionNorth:
            // filter for outputs on same vertical line
            return output->geometry().left() <= reference->geometry().right() && output->geometry().right() >= reference->geometry().left();
        default:
            // take all outputs
            return true;
        }
    });

    std::sort(relevantOutputs.begin(), relevantOutputs.end(), [direction](const LogicalOutput *o1, const LogicalOutput *o2) {
        switch (direction) {
        case DirectionEast:
        case DirectionWest:
            // order outputs from left to right
            return o1->geometry().center().x() < o2->geometry().center().x();
        case DirectionSouth:
        case DirectionNorth:
            // order outputs from top to bottom
            return o1->geometry().center().y() < o2->geometry().center().y();
        default:
            // order outputs from top to bottom, then left to right
            // case 1: o1 is above o2
            // case 2: o1 is not below o2, and o1 is left of o2
            return o1->geometry().y() + o1->geometry().height() <= o2->geometry().top() || (o1->geometry().top() < o2->geometry().y() + o2->geometry().height() && o1->geometry().left() < o2->geometry().left());
        }
    });

    const int index = relevantOutputs.indexOf(reference);
    Q_ASSERT(index != -1);
    switch (direction) {
    case DirectionEast:
    case DirectionSouth:
    case DirectionNext:
        // go forward in the list
        return relevantOutputs[wrapAround ? (index + 1) % relevantOutputs.count() : std::min(index + 1, (int)relevantOutputs.count() - 1)];
    case DirectionWest:
    case DirectionNorth:
    case DirectionPrev:
        // go backward in the list
        return relevantOutputs[wrapAround ? (index + relevantOutputs.count() - 1) % relevantOutputs.count() : std::max(index - 1, 0)];
    default:
        Q_UNREACHABLE();
    }
}

LogicalOutput *Workspace::findOutput(BackendOutput *backendOutput) const
{
    const auto it = std::ranges::find_if(m_outputs, [backendOutput](LogicalOutput *logical) {
        return logical->backendOutput() == backendOutput
            || logical->uuid() == backendOutput->replicationSource();
    });
    return it == m_outputs.end() ? nullptr : *it;
}

void Workspace::slotOutputBackendOutputsQueried()
{
    updateOutputConfiguration();
    updateOutputs();
}

static const int s_dpmsTimeout = environmentVariableIntValue("KWIN_DPMS_WORKAROUND_TIMEOUT").value_or(2000);

void Workspace::updateOutputs()
{
    const auto availableOutputs = kwinApp()->outputBackend()->outputs();
    const auto oldLogicalOutputs = m_outputs;
    QList<BackendOutput *> newBackendOutputs;
    bool wakeUp = false;

    if (m_moveResizeWindow) {
        m_moveResizeWindow->cancelInteractiveMoveResize();
    }

    for (BackendOutput *output : availableOutputs) {
        output->setAutoRotateAvailable(m_orientationSensor->isAvailable());
        if (output->isNonDesktop() || !output->isEnabled()) {
            continue;
        }
        const auto replicationSource = std::ranges::find_if(availableOutputs, [output](BackendOutput *other) {
            return other->uuid() == output->replicationSource();
        });
        if (replicationSource != availableOutputs.end() && (*replicationSource)->isEnabled()) {
            continue;
        }
        newBackendOutputs.append(output);
    }

    // The workspace requires at least one output connected.
    if (newBackendOutputs.isEmpty()) {
        if (!m_placeholderOutput) {
            m_placeholderOutput = new PlaceholderOutput(QSize(1920, 1080), 1);
            m_placeholderFilter = std::make_unique<PlaceholderInputEventFilter>();
            input()->installInputEventFilter(m_placeholderFilter.get());
        }
        newBackendOutputs.append(m_placeholderOutput);
    } else {
        if (m_placeholderOutput) {
            m_placeholderOutput->unref();
            m_placeholderOutput = nullptr;
            m_placeholderFilter.reset();
        }
    }

    // Re-create m_outputs list, creating new outputs as necessary
    // Removed outputs will be unreferenced later
    m_outputs.clear();
    for (BackendOutput *output : newBackendOutputs) {
        const auto existing = std::ranges::find_if(oldLogicalOutputs, [output](LogicalOutput *logical) {
            return output == logical->backendOutput();
        });
        if (existing == oldLogicalOutputs.end()) {
            m_outputs.push_back(new LogicalOutput(output));
        } else {
            m_outputs.push_back(*existing);
        }
    }

    if (!m_activeOutput) {
        setActiveOutput(m_outputs[0]);
    }

    updateOutputOrder();

    const QSet<LogicalOutput *> oldOutputsSet(oldLogicalOutputs.constBegin(), oldLogicalOutputs.constEnd());
    const QSet<LogicalOutput *> outputsSet(m_outputs.constBegin(), m_outputs.constEnd());

    const auto added = outputsSet - oldOutputsSet;
    for (LogicalOutput *output : added) {
        output->backendOutput()->ref();
        m_tileManagers[output] = std::make_unique<TileManager>(output);
        Q_EMIT outputAdded(output);
        wakeUp |= !m_recentlyRemovedDpmsOffOutputs.contains(output->uuid());
    }

    m_placementTracker->inhibit();

    const auto removed = oldOutputsSet - outputsSet;
    for (LogicalOutput *output : removed) {
        Q_EMIT outputRemoved(output);

        auto tileManager = std::move(m_tileManagers[output]);
        m_tileManagers.erase(output);

        const auto desktops = VirtualDesktopManager::self()->desktops();
        for (VirtualDesktop *desktop : desktops) {
            // Evacuate windows from the defunct custom tile tree.
            tileManager->rootTile(desktop)->visitDescendants([](Tile *child) {
                const QList<Window *> windows = child->windows();
                for (Window *window : windows) {
                    child->unmanage(window);
                }
            });

            // Migrate windows from the defunct quick tile to a quick tile tree on another output.
            static constexpr QuickTileMode quickTileModes[] = {
                QuickTileFlag::Left,
                QuickTileFlag::Right,
                QuickTileFlag::Top,
                QuickTileFlag::Bottom,
                QuickTileFlag::Top | QuickTileFlag::Left,
                QuickTileFlag::Top | QuickTileFlag::Right,
                QuickTileFlag::Bottom | QuickTileFlag::Left,
                QuickTileFlag::Bottom | QuickTileFlag::Right,
            };

            for (const QuickTileMode &quickTileMode : quickTileModes) {
                Tile *quickTile = tileManager->quickRootTile(desktop)->tileForMode(quickTileMode);
                const QList<Window *> windows = quickTile->windows();
                if (windows.isEmpty()) {
                    continue;
                }

                LogicalOutput *bestOutput = outputAt(output->geometry().center());
                Tile *bestTile = m_tileManagers[bestOutput]->quickRootTile(desktop)->tileForMode(quickTileMode);

                if (bestTile) {
                    for (Window *window : windows) {
                        bestTile->manage(window);
                    }
                }
            }
        }
    }

    desktopResized();

    m_placementTracker->uninhibit();
    m_placementTracker->restore(outputLayoutId());

    Q_EMIT outputsChanged();

    for (LogicalOutput *output : removed) {
        if (m_dpms == Workspace::DpmsState::Off) {
            m_recentlyRemovedDpmsOffOutputs.push_back(output->uuid());
            QTimer::singleShot(s_dpmsTimeout, [this, uuid = output->uuid()]() {
                m_recentlyRemovedDpmsOffOutputs.removeOne(uuid);
            });
        }
        output->backendOutput()->unref();
        output->unref();
    }

    // if a new output was added, turn all displays on
    if (wakeUp) {
        requestDpmsState(DpmsState::On);
    }
}

void Workspace::assignBrightnessDevices(OutputConfiguration &outputConfig)
{
    QList<BackendOutput *> candidates = kwinApp()->outputBackend()->outputs();
    const auto devices = waylandServer()->externalBrightness()->devices();
    for (BrightnessDevice *device : devices) {
        // assign the device to the most fitting output
        const auto it = std::ranges::find_if(candidates, [device, &outputConfig](BackendOutput *output) {
            if (output->isInternal() != device->isInternal()) {
                return false;
            }
            const auto changeset = outputConfig.constChangeSet(output);
            const bool disallowDdcCi = output->isDdcCiKnownBroken()
                || (changeset && !changeset->allowDdcCi.value_or(output->allowDdcCi()))
                || (!changeset && !output->allowDdcCi());
            if (disallowDdcCi && device->usesDdcCi()) {
                return false;
            }
            if (output->isInternal()) {
                return true;
            } else {
                return output->edid().isValid() && !device->edidBeginning().isEmpty() && output->edid().raw().startsWith(device->edidBeginning());
            }
        });
        if (it != candidates.end()) {
            BackendOutput *const output = *it;
            candidates.erase(it);
            const auto changeset = outputConfig.changeSet(output);
            changeset->brightnessDevice = device;
            if (device->usesDdcCi()) {
                changeset->detectedDdcCi = true;
            }
            if (changeset->allowSdrSoftwareBrightness.value_or(output->allowSdrSoftwareBrightness())) {
                changeset->allowSdrSoftwareBrightness = false;
                changeset->brightness = device->observedBrightness();
                changeset->currentBrightness = device->observedBrightness();
            }
        }
    }
    for (BackendOutput *output : candidates) {
        outputConfig.changeSet(output)->brightnessDevice = nullptr;
    }
}

void Workspace::slotDesktopAdded(VirtualDesktop *desktop)
{
    m_focusChain->addDesktop(desktop);
    rearrange();
}

void Workspace::slotDesktopRemoved(VirtualDesktop *desktop)
{
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        if (!(*it)->desktops().contains(desktop)) {
            continue;
        }
        if ((*it)->desktops().count() > 1) {
            (*it)->leaveDesktop(desktop);
        } else {
            const uint desktopId = std::min(desktop->x11DesktopNumber(), VirtualDesktopManager::self()->count());
            sendWindowToDesktops(*it, {VirtualDesktopManager::self()->desktopForX11Id(desktopId)}, true);
        }
    }

    for (auto it = deleted.constBegin(); it != deleted.constEnd(); ++it) {
        if ((*it)->desktops().contains(desktop)) {
            (*it)->leaveDesktop(desktop);
        }
    }

    rearrange();
    m_focusChain->removeDesktop(desktop);
}

void Workspace::slotEndInteractiveMoveResize()
{
    auto moveResizeWindow = workspace()->moveResizeWindow();
    if (moveResizeWindow) {
        moveResizeWindow->endInteractiveMoveResize();
    }
}

#if KWIN_BUILD_X11
void Workspace::selectWmInputEventMask()
{
    uint32_t presentMask = 0;
    Xcb::WindowAttributes attr(kwinApp()->x11RootWindow());
    if (!attr.isNull()) {
        presentMask = attr->your_event_mask;
    }

    const uint32_t wmMask = XCB_EVENT_MASK_PROPERTY_CHANGE
        | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
        | XCB_EVENT_MASK_FOCUS_CHANGE; // For NotifyDetailNone

    Xcb::selectInput(kwinApp()->x11RootWindow(), presentMask | wmMask);
}
#endif

void Workspace::addWindowToDesktop(Window *window, VirtualDesktop *desktop)
{
    auto desktops = window->desktops();
    if (desktops.contains(desktop)) {
        return;
    }
    desktops.append(desktop);
    sendWindowToDesktops(window, desktops, false);
}

void Workspace::removeWindowFromDesktop(Window *window, VirtualDesktop *desktop)
{
    auto desktops = window->desktops();
    if (!desktops.contains(desktop)) {
        return;
    }
    desktops.removeOne(desktop);
    sendWindowToDesktops(window, desktops, false);
}

/**
 * Sends window \a window to desktop \a desk.
 *
 * Takes care of transients as well.
 */
void Workspace::sendWindowToDesktops(Window *window, const QList<VirtualDesktop *> &desktops, bool dont_activate)
{
    const QList<VirtualDesktop *> oldDesktops = window->desktops();
    const bool wasOnCurrent = window->isOnCurrentDesktop();
    const bool wasActive = window->isActive();
    window->setDesktops(desktops);
    if (window->desktops() != desktops) { // No change or desktop forced
        return;
    }

    if (window->isOnCurrentDesktop()) {
        if (window->wantsTabFocus() && options->focusPolicyIsReasonable() && !wasOnCurrent && // for stickiness changes
            !dont_activate) {
            requestFocus(window);
        } else {
            restackWindowUnderActive(window);
        }
    } else {

        // raise the window on the desktop it has been added to
        raiseWindow(window);
        // but set a new active window on the current desktop
        if (wasActive) {
            activateWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop());
        }
    }

    window->checkWorkspacePosition(QRect(), oldDesktops.isEmpty() ? nullptr : oldDesktops.last());

    auto transients_stacking_order = ensureStackingOrder(window->transients());
    for (auto it = transients_stacking_order.constBegin(); it != transients_stacking_order.constEnd(); ++it) {
        sendWindowToDesktops(*it, window->desktops(), dont_activate);
    }
    rearrange();
}

/**
 * Delayed focus functions
 */
void Workspace::delayFocus()
{
    Q_ASSERT(m_delayFocusWindow);
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
    m_delayFocusWindow = nullptr;
}

#if KWIN_BUILD_X11
bool Workspace::checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data)
{
    return m_startup->checkStartup(w, id, data) == KStartupInfo::Match;
}
#endif

/**
 * Puts the focus on a dummy window
 * Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull()
{
#if KWIN_BUILD_X11
    if (m_nullFocus) {
        const xcb_void_cookie_t cookie = xcb_set_input_focus(kwinApp()->x11Connection(), XCB_INPUT_FOCUS_POINTER_ROOT, *m_nullFocus, XCB_TIME_CURRENT_TIME);
        m_x11FocusSerial = cookie.sequence;
    }
#endif
}

#if KWIN_BUILD_X11
xcb_window_t Workspace::nullFocusWindow() const
{
    if (!m_nullFocus) {
        return XCB_WINDOW_NONE;
    }
    return *m_nullFocus;
}
#endif

bool Workspace::breaksShowingDesktop(Window *window) const
{
    return !(window->isUnmanaged() || window->isDock() || window->isDesktop() || window->belongsToDesktop() || window->isInputMethod());
}

void Workspace::setShowingDesktop(bool showing, bool animated)
{
    const bool changed = showing != showing_desktop;

#if KWIN_BUILD_X11
    if (rootInfo() && changed) {
        rootInfo()->setShowingDesktop(showing);
    }
#endif
    showing_desktop = showing;

    for (int i = stacking_order.count() - 1; i > -1; --i) {
        auto window = stacking_order.at(i);
        if (window->isDeleted()) {
            continue;
        }
        if (breaksShowingDesktop(window)) {
            window->setHiddenByShowDesktop(showing_desktop);
        }
    }

    if (showing_desktop) {
        Window *desktop = findDesktop(VirtualDesktopManager::self()->currentDesktop(), activeOutput());
        if (desktop) {
            requestFocus(desktop);
        }
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
#ifdef KWIN_BUILD_GLOBALSHORTCUTS
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/kglobalaccel"),
                                                          QStringLiteral("org.kde.KGlobalAccel"),
                                                          QStringLiteral("blockGlobalShortcuts"));
    message.setArguments(QList<QVariant>() << disable);
    QDBusConnection::sessionBus().asyncCall(message);
#endif

    m_globalShortcutsDisabledForWindow = disable;
}

QString Workspace::supportInformation() const
{
    QString support;
    const QString yes = QStringLiteral("yes\n");
    const QString no = QStringLiteral("no\n");

    support.append(ki18nc("Introductory text shown in the support information.",
                          "KWin Support Information:\n"
                          "The following information should be used when requesting support on e.g. https://discuss.kde.org.\n"
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
    support.append(KWIN_VERSION_STRING);
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt Version: "));
    support.append(QString::fromUtf8(qVersion()));
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt compile version: %1\n").arg(QStringLiteral(QT_VERSION_STR)));
    support.append(QStringLiteral("XCB compile version: %1\n\n").arg(XCB_VERSION_STRING));
    support.append(QStringLiteral("Operation Mode: Wayland"));
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
    support.append(QStringLiteral("\n"));

#if KWIN_BUILD_X11
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
#endif

    if (m_decorationBridge) {
        support.append(QStringLiteral("Decoration\n"));
        support.append(QStringLiteral("==========\n"));
        support.append(m_decorationBridge->supportInformation());
        support.append(QStringLiteral("\n"));
    }
    support.append(QStringLiteral("LogicalOutput backend\n"));
    support.append(QStringLiteral("==============\n"));
    support.append(kwinApp()->outputBackend()->supportInformation());
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
        if (variant.typeId() == QMetaType::QSize) {
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
    const QMetaObject *metaScreenEdges = m_screenEdges->metaObject();
    for (int i = 0; i < metaScreenEdges->propertyCount(); ++i) {
        const QMetaProperty property = metaScreenEdges->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name(), printProperty(m_screenEdges->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreens\n"));
    support.append(QStringLiteral("=======\n"));
    const QList<BackendOutput *> outputs = kwinApp()->outputBackend()->outputs();
    support.append(QStringLiteral("Number of Screens: %1\n\n").arg(outputs.count()));
    for (int i = 0; i < outputs.count(); ++i) {
        const auto output = outputs[i];
        const auto logicalOutput = workspace()->findOutput(output);
        const QRect geo = logicalOutput ? logicalOutput->geometry() : QRect();
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
            support.append(QStringLiteral("Physical size: %1x%2mm\n")
                               .arg(output->physicalSize().width())
                               .arg(output->physicalSize().height()));
            support.append(QStringLiteral("Scale: %1\n").arg(output->scale()));
            support.append(QStringLiteral("Refresh Rate: %1\n").arg(output->refreshRate()));
            QString vrr = QStringLiteral("incapable");
            if (output->capabilities() & BackendOutput::Capability::Vrr) {
                switch (output->vrrPolicy()) {
                case VrrPolicy::Never:
                    vrr = QStringLiteral("never");
                    break;
                case VrrPolicy::Always:
                    vrr = QStringLiteral("always");
                    break;
                case VrrPolicy::Automatic:
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
            const auto context = Compositor::self()->scene()->openglContext();
            GLPlatform *platform = context->glPlatform();
            if (context->isOpenGLES()) {
                support.append(QStringLiteral("Compositing Type: OpenGL ES 2.0\n"));
            } else {
                support.append(QStringLiteral("Compositing Type: OpenGL\n"));
            }
            support.append(QStringLiteral("OpenGL vendor string: ") + QString::fromUtf8(platform->glVendorString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL renderer string: ") + QString::fromUtf8(platform->glRendererString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL version string: ") + QString::fromUtf8(platform->glVersionString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL platform interface: EGL"));
            support.append(QStringLiteral("\n"));

            support.append(QStringLiteral("OpenGL shading language version string: ") + QString::fromUtf8(platform->glShadingLanguageVersionString()) + QStringLiteral("\n"));

            support.append(QStringLiteral("Driver: ") + GLPlatform::driverToString(platform->driver()) + QStringLiteral("\n"));
            if (!platform->isMesaDriver()) {
                support.append(QStringLiteral("Driver version: ") + platform->driverVersion().toString() + QStringLiteral("\n"));
            }

            support.append(QStringLiteral("GPU class: ") + GLPlatform::chipClassToString(platform->chipClass()) + QStringLiteral("\n"));

            support.append(QStringLiteral("OpenGL version: ") + platform->glVersion().toString() + QStringLiteral("\n"));

            support.append(QStringLiteral("GLSL version: ") + platform->glslVersion().toString() + QStringLiteral("\n"));

            if (platform->isMesaDriver()) {
                support.append(QStringLiteral("Mesa version: ") + platform->mesaVersion().toString() + QStringLiteral("\n"));
            }
#if KWIN_BUILD_X11
            if (auto xVersion = Xcb::xServerVersion(); xVersion.isValid()) {
                support.append(QStringLiteral("X server version: ") + xVersion.toString() + QStringLiteral("\n"));
            }
#endif
            if (auto kernelVersion = linuxKernelVersion(); kernelVersion.isValid()) {
                support.append(QStringLiteral("Linux kernel version: ") + kernelVersion.toString() + QStringLiteral("\n"));
            }

            support.append(QStringLiteral("Direct rendering: "));
            support.append(QStringLiteral("Requires strict binding: "));
            if (!platform->isLooseBinding()) {
                support.append(QStringLiteral("yes\n"));
            } else {
                support.append(QStringLiteral("no\n"));
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
        const auto loadedEffects = effects->loadedEffects();
        for (const QString &effect : loadedEffects) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nCurrently Active Effects:\n"));
        support.append(QStringLiteral("-------------------------\n"));
        const auto activeEffects = effects->activeEffects();
        for (const QString &effect : activeEffects) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nEffect Settings:\n"));
        support.append(QStringLiteral("----------------\n"));
        for (const QString &effect : loadedEffects) {
            support.append(effects->supportInformation(effect));
            support.append(QStringLiteral("\n"));
        }
        support.append(QLatin1String("\nLoaded Plugins:\n"));
        support.append(QLatin1String("---------------\n"));
        QStringList loadedPlugins = kwinApp()->pluginManager()->loadedPlugins();
        loadedPlugins.sort();
        for (const QString &plugin : std::as_const(loadedPlugins)) {
            support.append(plugin + QLatin1Char('\n'));
        }
        support.append(QLatin1String("\nAvailable Plugins:\n"));
        support.append(QLatin1String("------------------\n"));
        QStringList availablePlugins = kwinApp()->pluginManager()->availablePlugins();
        availablePlugins.sort();
        for (const QString &plugin : std::as_const(availablePlugins)) {
            support.append(plugin + QLatin1Char('\n'));
        }
    } else {
        support.append(QStringLiteral("Compositing is not active\n"));
    }
    return support;
}

#if KWIN_BUILD_X11
UInt32Serial Workspace::x11FocusSerial() const
{
    return m_x11FocusSerial;
}

void Workspace::setX11FocusSerial(UInt32Serial serial)
{
    m_x11FocusSerial = serial;
}

void Workspace::forEachClient(std::function<void(X11Window *)> func)
{
    for (Window *window : std::as_const(m_windows)) {
        X11Window *x11Window = qobject_cast<X11Window *>(window);
        if (x11Window && !x11Window->isUnmanaged()) {
            func(x11Window);
        }
    }
}

X11Window *Workspace::findClient(std::function<bool(const X11Window *)> func) const
{
    for (Window *window : std::as_const(m_windows)) {
        X11Window *x11Window = qobject_cast<X11Window *>(window);
        if (x11Window && !x11Window->isUnmanaged() && func(x11Window)) {
            return x11Window;
        }
    }
    return nullptr;
}

X11Window *Workspace::findUnmanaged(std::function<bool(const X11Window *)> func) const
{
    for (Window *window : m_windows) {
        X11Window *x11Window = qobject_cast<X11Window *>(window);
        if (x11Window && x11Window->isUnmanaged() && func(x11Window)) {
            return x11Window;
        }
    }
    return nullptr;
}

X11Window *Workspace::findUnmanaged(xcb_window_t w) const
{
    return findUnmanaged([w](const X11Window *u) {
        return u->window() == w;
    });
}

X11Window *Workspace::findClient(xcb_window_t w) const
{
    return findClient([w](const X11Window *c) {
        return c->window() == w;
    });
}
#endif

Window *Workspace::findWindow(std::function<bool(const Window *)> func) const
{
    return Window::findInList(m_windows, func);
}

Window *Workspace::findWindow(const QUuid &internalId) const
{
    return findWindow([internalId](const KWin::Window *l) -> bool {
        return internalId == l->internalId();
    });
}

void Workspace::forEachWindow(std::function<void(Window *)> func)
{
    std::for_each(m_windows.constBegin(), m_windows.constEnd(), func);
}

bool Workspace::hasWindow(const Window *c)
{
    return findWindow([&c](const Window *test) {
        return test == c;
    })
        != nullptr;
}

Window *Workspace::findInternal(QWindow *w) const
{
    if (!w) {
        return nullptr;
    }
    for (Window *window : m_windows) {
        if (InternalWindow *internal = qobject_cast<InternalWindow *>(window)) {
            if (internal->handle() == w) {
                return internal;
            }
        }
    }
    return nullptr;
}

void Workspace::setWasUserInteraction()
{
    was_user_interaction = true;
}

void Workspace::updateTabbox()
{
#if KWIN_BUILD_TABBOX
    // Need to reset the client model even if the task switcher is hidden otherwise there
    // might be dangling pointers. Consider rewriting client model logic!
    m_tabbox->reset(true);
#endif
}

void Workspace::addInternalWindow(InternalWindow *window)
{
    Q_ASSERT(!m_windows.contains(window));
    m_windows.append(window);
    addToStack(window);

    setupWindowConnections(window);
    window->updateLayer();

    if (window->isPlaceable()) {
        const QRectF area = clientArea(PlacementArea, window, workspace()->activeOutput());
        if (const auto placement = m_placement->place(window, area)) {
            window->place(*placement);
        }
    }

    updateStackingOrder(true);
    if (window->isOutline() && moveResizeWindow()) {
        constrain(moveResizeWindow(), window);
    }
    Q_EMIT windowAdded(window);
}

void Workspace::removeInternalWindow(InternalWindow *window)
{
    m_windows.removeOne(window);

    updateStackingOrder();
    Q_EMIT windowRemoved(window);
}

#if KWIN_BUILD_X11
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
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        X11Window *candidate = qobject_cast<X11Window *>(*it);
        if (!candidate || candidate == window) {
            continue;
        }
        if (candidate->wmClientLeader() == window->wmClientLeader()) {
            if (ret == nullptr || ret == candidate->group()) {
                ret = candidate->group();
            } else {
                // There are already two groups with the same client leader.
                // This most probably means the app uses group transients without
                // setting group for its windows. Merging the two groups is a bad
                // hack, but there's no really good solution for this case.
                QList<X11Window *> old_group = candidate->group()->members();
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
#endif

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
                (*it)->setMinimized(true);
                updateMinimizedOfTransients((*it));
            }
        }
        if (window->isModal()) { // if a modal dialog is minimized, minimize its mainwindow too
            const auto windows = window->mainWindows();
            for (Window *main : std::as_const(windows)) {
                main->setMinimized(true);
            }
        }
    } else {
        // else unmiminize the transients
        for (auto it = window->transients().constBegin(); it != window->transients().constEnd(); ++it) {
            if ((*it)->isMinimized()) {
                (*it)->setMinimized(false);
                updateMinimizedOfTransients((*it));
            }
        }
        if (window->isModal()) {
            const auto windows = window->mainWindows();
            for (Window *main : std::as_const(windows)) {
                main->setMinimized(false);
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

#if KWIN_BUILD_X11
// A new window has been mapped. Check if it's not a mainwindow for some already existing transient window.
void Workspace::checkTransients(xcb_window_t w)
{
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        if (X11Window *x11Window = qobject_cast<X11Window *>(*it)) {
            x11Window->checkTransient(w);
        }
    }
}
#endif

/**
 * Resizes the workspace after an XRANDR screen size change
 */
void Workspace::desktopResized()
{
    const QRect oldGeometry = m_geometry;
    m_geometry = QRect();
    for (const LogicalOutput *output : std::as_const(m_outputs)) {
        m_geometry = m_geometry.united(output->geometry());
    }

#if KWIN_BUILD_X11
    if (rootInfo()) {
        NETSize desktop_geometry;
        desktop_geometry.width = Xcb::toXNative(m_geometry.width());
        desktop_geometry.height = Xcb::toXNative(m_geometry.height());
        rootInfo()->setDesktopGeometry(desktop_geometry);
    }
#endif

    rearrange();

    if (!m_outputs.contains(m_activeOutput)) {
        setActiveOutput(m_outputs[0]);
    }

    const auto stack = stackingOrder();
    for (Window *window : stack) {
        window->setMoveResizeOutput(outputAt(window->moveResizeGeometry().center()));
        window->setOutput(outputAt(window->frameGeometry().center()));
    }

    // TODO: Track uninitialized windows in the Workspace too.
    const auto windows = waylandServer()->windows();
    for (Window *window : windows) {
        window->setMoveResizeOutput(outputAt(window->moveResizeGeometry().center()));
        window->setOutput(outputAt(window->frameGeometry().center()));
    }

    // restore cursor position
    const auto oldCursorOutput = std::find_if(m_oldScreenGeometries.cbegin(), m_oldScreenGeometries.cend(), [](const auto &geometry) {
        return exclusiveContains(geometry, Cursors::self()->mouse()->pos());
    });
    if (oldCursorOutput != m_oldScreenGeometries.cend()) {
        const LogicalOutput *cursorOutput = oldCursorOutput.key();
        if (std::find(m_outputs.cbegin(), m_outputs.cend(), cursorOutput) != m_outputs.cend()) {
            const QRect oldGeometry = oldCursorOutput.value();
            const QRect newGeometry = cursorOutput->geometry();
            const QPointF relativePosition = Cursors::self()->mouse()->pos() - oldGeometry.topLeft();
            const QPointF newRelativePosition(newGeometry.width() * relativePosition.x() / float(oldGeometry.width()), newGeometry.height() * relativePosition.y() / float(oldGeometry.height()));
            input()->pointer()->warp(newGeometry.topLeft() + newRelativePosition);
        }
    }

    saveOldScreenSizes(); // after rearrange(), so that one still uses the previous one

    // TODO: emit a signal instead and remove the deep function calls into edges and effects
    m_screenEdges->recreateEdges();

    if (m_geometry != oldGeometry) {
        Q_EMIT geometryChanged();
    }
}

void Workspace::saveOldScreenSizes()
{
    m_oldScreenGeometries.clear();
    for (const LogicalOutput *output : std::as_const(m_outputs)) {
        m_oldScreenGeometries.insert(output, output->geometry());
    }
}

QRectF Workspace::adjustClientArea(Window *window, const QRectF &area) const
{
    RectF adjustedArea = area;

    RectF strutLeft = window->strutRect(StrutAreaLeft);
    RectF strutRight = window->strutRect(StrutAreaRight);
    RectF strutTop = window->strutRect(StrutAreaTop);
    RectF strutBottom = window->strutRect(StrutAreaBottom);

    // Handle struts at xinerama edges that are inside the virtual screen.
    // They're given in virtual screen coordinates, make them affect only
    // their xinerama screen.
    RectF screenArea = clientArea(ScreenArea, window);
    strutLeft.setLeft(std::max(strutLeft.left(), screenArea.left()));
    strutRight.setRight(std::min(strutRight.right(), screenArea.right()));
    strutTop.setTop(std::max(strutTop.top(), screenArea.top()));
    strutBottom.setBottom(std::min(strutBottom.bottom(), screenArea.bottom()));

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

void Workspace::scheduleRearrange()
{
    m_rearrangeTimer.start(0);
}

void Workspace::rearrange()
{
    Q_EMIT aboutToRearrange();
    m_rearrangeTimer.stop();

    const QList<VirtualDesktop *> desktops = VirtualDesktopManager::self()->desktops();

    QHash<const VirtualDesktop *, QRectF> workAreas;
    QHash<const VirtualDesktop *, StrutRects> restrictedAreas;
    QHash<const VirtualDesktop *, QHash<const LogicalOutput *, QRectF>> screenAreas;

    for (const VirtualDesktop *desktop : desktops) {
        workAreas[desktop] = m_geometry;

        for (const LogicalOutput *output : std::as_const(m_outputs)) {
            screenAreas[desktop][output] = output->geometryF();
        }
    }

    for (Window *window : std::as_const(m_windows)) {
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
        for (const LogicalOutput *output : std::as_const(m_outputs)) {
            if (!r.intersects(output->geometry())) {
                qCDebug(KWIN_CORE) << "Adjusted client area would exclude a complete screen, ignore";
                r = m_geometry;
                break;
            }
        }
        StrutRects strutRegion = window->strutRects();
        const QRect clientsScreenRect = window->output()->geometry();
        for (int i = strutRegion.size() - 1; i >= 0; --i) {
            const StrutRect clipped = StrutRect(strutRegion[i].intersected(clientsScreenRect), strutRegion[i].area());
            if (clipped.isEmpty()) {
                strutRegion.removeAt(i);
            } else {
                strutRegion[i] = clipped;
            }
        }

        const auto vds = window->isOnAllDesktops() ? desktops : window->desktops();
        for (VirtualDesktop *vd : vds) {
            workAreas[vd] &= r;
            restrictedAreas[vd] += strutRegion;
            for (LogicalOutput *output : std::as_const(m_outputs)) {
                const auto geo = screenAreas[vd][output].intersected(adjustClientArea(window, output->geometryF()));
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

        m_inRearrange = true;
        m_oldRestrictedAreas = m_restrictedAreas;
        m_restrictedAreas = restrictedAreas;

#if KWIN_BUILD_X11
        if (rootInfo()) {
            for (VirtualDesktop *desktop : desktops) {
                const QRectF &workArea = m_workAreas[desktop];
                NETRect r(Xcb::toXNative(workArea));
                rootInfo()->setWorkArea(desktop->x11DesktopNumber(), r);
            }
        }
#endif

        for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
            if ((*it)->isClient()) {
                (*it)->checkWorkspacePosition();
            }
        }

        m_oldRestrictedAreas.clear(); // reset, no longer valid or needed
        m_inRearrange = false;
    }
}

/**
 * Returns the area available for windows. This is the desktop
 * geometry minus windows on the dock. Placement algorithms should
 * refer to this rather than Screens::geometry.
 */
QRectF Workspace::clientArea(clientAreaOption opt, const LogicalOutput *output, const VirtualDesktop *desktop) const
{
    switch (opt) {
    case MaximizeArea:
    case PlacementArea:
        if (auto desktopIt = m_screenAreas.constFind(desktop); desktopIt != m_screenAreas.constEnd()) {
            if (auto outputIt = desktopIt->constFind(output); outputIt != desktopIt->constEnd()) {
                return *outputIt;
            }
        }
        return output->geometryF();
    case MaximizeFullArea:
    case FullScreenArea:
    case MovementArea:
    case ScreenArea:
        return output->geometryF();
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

QRectF Workspace::clientArea(clientAreaOption opt, const Window *window, const LogicalOutput *output) const
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

StrutRects Workspace::restrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas) const
{
    const StrutRects strut = m_restrictedAreas.value(desktop);
    if (areas == StrutAreaAll) {
        return strut;
    }

    StrutRects ret;
    ret.reserve(strut.size());
    for (const StrutRect &rect : strut) {
        if (rect.area() & areas) {
            ret.append(rect);
        }
    }
    return ret;
}

bool Workspace::inRearrange() const
{
    return m_inRearrange;
}

StrutRects Workspace::previousRestrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas) const
{
    const StrutRects strut = m_oldRestrictedAreas.value(desktop);
    if (areas == StrutAreaAll) {
        return strut;
    }

    StrutRects ret;
    ret.reserve(strut.size());
    for (const StrutRect &rect : strut) {
        if (rect.area() & areas) {
            ret.append(rect);
        }
    }
    return ret;
}

QHash<const LogicalOutput *, QRect> Workspace::previousScreenSizes() const
{
    return m_oldScreenGeometries;
}

#if KWIN_BUILD_X11
LogicalOutput *Workspace::xineramaIndexToOutput(int index) const
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return nullptr;
    }

    xcb_randr_get_monitors_cookie_t cookie = xcb_randr_get_monitors(kwinApp()->x11Connection(), kwinApp()->x11RootWindow(), 1);
    const UniqueCPtr<xcb_randr_get_monitors_reply_t> monitors(xcb_randr_get_monitors_reply(kwinApp()->x11Connection(), cookie, nullptr));
    if (!monitors) {
        return nullptr;
    }

    xcb_randr_monitor_info_t *monitorInfo = nullptr;
    for (auto it = xcb_randr_get_monitors_monitors_iterator(monitors.get()); it.rem; xcb_randr_monitor_info_next(&it)) {
        const int current = monitors->nMonitors - it.rem;
        if (current == index) {
            monitorInfo = it.data;
            break;
        }
    }

    if (!monitorInfo) {
        return nullptr;
    }

    return findOutput(Xcb::atomName(monitorInfo->name));
}
#endif

void Workspace::updateOutputOrder()
{
    auto previousOutputOrder = std::move(m_outputOrder);
    m_outputOrder = m_outputs;
    std::ranges::stable_sort(m_outputOrder, [](LogicalOutput *l, LogicalOutput *r) {
        return l->backendOutput()->priority() < r->backendOutput()->priority();
    });
    if (m_outputOrder != previousOutputOrder) {
        Q_EMIT outputOrderChanged();
    }
}

QList<LogicalOutput *> Workspace::outputOrder() const
{
    return m_outputOrder;
}

LogicalOutput *Workspace::activeOutput() const
{
    return m_activeOutput;
}

void Workspace::setActiveOutput(LogicalOutput *output)
{
    m_activeOutput = output;
}

void Workspace::setActiveOutput(const QPointF &pos)
{
    setActiveOutput(outputAt(pos));
}

static bool canSnap(const Window *window, const Window *other)
{
    if (other == window) {
        return false;
    }
    if (!other->isShown()) {
        return false;
    }
    if (!other->isOnCurrentDesktop()) {
        return false;
    }
    if (!other->isOnCurrentActivity()) {
        return false;
    }
    return !(other->isUnmanaged() || other->isDesktop() || other->isSplash() || other->isNotification() || other->isCriticalNotification() || other->isOnScreenDisplay() || other->isAppletPopup() || other->isDock());
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
QPointF Workspace::adjustWindowPosition(const Window *window, QPointF pos, bool unrestricted, double snapAdjust) const
{
    QSizeF borderSnapZone(options->borderSnapZone(), options->borderSnapZone());
    QRectF maxRect;
    int guideMaximized = MaximizeRestore;
    if (window->maximizeMode() != MaximizeRestore) {
        maxRect = clientArea(MaximizeArea, window, pos + window->rect().center());
        QRectF geo = window->frameGeometry();
        if (window->maximizeMode() & MaximizeHorizontal && (geo.x() == maxRect.left() || geo.right() == maxRect.right())) {
            guideMaximized |= MaximizeHorizontal;
            borderSnapZone.setWidth(std::max(borderSnapZone.width() + 2, maxRect.width() / 16));
        }
        if (window->maximizeMode() & MaximizeVertical && (geo.y() == maxRect.top() || geo.bottom() == maxRect.bottom())) {
            guideMaximized |= MaximizeVertical;
            borderSnapZone.setHeight(std::max(borderSnapZone.height() + 2, maxRect.height() / 16));
        }
    }

    if (options->windowSnapZone() || !borderSnapZone.isNull() || options->centerSnapZone()) {

        const bool sOWO = options->isSnapOnlyWhenOverlapping();
        const LogicalOutput *output = outputAt(pos + window->rect().center());
        if (maxRect.isNull()) {
            maxRect = clientArea(MaximizeArea, window, output);
        }
        const qreal xmin = maxRect.left();
        const qreal xmax = maxRect.right(); // desk size
        const qreal ymin = maxRect.top();
        const qreal ymax = maxRect.bottom();

        const qreal cx(pos.x());
        const qreal cy(pos.y());
        const qreal cw(window->width());
        const qreal ch(window->height());
        const qreal rx(cx + cw);
        const qreal ry(cy + ch); // these don't change

        qreal nx(cx), ny(cy); // buffers
        qreal deltaX(xmax);
        qreal deltaY(ymax); // minimum distance to other windows

        qreal lx, ly, lrx, lry; // coords and size for the comparison window, l

        // border snap
        const qreal borderXSnapZone = borderSnapZone.width() * snapAdjust; // snap trigger
        const qreal borderYSnapZone = borderSnapZone.height() * snapAdjust;
        if (borderXSnapZone > 0 || borderYSnapZone > 0) {
            if ((sOWO ? (cx < xmin) : true) && (std::abs(xmin - cx) < borderXSnapZone)) {
                deltaX = xmin - cx;
                nx = xmin;
            }
            if ((sOWO ? (rx > xmax) : true) && (std::abs(rx - xmax) < borderXSnapZone) && (std::abs(xmax - rx) < deltaX)) {
                deltaX = rx - xmax;
                nx = xmax - cw;
            }

            if ((sOWO ? (cy < ymin) : true) && (std::abs(ymin - cy) < borderYSnapZone)) {
                deltaY = ymin - cy;
                ny = ymin;
            }
            if ((sOWO ? (ry > ymax) : true) && (std::abs(ry - ymax) < borderYSnapZone) && (std::abs(ymax - ry) < deltaY)) {
                deltaY = ry - ymax;
                ny = ymax - ch;
            }
        }

        // windows snap
        const qreal windowSnapZone = options->windowSnapZone() * snapAdjust;
        if (windowSnapZone > 0) {
            for (auto l = m_windows.constBegin(); l != m_windows.constEnd(); ++l) {
                if (!canSnap(window, (*l))) {
                    continue;
                }

                lx = (*l)->x();
                ly = (*l)->y();
                lrx = lx + (*l)->width();
                lry = ly + (*l)->height();

                if (!(guideMaximized & MaximizeHorizontal) && (cy <= lry) && (ly <= ry)) {
                    if ((sOWO ? (cx < lrx) : true) && (std::abs(lrx - cx) < windowSnapZone) && (std::abs(lrx - cx) < deltaX)) {
                        deltaX = std::abs(lrx - cx);
                        nx = lrx;
                    }
                    if ((sOWO ? (rx > lx) : true) && (std::abs(rx - lx) < windowSnapZone) && (std::abs(rx - lx) < deltaX)) {
                        deltaX = std::abs(rx - lx);
                        nx = lx - cw;
                    }
                }

                if (!(guideMaximized & MaximizeVertical) && (cx <= lrx) && (lx <= rx)) {
                    if ((sOWO ? (cy < lry) : true) && (std::abs(lry - cy) < windowSnapZone) && (std::abs(lry - cy) < deltaY)) {
                        deltaY = std::abs(lry - cy);
                        ny = lry;
                    }
                    // if ( (std::abs( ry-ly ) < snap) && (std::abs( ry - ly ) < deltaY ))
                    if ((sOWO ? (ry > ly) : true) && (std::abs(ry - ly) < windowSnapZone) && (std::abs(ry - ly) < deltaY)) {
                        deltaY = std::abs(ry - ly);
                        ny = ly - ch;
                    }
                }

                // Corner snapping
                if (!(guideMaximized & MaximizeVertical) && (nx == lrx || nx + cw == lx)) {
                    if ((sOWO ? (ry > lry) : true) && (std::abs(lry - ry) < windowSnapZone) && (std::abs(lry - ry) < deltaY)) {
                        deltaY = std::abs(lry - ry);
                        ny = lry - ch;
                    }
                    if ((sOWO ? (cy < ly) : true) && (std::abs(cy - ly) < windowSnapZone) && (std::abs(cy - ly) < deltaY)) {
                        deltaY = std::abs(cy - ly);
                        ny = ly;
                    }
                }
                if (!(guideMaximized & MaximizeHorizontal) && (ny == lry || ny + ch == ly)) {
                    if ((sOWO ? (rx > lrx) : true) && (std::abs(lrx - rx) < windowSnapZone) && (std::abs(lrx - rx) < deltaX)) {
                        deltaX = std::abs(lrx - rx);
                        nx = lrx - cw;
                    }
                    if ((sOWO ? (cx < lx) : true) && (std::abs(cx - lx) < windowSnapZone) && (std::abs(cx - lx) < deltaX)) {
                        deltaX = std::abs(cx - lx);
                        nx = lx;
                    }
                }
            }
        }

        // center snap
        const qreal centerSnapZone = options->centerSnapZone() * snapAdjust;
        if (centerSnapZone > 0) {
            qreal diffX = std::abs((xmin + xmax) / 2 - (cx + cw / 2));
            qreal diffY = std::abs((ymin + ymax) / 2 - (cy + ch / 2));
            if (diffX < centerSnapZone && diffY < centerSnapZone && diffX < deltaX && diffY < deltaY) {
                // Snap to center of screen
                nx = (xmin + xmax) / 2 - cw / 2;
                ny = (ymin + ymax) / 2 - ch / 2;
            } else if (options->borderSnapZone() > 0) {
                // Enhance border snap
                if ((nx == xmin || nx == xmax - cw) && diffY < centerSnapZone && diffY < deltaY) {
                    // Snap to vertical center on screen edge
                    ny = (ymin + ymax) / 2 - ch / 2;
                } else if (((unrestricted ? ny == ymin : ny <= ymin) || ny == ymax - ch) && diffX < centerSnapZone && diffX < deltaX) {
                    // Snap to horizontal center on screen edge
                    nx = (xmin + xmax) / 2 - cw / 2;
                }
            }
        }

        pos = QPointF(nx, ny);
    }
    return pos;
}

QRectF Workspace::adjustWindowSize(const Window *window, QRectF moveResizeGeom, Gravity gravity) const
{
    // adapted from adjustWindowPosition on 29May2004
    // this function is called when resizing a window and will modify
    // the new dimensions to snap to other windows/borders if appropriate
    if (options->windowSnapZone() || options->borderSnapZone()) { // || options->centerSnapZone )
        const bool sOWO = options->isSnapOnlyWhenOverlapping();

        const QRectF maxRect = clientArea(MaximizeArea, window, window->rect().center());
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

#define SNAP_BORDER_TOP                                                        \
    if ((sOWO ? (newcy < ymin) : true) && (std::abs(ymin - newcy) < deltaY)) { \
        deltaY = std::abs(ymin - newcy);                                       \
        newcy = ymin;                                                          \
    }

#define SNAP_BORDER_BOTTOM                                                     \
    if ((sOWO ? (newry > ymax) : true) && (std::abs(ymax - newry) < deltaY)) { \
        deltaY = std::abs(ymax - newcy);                                       \
        newry = ymax;                                                          \
    }

#define SNAP_BORDER_LEFT                                                       \
    if ((sOWO ? (newcx < xmin) : true) && (std::abs(xmin - newcx) < deltaX)) { \
        deltaX = std::abs(xmin - newcx);                                       \
        newcx = xmin;                                                          \
    }

#define SNAP_BORDER_RIGHT                                                      \
    if ((sOWO ? (newrx > xmax) : true) && (std::abs(xmax - newrx) < deltaX)) { \
        deltaX = std::abs(xmax - newrx);                                       \
        newrx = xmax;                                                          \
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
            for (auto l = m_windows.constBegin(); l != m_windows.constEnd(); ++l) {
                if (canSnap(window, (*l))) {
                    lx = (*l)->x();
                    ly = (*l)->y();
                    lrx = (*l)->x() + (*l)->width();
                    lry = (*l)->y() + (*l)->height();

#define WITHIN_HEIGHT ((newcy <= lry) && (ly <= newry))

#define WITHIN_WIDTH ((cx <= lrx) && (lx <= rx))

#define SNAP_WINDOW_TOP                        \
    if ((sOWO ? (newcy < lry) : true)          \
        && WITHIN_WIDTH                        \
        && (std::abs(lry - newcy) < deltaY)) { \
        deltaY = std::abs(lry - newcy);        \
        newcy = lry;                           \
    }

#define SNAP_WINDOW_BOTTOM                    \
    if ((sOWO ? (newry > ly) : true)          \
        && WITHIN_WIDTH                       \
        && (std::abs(ly - newry) < deltaY)) { \
        deltaY = std::abs(ly - newry);        \
        newry = ly;                           \
    }

#define SNAP_WINDOW_LEFT                       \
    if ((sOWO ? (newcx < lrx) : true)          \
        && WITHIN_HEIGHT                       \
        && (std::abs(lrx - newcx) < deltaX)) { \
        deltaX = std::abs(lrx - newcx);        \
        newcx = lrx;                           \
    }

#define SNAP_WINDOW_RIGHT                     \
    if ((sOWO ? (newrx > lx) : true)          \
        && WITHIN_HEIGHT                      \
        && (std::abs(lx - newrx) < deltaX)) { \
        deltaX = std::abs(lx - newrx);        \
        newrx = lx;                           \
    }

#define SNAP_WINDOW_C_TOP                   \
    if ((sOWO ? (newcy < ly) : true)        \
        && (newcx == lrx || newrx == lx)    \
        && std::abs(ly - newcy) < deltaY) { \
        deltaY = std::abs(ly - newcy);      \
        newcy = ly;                         \
    }

#define SNAP_WINDOW_C_BOTTOM                 \
    if ((sOWO ? (newry > lry) : true)        \
        && (newcx == lrx || newrx == lx)     \
        && std::abs(lry - newry) < deltaY) { \
        deltaY = std::abs(lry - newry);      \
        newry = lry;                         \
    }

#define SNAP_WINDOW_C_LEFT                  \
    if ((sOWO ? (newcx < lx) : true)        \
        && (newcy == lry || newry == ly)    \
        && std::abs(lx - newcx) < deltaX) { \
        deltaX = std::abs(lx - newcx);      \
        newcx = lx;                         \
    }

#define SNAP_WINDOW_C_RIGHT                  \
    if ((sOWO ? (newrx > lrx) : true)        \
        && (newcy == lry || newry == ly)     \
        && std::abs(lrx - newrx) < deltaX) { \
        deltaX = std::abs(lrx - newrx);      \
        newrx = lrx;                         \
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

#if KWIN_BUILD_X11
// When kwin crashes, windows will not be gravitated back to their original position
// and will remain offset by the size of the decoration. So when restarting, fix this
// (the property with the size of the frame remains on the window after the crash).
void Workspace::fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geometry)
{
    NETWinInfo i(kwinApp()->x11Connection(), w, kwinApp()->x11RootWindow(), NET::WMFrameExtents, NET::Properties2());
    NETStrut frame = i.frameExtents();

    if (frame.left != 0 || frame.top != 0) {
        // left and top needed due to narrowing conversations restrictions in C++11
        const int32_t left = frame.left;
        const int32_t top = frame.top;
        const uint32_t values[] = {Xcb::toXNative(geometry->x - left), Xcb::toXNative(geometry->y - top)};
        xcb_configure_window(kwinApp()->x11Connection(), w, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }
}
#endif

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

Outline *Workspace::outline() const
{
    return m_outline.get();
}

Placement *Workspace::placement() const
{
    return m_placement.get();
}

RuleBook *Workspace::rulebook() const
{
    return m_rulebook.get();
}

ScreenEdges *Workspace::screenEdges() const
{
    return m_screenEdges.get();
}

TileManager *Workspace::tileManager(LogicalOutput *output) const
{
    if (auto search = m_tileManagers.find(output); search != m_tileManagers.end()) {
        return search->second.get();
    } else {
        return nullptr;
    }
}

RootTile *Workspace::rootTile(LogicalOutput *output) const
{
    return rootTile(output, VirtualDesktopManager::self()->currentDesktop());
}

RootTile *Workspace::rootTile(LogicalOutput *output, VirtualDesktop *desktop) const
{
    if (auto manager = tileManager(output)) {
        return manager->rootTile(desktop);
    }
    return nullptr;
}

#if KWIN_BUILD_TABBOX
TabBox::TabBox *Workspace::tabbox() const
{
    return m_tabbox.get();
}
#endif

#if KWIN_BUILD_ACTIVITIES
Activities *Workspace::activities() const
{
    return m_activities.get();
}
#endif

} // namespace

#include "moc_workspace.cpp"
