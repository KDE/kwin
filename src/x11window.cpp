/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// own
#include "x11window.h"
// kwin
#if KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "atoms.h"
#include "client_machine.h"
#include "core/output.h"
#include "cursor.h"
#include "decorations/decoratedwindow.h"
#include "decorations/decorationbridge.h"
#include "focuschain.h"
#include "group.h"
#include "killprompt.h"
#include "netinfo.h"
#include "placement.h"
#include "scene/windowitem.h"
#include "shadow.h"
#include "virtualdesktops.h"
#include "wayland/surface.h"
#include "wayland/xwaylandshell_v1.h"
#include "wayland_server.h"
#include "workspace.h"
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
// KDE
#include <KApplicationTrader>
#include <KStartupInfo>
#include <KX11Extras>
// Qt
#include <QProcess>
// xcb
#include <xcb/xcb_icccm.h>
// system
#include <unistd.h>
// c++
#include <cmath>
#include <csignal>

namespace KWin
{

// window types that are supported as normal windows (i.e. KWin actually manages them)
const NET::WindowTypes SUPPORTED_MANAGED_WINDOW_TYPES_MASK = NET::NormalMask
    | NET::DesktopMask
    | NET::DockMask
    | NET::ToolbarMask
    | NET::MenuMask
    | NET::DialogMask
    /*| NET::OverrideMask*/
    | NET::TopMenuMask
    | NET::UtilityMask
    | NET::SplashMask
    | NET::NotificationMask
    | NET::OnScreenDisplayMask
    | NET::CriticalNotificationMask
    | NET::AppletPopupMask;

// window types that are supported as unmanaged (mainly for compositing)
const NET::WindowTypes SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK = NET::NormalMask
    | NET::DesktopMask
    | NET::DockMask
    | NET::ToolbarMask
    | NET::MenuMask
    | NET::DialogMask
    /*| NET::OverrideMask*/
    | NET::TopMenuMask
    | NET::UtilityMask
    | NET::SplashMask
    | NET::DropdownMenuMask
    | NET::PopupMenuMask
    | NET::TooltipMask
    | NET::NotificationMask
    | NET::ComboBoxMask
    | NET::DNDIconMask
    | NET::OnScreenDisplayMask
    | NET::CriticalNotificationMask;

// Creating a client:
//  - only by calling Workspace::createClient()
//      - it creates a new client and calls manage() for it
//
// Destroying a client:
//  - destroyWindow() - only when the window itself has been destroyed
//      - releaseWindow() - the window is kept, only the client itself is destroyed

/**
 * \class Client x11client.h
 * \brief The Client class encapsulates a window decoration frame.
 */

/**
 * This ctor is "dumb" - it only initializes data. All the real initialization
 * is done in manage().
 */
X11Window::X11Window()
    : Window()
    , m_client()
    , m_managed(false)
    , m_transientForId(XCB_WINDOW_NONE)
    , m_originalTransientForId(XCB_WINDOW_NONE)
    , m_motif(atoms->motif_wm_hints)
    , in_group(nullptr)
    , ping_timer(nullptr)
    , m_pingTimestamp(XCB_TIME_CURRENT_TIME)
    , m_userTime(XCB_TIME_CURRENT_TIME) // Not known yet
    , allowed_actions()
    , sm_stacking_order(-1)
    , activitiesDefined(false)
    , sessionActivityOverride(false)
    , m_focusOutTimer(nullptr)
{
    setOutput(workspace()->activeOutput());
    setMoveResizeOutput(workspace()->activeOutput());

    // TODO: Do all as initialization
    m_syncRequest.counter = m_syncRequest.alarm = XCB_NONE;
    m_syncRequest.timeout = nullptr;
    m_syncRequest.lastTimestamp = xTime();
    m_syncRequest.enabled = false;
    m_syncRequest.pending = false;
    m_syncRequest.acked = false;
    m_syncRequest.interactiveResize = false;

    // Set the initial mapping state
    mapping_state = Withdrawn;

    info = nullptr;

    m_fullscreenMode = FullScreenNone;
    noborder = false;
    app_noborder = false;
    ignore_focus_stealing = false;
    check_active_modal = false;

    max_mode = MaximizeRestore;

    connect(clientMachine(), &ClientMachine::localhostChanged, this, &X11Window::updateCaption);
    connect(options, &Options::condensedTitleChanged, this, &X11Window::updateCaption);

    m_releaseTimer.setSingleShot(true);
    connect(&m_releaseTimer, &QTimer::timeout, this, [this]() {
        releaseWindow();
    });

    // SELI TODO: Initialize xsizehints??
}

/**
 * "Dumb" destructor.
 */
X11Window::~X11Window()
{
    delete info;

    if (m_killPrompt) {
        m_killPrompt->quit();
    }

    Q_ASSERT(!isInteractiveMoveResize());
    Q_ASSERT(!check_active_modal);
    Q_ASSERT(m_blockGeometryUpdates == 0);
}

std::unique_ptr<WindowItem> X11Window::createItem(Item *parentItem)
{
    return std::make_unique<WindowItemX11>(this, parentItem);
}

void X11Window::doSetNextTargetScale()
{
    // the decoration will target the screen's scale,
    // which may not be the same as Xwayland scale
    // but there isn't really any other good option here
    setTargetScale(nextTargetScale());
}

// Use destroyWindow() or releaseWindow(), Client instances cannot be deleted directly
void X11Window::deleteClient(X11Window *c)
{
    delete c;
}

bool X11Window::hasScheduledRelease() const
{
    return m_releaseTimer.isActive();
}

/**
 * Releases the window. The client has done its job and the window is still existing.
 */
void X11Window::releaseWindow(bool on_shutdown)
{
    destroyWindowManagementInterface();

    markAsDeleted();
    Q_EMIT closed();

    if (isUnmanaged()) {
        m_releaseTimer.stop();
        if (Xcb::Extensions::self()->isShapeAvailable()) {
            xcb_shape_select_input(kwinApp()->x11Connection(), window(), false);
        }
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
        workspace()->removeUnmanaged(this);
    } else {
        cleanTabBox();
        if (isInteractiveMoveResize()) {
            Q_EMIT interactiveMoveResizeFinished();
        }
        workspace()->rulebook()->discardUsed(this, true); // Remove ForceTemporarily rules
        StackingUpdatesBlocker blocker(workspace());
        stopDelayedInteractiveMoveResize();
        if (isInteractiveMoveResize()) {
            leaveInteractiveMoveResize();
        }
        finishWindowRules();
        // Grab X during the release to make removing of properties, setting to withdrawn state
        // and repareting to root an atomic operation (https://lists.kde.org/?l=kde-devel&m=116448102901184&w=2)
        grabXServer();
        exportMappingState(XCB_ICCCM_WM_STATE_WITHDRAWN);
        if (!on_shutdown) {
            workspace()->activateNextWindow(this);
        }
        cleanGrouping();
        workspace()->removeX11Window(this);
        if (!on_shutdown) {
            // Only when the window is being unmapped, not when closing down KWin (NETWM sections 5.5,5.7)
            info->setDesktop(0);
            info->setState(NET::States(), info->state()); // Reset all state flags
        }
        if (WinInfo *cinfo = dynamic_cast<WinInfo *>(info)) {
            cinfo->disable();
        }
        m_client.deleteProperty(atoms->kde_net_wm_user_creation_time);
        m_client.deleteProperty(atoms->net_frame_extents);
        m_client.deleteProperty(atoms->kde_net_wm_frame_strut);
        m_client.move(Xcb::toXNative(calculateGravitation(true)));
        m_client.selectInput(XCB_EVENT_MASK_NO_EVENT);
        m_client.reset();
        ungrabXServer();
    }

    if (m_syncRequest.timeout) {
        m_syncRequest.timeout->stop();
    }
    if (m_syncRequest.alarm != XCB_NONE) {
        xcb_sync_destroy_alarm(kwinApp()->x11Connection(), m_syncRequest.alarm);
        m_syncRequest.alarm = XCB_NONE;
    }

    unref();
}

/**
 * Like releaseWindow(), but this one is called when the window has been already destroyed
 * (E.g. The application closed it)
 */
void X11Window::destroyWindow()
{
    destroyWindowManagementInterface();

    markAsDeleted();
    Q_EMIT closed();

    if (isUnmanaged()) {
        m_releaseTimer.stop();
        workspace()->removeUnmanaged(this);
    } else {
        cleanTabBox();
        if (isInteractiveMoveResize()) {
            Q_EMIT interactiveMoveResizeFinished();
        }
        workspace()->rulebook()->discardUsed(this, true); // Remove ForceTemporarily rules
        StackingUpdatesBlocker blocker(workspace());
        stopDelayedInteractiveMoveResize();
        if (isInteractiveMoveResize()) {
            leaveInteractiveMoveResize();
        }
        finishWindowRules();
        workspace()->activateNextWindow(this);
        cleanGrouping();
        workspace()->removeX11Window(this);
        if (WinInfo *cinfo = dynamic_cast<WinInfo *>(info)) {
            cinfo->disable();
        }
        m_client.reset();
    }

    if (m_syncRequest.timeout) {
        m_syncRequest.timeout->stop();
    }
    if (m_syncRequest.alarm != XCB_NONE) {
        xcb_sync_destroy_alarm(kwinApp()->x11Connection(), m_syncRequest.alarm);
        m_syncRequest.alarm = XCB_NONE;
    }

    unref();
}

bool X11Window::track(xcb_window_t w)
{
    XServerGrabber xserverGrabber;
    Xcb::WindowAttributes attr(w);
    Xcb::WindowGeometry geo(w);
    if (attr.isNull() || attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        return false;
    }
    if (attr->_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        return false;
    }
    if (geo.isNull()) {
        return false;
    }

    m_unmanaged = true;

    m_client.reset(w, false);

    auto pidCookie = fetchPid();

    Xcb::selectInput(w, attr->your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    m_bufferGeometry = Xcb::fromXNative(geo.rect());
    m_frameGeometry = Xcb::fromXNative(geo.rect());
    m_clientGeometry = Xcb::fromXNative(geo.rect());
    checkOutput();
    m_visual = attr->visual;
    bit_depth = geo->depth;
    info = new NETWinInfo(kwinApp()->x11Connection(), w, kwinApp()->x11RootWindow(),
                          NET::WMWindowType,
                          NET::WM2Opacity | NET::WM2WindowRole | NET::WM2WindowClass | NET::WM2OpaqueRegion);
    setOpacity(info->opacityF());
    readPid(pidCookie);
    getResourceClass();
    getWmClientLeader();
    getWmClientMachine();
    if (Xcb::Extensions::self()->isShapeAvailable()) {
        xcb_shape_select_input(kwinApp()->x11Connection(), w, true);
    }
    updateShapeRegion();
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    updateShadow();
    setupCompositing();

    return true;
}

bool X11Window::manage(xcb_window_t w, bool isMapped)
{
    StackingUpdatesBlocker stacking_blocker(workspace());

    Xcb::WindowAttributes attr(w);
    Xcb::WindowGeometry windowGeometry(w);
    if (attr.isNull() || windowGeometry.isNull()) {
        return false;
    }

    // From this place on, manage() must not return false
    blockGeometryUpdates();

    embedClient(w, attr->visual, attr->colormap, windowGeometry.rect(), windowGeometry->depth);

    m_visual = attr->visual;
    bit_depth = windowGeometry->depth;

    // SELI TODO: Order all these things in some sane manner

    const NET::Properties properties =
        NET::WMDesktop | NET::WMState | NET::WMWindowType | NET::WMName | NET::WMIcon | NET::WMIconName;
    const NET::Properties2 properties2 =
        NET::WM2WindowClass | NET::WM2WindowRole | NET::WM2UserTime | NET::WM2StartupId | NET::WM2Opacity | NET::WM2FullscreenMonitors | NET::WM2GroupLeader | NET::WM2Urgency | NET::WM2Input | NET::WM2Protocols | NET::WM2InitialMappingState | NET::WM2IconPixmap | NET::WM2OpaqueRegion | NET::WM2DesktopFileName | NET::WM2GTKFrameExtents | NET::WM2GTKApplicationId;

    auto wmClientLeaderCookie = fetchWmClientLeader();
    auto skipCloseAnimationCookie = fetchSkipCloseAnimation();
    auto colorSchemeCookie = fetchPreferredColorScheme();
    auto transientCookie = fetchTransient();
    auto activitiesCookie = fetchActivities();
    auto applicationMenuServiceNameCookie = fetchApplicationMenuServiceName();
    auto applicationMenuObjectPathCookie = fetchApplicationMenuObjectPath();
    auto pidCookie = fetchPid();

    m_geometryHints.init(window());
    m_motif.init(window());
    info = new WinInfo(this, m_client, kwinApp()->x11RootWindow(), properties, properties2);

    if (isDesktop() && bit_depth == 32) {
        // force desktop windows to be opaque. It's a desktop after all, there is no window below
        bit_depth = 24;
    }

    // If it's already mapped, ignore hint
    bool init_minimize = !isMapped && (info->initialMappingState() == NET::Iconic);

    readPid(pidCookie);
    getResourceClass();
    readWmClientLeader(wmClientLeaderCookie);
    getWmClientMachine();
    getSyncCounter();
    setCaption(readName());

    // Sending ConfigureNotify is done when setting mapping state below, getting the
    // first sync response means window is ready for compositing.
    //
    // The sync request will block wl_surface commits, and with Xwayland, it is really
    // important that wl_surfaces commits are blocked before the frame window is mapped.
    // Otherwise Xwayland can attach a buffer before the sync request is acked.
    sendSyncRequest();

    setupWindowRules();
    connect(this, &X11Window::windowClassChanged, this, &X11Window::evaluateWindowRules);

    if (Xcb::Extensions::self()->isShapeAvailable()) {
        xcb_shape_select_input(kwinApp()->x11Connection(), window(), true);
    }
    updateShapeRegion();
    detectNoBorder();
    fetchIconicName();
    setClientFrameExtents(info->gtkFrameExtents());

    // Needs to be done before readTransient() because of reading the group
    checkGroup();
    updateUrgency();
    updateAllowedActions(); // Group affects isMinimizable()

    setModal((info->state() & NET::Modal) != 0); // Needs to be valid before handling groups
    readTransientProperty(transientCookie);
    QString desktopFileName = QString::fromUtf8(info->desktopFileName());
    if (desktopFileName.isEmpty()) {
        desktopFileName = QString::fromUtf8(info->gtkApplicationId());
    }
    if (desktopFileName.isEmpty() && !resourceName().isEmpty()) {
        // Fallback to StartupWMClass for legacy apps
        const auto service = KApplicationTrader::query([this](const KService::Ptr &service) {
            return service->property<QString>("StartupWMClass").compare(resourceName(), Qt::CaseInsensitive) == 0;
        });
        if (!service.isEmpty()) {
            desktopFileName = service.constFirst()->desktopEntryName();
        }
    }
    setDesktopFileName(rules()->checkDesktopFile(desktopFileName, true));
    getIcons();
    connect(this, &X11Window::desktopFileNameChanged, this, &X11Window::getIcons);

    m_geometryHints.read();
    getMotifHints();
    getWmOpaqueRegion();
    readSkipCloseAnimation(skipCloseAnimationCookie);
    updateShadow();

    // TODO: Try to obey all state information from info->state()

    setOriginalSkipTaskbar((info->state() & NET::SkipTaskbar) != 0);
    setSkipPager((info->state() & NET::SkipPager) != 0);
    setSkipSwitcher((info->state() & NET::SkipSwitcher) != 0);

    setupCompositing();

    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(window(), asn_id, asn_data);

    // Make sure that the input window is created before we update the stacking order
    updateLayer();

    std::optional<SessionInfo> session = workspace()->sessionManager()->takeSessionInfo(this);
    if (session) {
        init_minimize = session->minimized;
        noborder = session->noBorder;
    }

    setShortcut(rules()->checkShortcut(session ? session->shortcut : QString(), true));

    init_minimize = rules()->checkMinimize(init_minimize, !isMapped);
    noborder = rules()->checkNoBorder(noborder, !isMapped);

    readActivities(activitiesCookie);

    // Initial desktop placement
    std::optional<QList<VirtualDesktop *>> initialDesktops;
    if (session) {
        if (session->onAllDesktops) {
            initialDesktops = QList<VirtualDesktop *>{};
        } else {
            VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(session->desktop);
            if (desktop) {
                initialDesktops = QList<VirtualDesktop *>{desktop};
            }
        }
        setOnActivities(session->activities);
    } else {
        // If this window is transient, ensure that it is opened on the
        // same window as its parent.  this is necessary when an application
        // starts up on a different desktop than is currently displayed
        if (isTransient()) {
            auto mainwindows = mainWindows();
            bool on_current = false;
            bool on_all = false;
            Window *maincl = nullptr;
            // This is slightly duplicated from Placement::placeOnMainWindow()
            for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
                if (mainwindows.count() > 1 && // A group-transient
                    (*it)->isSpecialWindow() && // Don't consider toolbars etc when placing
                    !(info->state() & NET::Modal)) { // except when it's modal (blocks specials as well)
                    continue;
                }
                maincl = *it;
                if ((*it)->isOnCurrentDesktop()) {
                    on_current = true;
                }
                if ((*it)->isOnAllDesktops()) {
                    on_all = true;
                }
            }
            if (on_all) {
                initialDesktops = QList<VirtualDesktop *>{};
            } else if (on_current) {
                initialDesktops = QList<VirtualDesktop *>{VirtualDesktopManager::self()->currentDesktop()};
            } else if (maincl) {
                initialDesktops = maincl->desktops();
            }

            if (maincl) {
                setOnActivities(maincl->activities());
            }
        } else if (RootInfo::desktopEnabled()) { // a transient shall appear on its leader and not drag that around
            int desktopId = 0;
            if (info->desktop()) {
                desktopId = info->desktop(); // Window had the initial desktop property, force it
            }
            if (desktopId == 0 && asn_valid && asn_data.desktop() != 0) {
                desktopId = asn_data.desktop();
            }
            if (desktopId) {
                if (desktopId == NET::OnAllDesktops) {
                    initialDesktops = QList<VirtualDesktop *>{};
                } else {
                    VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(desktopId);
                    if (desktop) {
                        initialDesktops = QList<VirtualDesktop *>{desktop};
                    }
                }
            }
        }
#if KWIN_BUILD_ACTIVITIES
        if (Workspace::self()->activities() && !isMapped && !skipTaskbar() && isNormalWindow() && !activitiesDefined) {
            // a new, regular window, when we're not recovering from a crash,
            // and it hasn't got an activity. let's try giving it the current one.
            // TODO: decide whether to keep this before the 4.6 release
            // TODO: if we are keeping it (at least as an option), replace noborder checking
            // with a public API for setting windows to be on all activities.
            // something like KWindowSystem::setOnAllActivities or
            // KActivityConsumer::setOnAllActivities
            setOnActivity(Workspace::self()->activities()->current(), true);
        }
#endif
    }

    // If initialDesktops has no value, it means that the client doesn't prefer any
    // desktop so place it on the current virtual desktop.
    if (!initialDesktops.has_value()) {
        if (isDesktop()) {
            initialDesktops = QList<VirtualDesktop *>{};
        } else {
            initialDesktops = QList<VirtualDesktop *>{VirtualDesktopManager::self()->currentDesktop()};
        }
    }
    setDesktops(rules()->checkDesktops(*initialDesktops, !isMapped));
    if (RootInfo::desktopEnabled()) {
        info->setDesktop(desktopId());
    } else {
        info->setDesktop(1);
    }
    workspace()->updateOnAllDesktopsOfTransients(this); // SELI TODO
    // onAllDesktopsChange(); // Decoration doesn't exist here yet

    QStringList activitiesList;
    activitiesList = rules()->checkActivity(activitiesList, !isMapped);
    if (!activitiesList.isEmpty()) {
        setOnActivities(activitiesList);
    }

    QRectF geom = session ? session->geometry : Xcb::fromXNative(windowGeometry.rect());
    bool placementDone = false;

    QRectF area;
    bool partial_keep_in_area = isMapped || session;
    if (isMapped || session) {
        area = workspace()->clientArea(FullArea, this, geom.center());
        checkOffscreenPosition(&geom, area);
    } else {
        Output *output = nullptr;
        if (asn_data.xinerama() != -1) {
            output = workspace()->xineramaIndexToOutput(asn_data.xinerama());
        }
        if (!output) {
            output = workspace()->activeOutput();
        }
        output = rules()->checkOutput(output, !isMapped);
        area = workspace()->clientArea(PlacementArea, this, output->geometry().center());
    }

    if (isDesktop()) {
        // KWin doesn't manage desktop windows
        placementDone = true;
    }

    bool usePosition = false;
    if (isMapped || session || placementDone) {
        placementDone = true; // Use geometry
    } else if (isTransient() && !isUtility() && !isDialog() && !isSplash()) {
        usePosition = true;
    } else if (isTransient() && !hasNETSupport()) {
        usePosition = true;
    } else if (isDialog() && hasNETSupport()) {
        // If the dialog is actually non-NETWM transient window, don't try to apply placement to it,
        // it breaks with too many things (xmms, display)
        if (mainWindows().count() >= 1) {
#if 1
            // #78082 - Ok, it seems there are after all some cases when an application has a good
            // reason to specify a position for its dialog. Too bad other WMs have never bothered
            // with placement for dialogs, so apps always specify positions for their dialogs,
            // including such silly positions like always centered on the screen or under mouse.
            // Using ignoring requested position in window-specific settings helps, and now
            // there's also _NET_WM_FULL_PLACEMENT.
            usePosition = true;
#else
            ; // Force using placement policy
#endif
        } else {
            usePosition = true;
        }
    } else if (isSplash()) {
        ; // Force using placement policy
    } else {
        usePosition = true;
    }
    if (!rules()->checkIgnoreGeometry(!usePosition, true)) {
        if (m_geometryHints.hasPosition()) {
            placementDone = true;
            // Disobey xinerama placement option for now (#70943)
            area = workspace()->clientArea(PlacementArea, this, geom.center());
        }
    }

    if (isMovable() && (geom.x() > area.right() || geom.y() > area.bottom())) {
        placementDone = false; // Weird, do not trust.
    }

    if (placementDone) {
        QPointF position = geom.topLeft();
        // Session contains the position of the frame geometry before gravitating.
        if (!session) {
            position = clientPosToFramePos(position);
        }
        place(position);
    }

    // Create client group if the window will have a decoration
    bool dontKeepInArea = false;
    setColorScheme(readPreferredColorScheme(colorSchemeCookie));

    readApplicationMenuServiceName(applicationMenuServiceNameCookie);
    readApplicationMenuObjectPath(applicationMenuObjectPathCookie);

    updateDecoration(false); // Also gravitates
    // TODO: Is CentralGravity right here, when resizing is done after gravitating?
    const QSizeF constrainedClientSize = constrainClientSize(geom.size());
    resize(rules()->checkSize(clientSizeToFrameSize(constrainedClientSize), !isMapped));

    QPointF forced_pos = rules()->checkPositionSafe(invalidPoint, !isMapped);
    if (forced_pos != invalidPoint) {
        place(forced_pos);
        placementDone = true;
        // Don't keep inside workarea if the window has specially configured position
        partial_keep_in_area = true;
        area = workspace()->clientArea(FullArea, this, geom.center());
    }
    if (!placementDone) {
        // Placement needs to be after setting size
        if (const auto placement = workspace()->placement()->place(this, area)) {
            place(*placement);
        }
        // The client may have been moved to another screen, update placement area.
        area = workspace()->clientArea(PlacementArea, this, moveResizeOutput());
        dontKeepInArea = true;
        placementDone = true;
    }

    // bugs #285967, #286146, #183694
    // geometry() now includes the requested size and the decoration and is at the correct screen/position (hopefully)
    // Maximization for oversized windows must happen NOW.
    // If we effectively pass keepInArea(), the window will resizeWithChecks() - i.e. constrained
    // to the combo of all screen MINUS all struts on the edges
    // If only one screen struts, this will affect screens as a side-effect, the window is artificially shrinked
    // below the screen size and as result no more maximized what breaks KMainWindow's stupid width+1, height+1 hack
    // TODO: get KMainWindow a correct state storage what will allow to store the restore size as well.

    if (!session) { // has a better handling of this
        setGeometryRestore(moveResizeGeometry()); // Remember restore geometry
        if (isMaximizable() && (width() >= area.width() || height() >= area.height())) {
            // Window is too large for the screen, maximize in the
            // directions necessary
            const QSizeF ss = workspace()->clientArea(ScreenArea, this, area.center()).size();
            const QRectF fsa = workspace()->clientArea(FullArea, this, geom.center());
            const QSizeF cs = clientSize();
            int pseudo_max = ((info->state() & NET::MaxVert) ? MaximizeVertical : 0) | ((info->state() & NET::MaxHoriz) ? MaximizeHorizontal : 0);
            if (width() >= area.width()) {
                pseudo_max |= MaximizeHorizontal;
            }
            if (height() >= area.height()) {
                pseudo_max |= MaximizeVertical;
            }

            // heuristics:
            // if decorated client is smaller than the entire screen, the user might want to move it around (multiscreen)
            // in this case, if the decorated client is bigger than the screen (+1), we don't take this as an
            // attempt for maximization, but just constrain the size (the window simply wants to be bigger)
            // NOTICE
            // i intended a second check on cs < area.size() ("the managed client ("minus border") is smaller
            // than the workspace") but gtk / gimp seems to store it's size including the decoration,
            // thus a former maximized window will become non-maximized
            bool keepInFsArea = false;
            if (width() < fsa.width() && (cs.width() > ss.width() + 1)) {
                pseudo_max &= ~MaximizeHorizontal;
                keepInFsArea = true;
            }
            if (height() < fsa.height() && (cs.height() > ss.height() + 1)) {
                pseudo_max &= ~MaximizeVertical;
                keepInFsArea = true;
            }

            if (pseudo_max != MaximizeRestore) {
                maximize((MaximizeMode)pseudo_max);
                // from now on, care about maxmode, since the maximization call will override mode for fix aspects
                dontKeepInArea |= (max_mode == MaximizeFull);
                QRectF savedGeometry; // Use placement when unmaximizing ...
                if (!(max_mode & MaximizeVertical)) {
                    savedGeometry.setY(y()); // ...but only for horizontal direction
                    savedGeometry.setHeight(height());
                }
                if (!(max_mode & MaximizeHorizontal)) {
                    savedGeometry.setX(x()); // ...but only for vertical direction
                    savedGeometry.setWidth(width());
                }
                setGeometryRestore(savedGeometry);
            }
            if (keepInFsArea) {
                moveResize(keepInArea(moveResizeGeometry(), fsa, partial_keep_in_area));
            }
        }
    }

    if ((!isSpecialWindow() || isToolbar()) && isMovable() && !dontKeepInArea) {
        moveResize(keepInArea(moveResizeGeometry(), area, partial_keep_in_area));
    }

    // CT: Extra check for stupid jdk 1.3.1. But should make sense in general
    // if client has initial state set to Iconic and is transient with a parent
    // window that is not Iconic, set init_state to Normal
    if (init_minimize && isTransient()) {
        auto mainwindows = mainWindows();
        for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
            if ((*it)->isShown()) {
                init_minimize = false; // SELI TODO: Even e.g. for NET::Utility?
            }
        }
    }
    // If a dialog is shown for minimized window, minimize it too
    if (!init_minimize && isTransient() && mainWindows().count() > 0 && workspace()->sessionManager()->state() != SessionState::Saving) {
        bool visible_parent = false;
        // Use allMainWindows(), to include also main clients of group transients
        // that have been optimized out in X11Window::checkGroupTransients()
        auto mainwindows = allMainWindows();
        for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
            if ((*it)->isShown()) {
                visible_parent = true;
            }
        }
        if (!visible_parent) {
            init_minimize = true;
            demandAttention();
        }
    }

    setMinimized(init_minimize);

    // Other settings from the previous session
    if (session) {
        // Session restored windows are not considered to be new windows WRT rules,
        // I.e. obey only forcing rules
        setKeepAbove(session->keepAbove);
        setKeepBelow(session->keepBelow);
        setOriginalSkipTaskbar(session->skipTaskbar);
        setSkipPager(session->skipPager);
        setSkipSwitcher(session->skipSwitcher);
        setOpacity(session->opacity);
        setGeometryRestore(session->restore);
        if (session->maximized != MaximizeRestore) {
            maximize(MaximizeMode(session->maximized));
        }
        if (session->fullscreen != FullScreenNone) {
            setFullScreen(true);
            setFullscreenGeometryRestore(session->fsrestore);
        }
        QRectF checkedGeometryRestore = geometryRestore();
        checkOffscreenPosition(&checkedGeometryRestore, area);
        setGeometryRestore(checkedGeometryRestore);
        QRectF checkedFullscreenGeometryRestore = fullscreenGeometryRestore();
        checkOffscreenPosition(&checkedFullscreenGeometryRestore, area);
        setFullscreenGeometryRestore(checkedFullscreenGeometryRestore);
    } else {
        // Window may want to be maximized
        // done after checking that the window isn't larger than the workarea, so that
        // the restore geometry from the checks above takes precedence, and window
        // isn't restored larger than the workarea
        MaximizeMode maxmode = static_cast<MaximizeMode>(
            ((info->state() & NET::MaxVert) ? MaximizeVertical : 0) | ((info->state() & NET::MaxHoriz) ? MaximizeHorizontal : 0));
        MaximizeMode forced_maxmode = rules()->checkMaximize(maxmode, !isMapped);

        // Either hints were set to maximize, or is forced to maximize,
        // or is forced to non-maximize and hints were set to maximize
        if (forced_maxmode != MaximizeRestore || maxmode != MaximizeRestore) {
            maximize(forced_maxmode);
        }

        // Read other initial states
        setKeepAbove(rules()->checkKeepAbove(info->state() & NET::KeepAbove, !isMapped));
        setKeepBelow(rules()->checkKeepBelow(info->state() & NET::KeepBelow, !isMapped));
        setOriginalSkipTaskbar(rules()->checkSkipTaskbar(info->state() & NET::SkipTaskbar, !isMapped));
        setSkipPager(rules()->checkSkipPager(info->state() & NET::SkipPager, !isMapped));
        setSkipSwitcher(rules()->checkSkipSwitcher(info->state() & NET::SkipSwitcher, !isMapped));
        if (info->state() & NET::DemandsAttention) {
            demandAttention();
        }
        if (info->state() & NET::Modal) {
            setModal(true);
        }
        setOpacity(info->opacityF());

        setFullScreen(rules()->checkFullScreen(info->state() & NET::FullScreen, !isMapped));
    }

    updateAllowedActions(true);

    // Set initial user time directly
    m_userTime = readUserTimeMapTimestamp(asn_valid ? &asn_id : nullptr, asn_valid ? &asn_data : nullptr, session.has_value());
    group()->updateUserTime(m_userTime); // And do what X11Window::updateUserTime() does

    // This should avoid flicker, because real restacking is done
    // only after manage() finishes because of blocking, but the window is shown sooner
    m_client.lower();
    if (session && session->stackingOrder != -1) {
        sm_stacking_order = session->stackingOrder;
        workspace()->restoreSessionStackingOrder(this);
    }

    if (isShown()) {
        bool allow;
        if (session) {
            allow = session->active && (!workspace()->wasUserInteraction() || workspace()->activeWindow() == nullptr || workspace()->activeWindow()->isDesktop());
        } else {
            allow = allowWindowActivation(userTime(), false);
        }

        const bool isSessionSaving = workspace()->sessionManager()->state() == SessionState::Saving;

        // If session saving, force showing new windows (i.e. "save file?" dialogs etc.)
        // also force if activation is allowed
        if (!isOnCurrentDesktop() && !isMapped && !session && (allow || isSessionSaving)) {
            VirtualDesktopManager::self()->setCurrent(desktopId());
        }

        // If the window is on an inactive activity during session saving, temporarily force it to show.
        if (!isMapped && !session && isSessionSaving && !isOnCurrentActivity()) {
            setSessionActivityOverride(true);
            const auto windows = mainWindows();
            for (Window *w : windows) {
                if (X11Window *mw = dynamic_cast<X11Window *>(w)) {
                    mw->setSessionActivityOverride(true);
                }
            }
        }

        if (isOnCurrentDesktop() && !isMapped && !allow && (!session || session->stackingOrder < 0)) {
            workspace()->restackWindowUnderActive(this);
        }

        updateVisibility();

        if (!isMapped) {
            if (allow && isOnCurrentDesktop()) {
                if (!isSpecialWindow()) {
                    if (options->focusPolicyIsReasonable() && wantsTabFocus()) {
                        workspace()->requestFocus(this);
                    }
                }
            } else if (!session && !isSpecialWindow()) {
                demandAttention();
            }
        }
    } else {
        updateVisibility();
    }
    Q_ASSERT(mapping_state != Withdrawn);
    m_managed = true;
    blockGeometryUpdates(false);

    if (m_userTime == XCB_TIME_CURRENT_TIME || m_userTime == -1U) {
        // No known user time, set something old
        m_userTime = xTime() - 1000000;
        if (m_userTime == XCB_TIME_CURRENT_TIME || m_userTime == -1U) { // Let's be paranoid
            m_userTime = xTime() - 1000000 + 10;
        }
    }

    // sendSyntheticConfigureNotify(); // Done when setting mapping state

    applyWindowRules(); // Just in case
    workspace()->rulebook()->discardUsed(this, false); // Remove ApplyNow rules
    updateWindowRules(Rules::All); // Was blocked while !isManaged()

    setupWindowManagementInterface();

    connect(kwinApp(), &Application::xwaylandScaleChanged, this, &X11Window::handleXwaylandScaleChanged);
    return true;
}

// Called only from manage()
void X11Window::embedClient(xcb_window_t w, xcb_visualid_t visualid, xcb_colormap_t colormap, const QRect &nativeGeometry, uint8_t depth)
{
    Q_ASSERT(m_client == XCB_WINDOW_NONE);

    m_client.reset(w, false, nativeGeometry);
    m_client.setBorderWidth(0);
    m_client.selectInput(XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE);
}

void X11Window::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force && ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder()))) {
        return;
    }
    QRectF oldgeom = moveResizeGeometry();
    blockGeometryUpdates(true);
    if (force) {
        destroyDecoration();
    }
    if (!noBorder()) {
        createDecoration();
    } else {
        destroyDecoration();
    }
    updateShadow();
    if (check_workspace_pos) {
        checkWorkspacePosition(oldgeom);
    }
    blockGeometryUpdates(false);
    updateFrameExtents();
}

void X11Window::invalidateDecoration()
{
    updateDecoration(true, true);
}

void X11Window::createDecoration()
{
    std::shared_ptr<KDecoration3::Decoration> decoration(Workspace::self()->decorationBridge()->createDecoration(this));
    if (decoration) {
        connect(decoration.get(), &KDecoration3::Decoration::bordersChanged, this, [this]() {
            if (isDeleted()) {
                return;
            }
            const QRectF oldGeometry = moveResizeGeometry();
            checkWorkspacePosition(oldGeometry);
            updateFrameExtents();
        });

        decoration->apply(decoration->nextState()->clone());
        connect(decoration.get(), &KDecoration3::Decoration::nextStateChanged, this, [this](auto state) {
            if (!isDeleted()) {
                m_decoration.decoration->apply(state->clone());
            }
        });
    }
    setDecoration(decoration);

    moveResize(QRectF(calculateGravitation(false), clientSizeToFrameSize(clientSize())));
}

void X11Window::destroyDecoration()
{
    if (isDecorated()) {
        QPointF grav = calculateGravitation(true);
        setDecoration(nullptr);
        moveResize(QRectF(grav, clientSizeToFrameSize(clientSize())));
    }
}

void X11Window::detectNoBorder()
{
    switch (windowType()) {
    case WindowType::Desktop:
    case WindowType::Dock:
    case WindowType::TopMenu:
    case WindowType::Splash:
    case WindowType::Notification:
    case WindowType::OnScreenDisplay:
    case WindowType::CriticalNotification:
    case WindowType::AppletPopup:
        noborder = true;
        app_noborder = true;
        break;
    case WindowType::Unknown:
    case WindowType::Normal:
    case WindowType::Toolbar:
    case WindowType::Menu:
    case WindowType::Dialog:
    case WindowType::Utility:
        noborder = false;
        break;
    default:
        Q_UNREACHABLE();
    }
    // WindowType::Override is some strange beast without clear definition, usually
    // just meaning "noborder", so let's treat it only as such flag, and ignore it as
    // a window type otherwise (SUPPORTED_WINDOW_TYPES_MASK doesn't include it)
    if (WindowType(info->windowType(NET::OverrideMask)) == WindowType::Override) {
        noborder = true;
        app_noborder = true;
    }
}

void X11Window::updateFrameExtents()
{
    NETStrut strut;
    strut.left = Xcb::toXNative(borderLeft());
    strut.right = Xcb::toXNative(borderRight());
    strut.top = Xcb::toXNative(borderTop());
    strut.bottom = Xcb::toXNative(borderBottom());
    info->setFrameExtents(strut);
}

void X11Window::setClientFrameExtents(const NETStrut &strut)
{
    const QMarginsF clientFrameExtents(Xcb::fromXNative(strut.left),
                                       Xcb::fromXNative(strut.top),
                                       Xcb::fromXNative(strut.right),
                                       Xcb::fromXNative(strut.bottom));
    if (m_clientFrameExtents == clientFrameExtents) {
        return;
    }

    m_clientFrameExtents = clientFrameExtents;

    // We should resize the client when its custom frame extents are changed so
    // the logical bounds remain the same. This however means that we will send
    // several configure requests to the application upon restoring it from the
    // maximized or fullscreen state.
    moveResize(moveResizeGeometry());
}

bool X11Window::userNoBorder() const
{
    return noborder;
}

bool X11Window::isFullScreenable() const
{
    if (isUnmanaged()) {
        return false;
    }
    if (!rules()->checkFullScreen(true)) {
        return false;
    }
    // don't check size constrains - some apps request fullscreen despite requesting fixed size
    return isNormalWindow() || isDialog(); // also better disallow only weird types to go fullscreen
}

bool X11Window::noBorder() const
{
    return userNoBorder() || isFullScreen();
}

bool X11Window::userCanSetNoBorder() const
{
    if (isUnmanaged()) {
        return false;
    }

    // Client-side decorations and server-side decorations are mutually exclusive.
    if (isClientSideDecorated()) {
        return false;
    }

    return !isFullScreen();
}

void X11Window::setNoBorder(bool set)
{
    if (!userCanSetNoBorder()) {
        return;
    }
    set = rules()->checkNoBorder(set);
    if (noborder == set) {
        return;
    }
    noborder = set;
    updateDecoration(true, false);
    updateWindowRules(Rules::NoBorder);
    Q_EMIT noBorderChanged();
}

void X11Window::checkNoBorder()
{
    setNoBorder(app_noborder);
}

/**
 * Returns whether the window is minimizable or not
 */
bool X11Window::isMinimizable() const
{
    if (isSpecialWindow() && !isTransient()) {
        return false;
    }
    if (isAppletPopup()) {
        return false;
    }
    if (!rules()->checkMinimize(true)) {
        return false;
    }

    if (isTransient()) {
        // #66868 - Let other xmms windows be minimized when the mainwindow is minimized
        bool shown_mainwindow = false;
        auto mainwindows = mainWindows();
        for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd(); ++it) {
            if ((*it)->isShown()) {
                shown_mainwindow = true;
            }
        }
        if (!shown_mainwindow) {
            return true;
        }
    }
#if 0
    // This is here because kicker's taskbar doesn't provide separate entries
    // for windows with an explicitly given parent
    // TODO: perhaps this should be redone
    // Disabled for now, since at least modal dialogs should be minimizable
    // (resulting in the mainwindow being minimized too).
    if (transientFor() != NULL)
        return false;
#endif
    if (!wantsTabFocus()) { // SELI, TODO: - NET::Utility? why wantsTabFocus() - skiptaskbar? ?
        return false;
    }
    return true;
}

void X11Window::doMinimize()
{
    if (m_managed) {
        if (isMinimized()) {
            workspace()->activateNextWindow(this);
        }
    }
    updateVisibility();
    updateAllowedActions();
    workspace()->updateMinimizedOfTransients(this);
}

void X11Window::updateVisibility()
{
    if (isUnmanaged() || isDeleted()) {
        return;
    }
    if (isHidden()) {
        info->setState(NET::Hidden, NET::Hidden);
        setSkipTaskbar(true); // Also hide from taskbar
        internalHide();
        return;
    }
    if (isHiddenByShowDesktop()) {
        return;
    }
    setSkipTaskbar(originalSkipTaskbar()); // Reset from 'hidden'
    if (isMinimized()) {
        info->setState(NET::Hidden, NET::Hidden);
        internalHide();
        return;
    }
    info->setState(NET::States(), NET::Hidden);
    if (!isOnCurrentDesktop()) {
        internalKeep();
        return;
    }
    if (!isOnCurrentActivity()) {
        internalKeep();
        return;
    }
    internalShow();
}

/**
 * Sets the client window's mapping state. Possible values are
 * WithdrawnState, IconicState, NormalState.
 */
void X11Window::exportMappingState(int s)
{
    Q_ASSERT(m_client != XCB_WINDOW_NONE);
    Q_ASSERT(!isDeleted() || s == XCB_ICCCM_WM_STATE_WITHDRAWN);
    if (s == XCB_ICCCM_WM_STATE_WITHDRAWN) {
        m_client.deleteProperty(atoms->wm_state);
        return;
    }
    Q_ASSERT(s == XCB_ICCCM_WM_STATE_NORMAL || s == XCB_ICCCM_WM_STATE_ICONIC);

    int32_t data[2];
    data[0] = s;
    data[1] = XCB_NONE;
    m_client.changeProperty(atoms->wm_state, atoms->wm_state, 32, 2, data);
}

void X11Window::internalShow()
{
    if (mapping_state == Mapped) {
        return;
    }
    MappingState old = mapping_state;
    mapping_state = Mapped;
    if (old == Unmapped || old == Withdrawn) {
        map();
    }
    if (old == Kept) {
        workspace()->forceRestacking();
    }
}

void X11Window::internalHide()
{
    if (mapping_state == Unmapped) {
        return;
    }
    MappingState old = mapping_state;
    mapping_state = Unmapped;
    if (old == Mapped || old == Kept) {
        unmap();
    }
    if (old == Kept) {
        workspace()->forceRestacking();
    }
}

void X11Window::internalKeep()
{
    if (mapping_state == Kept) {
        return;
    }
    MappingState old = mapping_state;
    mapping_state = Kept;
    if (old == Unmapped || old == Withdrawn) {
        map();
    }
    if (isActive()) {
        workspace()->focusToNull(); // get rid of input focus, bug #317484
    }
    workspace()->forceRestacking();
}

void X11Window::map()
{
    m_client.map();
    exportMappingState(XCB_ICCCM_WM_STATE_NORMAL);
}

void X11Window::unmap()
{
    // Note that unmapping is not racy because if the client wants to unmap the window, it will
    // call xcb_unmap_window() and then send a synthetic UnmapNotify event to the root window, see ICCCM 4.1.4.

    m_client.unmap();
    m_inflightUnmaps++;

    exportMappingState(XCB_ICCCM_WM_STATE_ICONIC);
}

void X11Window::sendClientMessage(xcb_window_t w, xcb_atom_t a, xcb_atom_t protocol, uint32_t data1, uint32_t data2, uint32_t data3)
{
    xcb_client_message_event_t ev{};
    // Every X11 event is 32 bytes (see man xcb_send_event), so XCB will copy
    // 32 unconditionally. Add a static_assert to ensure we don't disclose
    // stack memory.
    static_assert(sizeof(ev) == 32, "Would leak stack data otherwise");
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = w;
    ev.type = a;
    ev.format = 32;
    ev.data.data32[0] = protocol;
    ev.data.data32[1] = xTime();
    ev.data.data32[2] = data1;
    ev.data.data32[3] = data2;
    ev.data.data32[4] = data3;
    uint32_t eventMask = 0;
    if (w == kwinApp()->x11RootWindow()) {
        eventMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT; // Magic!
    }
    xcb_send_event(kwinApp()->x11Connection(), false, w, eventMask, reinterpret_cast<const char *>(&ev));
    xcb_flush(kwinApp()->x11Connection());
}

/**
 * Returns whether the window may be closed (have a close button)
 */
bool X11Window::isCloseable() const
{
    return !isUnmanaged() && rules()->checkCloseable(m_motif.close() && !isSpecialWindow());
}

/**
 * Closes the window by either sending a delete_window message or using XKill.
 */
void X11Window::closeWindow()
{
    if (isDeleted()) {
        return;
    }
    if (!isCloseable()) {
        return;
    }

    // Update user time, because the window may create a confirming dialog.
    updateUserTime();

    if (info->supportsProtocol(NET::DeleteWindowProtocol)) {
        sendClientMessage(window(), atoms->wm_protocols, atoms->wm_delete_window);
        pingWindow();
    } else { // Client will not react on wm_delete_window. We have not choice
        // but destroy his connection to the XServer.
        killWindow();
    }
}

/**
 * Kills the window via XKill
 */
void X11Window::killWindow()
{
    qCDebug(KWIN_CORE) << "X11Window::killWindow():" << window();
    if (isUnmanaged()) {
        xcb_kill_client(kwinApp()->x11Connection(), window());
    } else {
        killProcess(false);
        m_client.kill(); // Always kill this client at the server
        destroyWindow();
    }
}

/**
 * Send a ping to the window using _NET_WM_PING if possible if it
 * doesn't respond within a reasonable time, it will be killed.
 */
void X11Window::pingWindow()
{
    if (!info->supportsProtocol(NET::PingProtocol)) {
        return; // Can't ping :(
    }
    if (options->killPingTimeout() == 0) {
        return; // Turned off
    }
    if (ping_timer != nullptr) {
        return; // Pinging already
    }
    ping_timer = new QTimer(this);
    connect(ping_timer, &QTimer::timeout, this, [this]() {
        if (unresponsive()) {
            qCDebug(KWIN_CORE) << "Final ping timeout, asking to kill:" << caption();
            ping_timer->deleteLater();
            ping_timer = nullptr;
            killProcess(true, m_pingTimestamp);
            return;
        }

        qCDebug(KWIN_CORE) << "First ping timeout:" << caption();

        setUnresponsive(true);
        ping_timer->start();
    });
    ping_timer->setSingleShot(true);
    // we'll run the timer twice, at first we'll desaturate the window
    // and the second time we'll show the "do you want to kill" prompt
    ping_timer->start(options->killPingTimeout() / 2);
    m_pingTimestamp = xTime();
    rootInfo()->sendPing(window(), m_pingTimestamp);
}

void X11Window::gotPing(xcb_timestamp_t timestamp)
{
    // Just plain compare is not good enough because of 64bit and truncating and whatnot
    if (NET::timestampCompare(timestamp, m_pingTimestamp) != 0) {
        return;
    }
    delete ping_timer;
    ping_timer = nullptr;

    setUnresponsive(false);

    if (m_killPrompt) {
        m_killPrompt->quit();
    }
}

void X11Window::killProcess(bool ask, xcb_timestamp_t timestamp)
{
    if (m_killPrompt && m_killPrompt->isRunning()) {
        return;
    }
    Q_ASSERT(!ask || timestamp != XCB_TIME_CURRENT_TIME);
    pid_t processId = pid();
    if (processId <= 0 || clientMachine()->hostName().isEmpty()) { // Needed properties missing
        return;
    }
    qCDebug(KWIN_CORE) << "Kill process:" << processId << "(" << clientMachine()->hostName() << ")";
    if (!ask) {
        if (!clientMachine()->isLocal()) {
            QStringList lst;
            lst << clientMachine()->hostName() << QStringLiteral("kill") << QString::number(processId);
            QProcess::startDetached(QStringLiteral("xon"), lst);
        } else {
            ::kill(processId, SIGTERM);
        }
    } else {
        if (!m_killPrompt) {
            m_killPrompt = std::make_unique<KillPrompt>(this);
        }
        m_killPrompt->start(timestamp);
    }
}

void X11Window::doSetKeepAbove()
{
    if (isDeleted()) {
        return;
    }
    info->setState(keepAbove() ? NET::KeepAbove : NET::States(), NET::KeepAbove);
}

void X11Window::doSetKeepBelow()
{
    if (isDeleted()) {
        return;
    }
    info->setState(keepBelow() ? NET::KeepBelow : NET::States(), NET::KeepBelow);
}

void X11Window::doSetSkipTaskbar()
{
    if (isDeleted()) {
        return;
    }
    info->setState(skipTaskbar() ? NET::SkipTaskbar : NET::States(), NET::SkipTaskbar);
}

void X11Window::doSetSkipPager()
{
    if (isDeleted()) {
        return;
    }
    info->setState(skipPager() ? NET::SkipPager : NET::States(), NET::SkipPager);
}

void X11Window::doSetSkipSwitcher()
{
    if (isDeleted()) {
        return;
    }
    info->setState(skipSwitcher() ? NET::SkipSwitcher : NET::States(), NET::SkipSwitcher);
}

void X11Window::doSetDesktop()
{
    if (isDeleted()) {
        return;
    }
    setNetWmDesktop(m_desktops.isEmpty() ? nullptr : m_desktops.last());
    updateVisibility();
}

void X11Window::setNetWmDesktop(VirtualDesktop *desktop)
{
    if (m_netWmDesktop == desktop) {
        return;
    }
    if (m_netWmDesktop) {
        disconnect(m_netWmDesktop, &VirtualDesktop::x11DesktopNumberChanged, this, &X11Window::updateNetWmDesktopId);
    }
    if (desktop) {
        connect(desktop, &VirtualDesktop::x11DesktopNumberChanged, this, &X11Window::updateNetWmDesktopId);
    }

    m_netWmDesktop = desktop;
    updateNetWmDesktopId();
}

void X11Window::updateNetWmDesktopId()
{
    if (isDeleted()) {
        return;
    }
    if (RootInfo::desktopEnabled()) {
        info->setDesktop(m_netWmDesktop ? m_netWmDesktop->x11DesktopNumber() : -1);
    }
}

void X11Window::doSetDemandsAttention()
{
    if (isDeleted()) {
        return;
    }
    info->setState(isDemandingAttention() ? NET::DemandsAttention : NET::States(), NET::DemandsAttention);
}

void X11Window::doSetHidden()
{
    if (isDeleted()) {
        return;
    }
    updateVisibility();
}

void X11Window::doSetHiddenByShowDesktop()
{
    if (isDeleted()) {
        return;
    }
    updateVisibility();
}

void X11Window::doSetModal()
{
    if (isDeleted()) {
        return;
    }
    info->setState(isModal() ? NET::Modal : NET::States(), NET::Modal);
}

void X11Window::doSetOnActivities(const QStringList &activityList)
{
#if KWIN_BUILD_ACTIVITIES
    if (isDeleted()) {
        return;
    }
    if (activityList.isEmpty()) {
        const QByteArray nullUuid = Activities::nullUuid().toUtf8();
        m_client.changeProperty(atoms->activities, XCB_ATOM_STRING, 8, nullUuid.length(), nullUuid.constData());
    } else {
        QByteArray joined = activityList.join(QStringLiteral(",")).toLatin1();
        m_client.changeProperty(atoms->activities, XCB_ATOM_STRING, 8, joined.length(), joined.constData());
    }
#endif
}

void X11Window::updateActivities(bool includeTransients)
{
    Window::updateActivities(includeTransients);
    if (!m_activityUpdatesBlocked) {
        updateVisibility();
    }
}

/**
 * Returns the list of activities the client window is on.
 * if it's on all activities, the list will be empty.
 * Don't use this, use isOnActivity() and friends (from class Window)
 */
QStringList X11Window::activities() const
{
    if (sessionActivityOverride) {
        return QStringList();
    }
    return Window::activities();
}

/**
 * Performs the actual focusing of the window using XSetInputFocus and WM_TAKE_FOCUS
 */
bool X11Window::takeFocus()
{
    const bool effectiveAcceptFocus = rules()->checkAcceptFocus(info->input());
    const bool effectiveTakeFocus = rules()->checkAcceptFocus(info->supportsProtocol(NET::TakeFocusProtocol));

    if (effectiveAcceptFocus) {
        xcb_void_cookie_t cookie = xcb_set_input_focus_checked(kwinApp()->x11Connection(),
                                                               XCB_INPUT_FOCUS_POINTER_ROOT,
                                                               window(), XCB_TIME_CURRENT_TIME);
        UniqueCPtr<xcb_generic_error_t> error(xcb_request_check(kwinApp()->x11Connection(), cookie));
        if (error) {
            qCWarning(KWIN_CORE, "Failed to focus 0x%x (error %d)", window(), error->error_code);
            return false;
        }
    } else {
        demandAttention(false); // window cannot take input, at least withdraw urgency
    }
    if (effectiveTakeFocus) {
        kwinApp()->updateXTime();
        sendClientMessage(window(), atoms->wm_protocols, atoms->wm_take_focus);
    }

    if (effectiveAcceptFocus || effectiveTakeFocus) {
        workspace()->setShouldGetFocus(this);
    }
    return true;
}

/**
 * Returns whether the window provides context help or not. If it does,
 * you should show a help menu item or a help button like '?' and call
 * contextHelp() if this is invoked.
 *
 * \sa contextHelp()
 */
bool X11Window::providesContextHelp() const
{
    return info->supportsProtocol(NET::ContextHelpProtocol);
}

/**
 * Invokes context help on the window. Only works if the window
 * actually provides context help.
 *
 * \sa providesContextHelp()
 */
void X11Window::showContextHelp()
{
    if (info->supportsProtocol(NET::ContextHelpProtocol)) {
        sendClientMessage(window(), atoms->wm_protocols, atoms->net_wm_context_help);
    }
}

/**
 * Fetches the window's caption (WM_NAME property). It will be
 * stored in the client's caption().
 */
void X11Window::fetchName()
{
    setCaption(readName());
}

static inline QString readNameProperty(xcb_window_t w, xcb_atom_t atom)
{
    const auto cookie = xcb_icccm_get_text_property_unchecked(kwinApp()->x11Connection(), w, atom);
    xcb_icccm_get_text_property_reply_t reply;
    if (xcb_icccm_get_wm_name_reply(kwinApp()->x11Connection(), cookie, &reply, nullptr)) {
        QString retVal;
        if (reply.encoding == atoms->utf8_string) {
            retVal = QString::fromUtf8(QByteArray(reply.name, reply.name_len));
        } else if (reply.encoding == XCB_ATOM_STRING) {
            retVal = QString::fromLatin1(QByteArray(reply.name, reply.name_len));
        }
        xcb_icccm_get_text_property_reply_wipe(&reply);
        return retVal.simplified();
    }
    return QString();
}

QString X11Window::readName() const
{
    if (info->name() && info->name()[0] != '\0') {
        return QString::fromUtf8(info->name()).simplified();
    } else {
        return readNameProperty(window(), XCB_ATOM_WM_NAME);
    }
}

// The list is taken from https://www.unicode.org/reports/tr9/ (#154840)
static const QChar LRM(0x200E);

void X11Window::setCaption(const QString &_s, bool force)
{
    QString s(_s);
    for (int i = 0; i < s.length();) {
        if (!s[i].isPrint()) {
            if (QChar(s[i]).isHighSurrogate() && i + 1 < s.length() && QChar(s[i + 1]).isLowSurrogate()) {
                const uint uc = QChar::surrogateToUcs4(s[i], s[i + 1]);
                if (!QChar::isPrint(uc)) {
                    s.remove(i, 2);
                } else {
                    i += 2;
                }
                continue;
            }
            s.remove(i, 1);
            continue;
        }
        ++i;
    }
    const bool changed = (s != cap_normal);
    if (!force && !changed) {
        return;
    }
    cap_normal = s;

    bool was_suffix = (!cap_suffix.isEmpty());
    cap_suffix.clear();
    QString machine_suffix;
    if (!options->condensedTitle()) { // machine doesn't qualify for "clean"
        if (clientMachine()->hostName() != ClientMachine::localhost() && !clientMachine()->isLocal()) {
            machine_suffix = QLatin1String(" <@") + clientMachine()->hostName() + QLatin1Char('>') + LRM;
        }
    }
    QString shortcut_suffix = shortcutCaptionSuffix();
    cap_suffix = machine_suffix + shortcut_suffix;
    if ((was_suffix && cap_suffix.isEmpty()) || force) {
        // If it was new window, it may have old value still set, if the window is reused
        info->setVisibleName("");
        info->setVisibleIconName("");
    } else if (!cap_suffix.isEmpty() && !cap_iconic.isEmpty()) {
        // Keep the same suffix in iconic name if it's set
        info->setVisibleIconName(QString(cap_iconic + cap_suffix).toUtf8().constData());
    }

    if (changed) {
        Q_EMIT captionNormalChanged();
    }
    Q_EMIT captionChanged();
}

void X11Window::updateCaption()
{
    setCaption(cap_normal, true);
}

void X11Window::fetchIconicName()
{
    QString s;
    if (info->iconName() && info->iconName()[0] != '\0') {
        s = QString::fromUtf8(info->iconName());
    } else {
        s = readNameProperty(window(), XCB_ATOM_WM_ICON_NAME);
    }
    if (s != cap_iconic) {
        bool was_set = !cap_iconic.isEmpty();
        cap_iconic = s;
        if (!cap_suffix.isEmpty()) {
            if (!cap_iconic.isEmpty()) { // Keep the same suffix in iconic name if it's set
                info->setVisibleIconName(QString(s + cap_suffix).toUtf8().constData());
            } else if (was_set) {
                info->setVisibleIconName("");
            }
        }
    }
}

void X11Window::getMotifHints()
{
    const bool wasClosable = isCloseable();
    const bool wasNoBorder = m_motif.noDecorations();
    if (m_managed) { // only on property change, initial read is prefetched
        m_motif.fetch();
    }
    m_motif.read();
    if (m_motif.hasDecorationsFlag() && m_motif.noDecorations() != wasNoBorder) {
        // If we just got a hint telling us to hide decorations, we do so.
        if (m_motif.noDecorations()) {
            noborder = rules()->checkNoBorder(true);
            // If the Motif hint is now telling us to show decorations, we only do so if the app didn't
            // instruct us to hide decorations in some other way, though.
        } else if (!app_noborder) {
            noborder = rules()->checkNoBorder(false);
        }
    }

    // mminimize; - Ignore, bogus - E.g. shading or sending to another desktop is "minimizing" too
    // mmaximize; - Ignore, bogus - Maximizing is basically just resizing
    const bool closabilityChanged = wasClosable != isCloseable();
    if (isManaged()) {
        updateDecoration(true); // Check if noborder state has changed
    }
    if (closabilityChanged) {
        Q_EMIT closeableChanged(isCloseable());
    }
}

void X11Window::getIcons()
{
    if (isUnmanaged()) {
        return;
    }
    // First read icons from the window itself
    const QString themedIconName = iconFromDesktopFile();
    if (!themedIconName.isEmpty()) {
        setIcon(QIcon::fromTheme(themedIconName));
        return;
    }
    QIcon icon;
    auto readIcon = [this, &icon](int size, bool scale = true) {
        const QPixmap pix = KX11Extras::icon(window(), size, size, scale, KX11Extras::NETWM | KX11Extras::WMHints, info);
        if (!pix.isNull()) {
            icon.addPixmap(pix);
        }
    };
    readIcon(16);
    readIcon(32);
    readIcon(48, false);
    readIcon(64, false);
    readIcon(128, false);
    if (icon.isNull()) {
        // Then try window group
        icon = group()->icon();
    }
    if (icon.isNull() && isTransient()) {
        // Then mainwindows
        auto mainwindows = mainWindows();
        for (auto it = mainwindows.constBegin(); it != mainwindows.constEnd() && icon.isNull(); ++it) {
            if (!(*it)->icon().isNull()) {
                icon = (*it)->icon();
                break;
            }
        }
    }
    if (icon.isNull()) {
        // And if nothing else, load icon from classhint or xapp icon
        icon.addPixmap(KX11Extras::icon(window(), 32, 32, true, KX11Extras::ClassHint | KX11Extras::XApp, info));
        icon.addPixmap(KX11Extras::icon(window(), 16, 16, true, KX11Extras::ClassHint | KX11Extras::XApp, info));
        icon.addPixmap(KX11Extras::icon(window(), 64, 64, false, KX11Extras::ClassHint | KX11Extras::XApp, info));
        icon.addPixmap(KX11Extras::icon(window(), 128, 128, false, KX11Extras::ClassHint | KX11Extras::XApp, info));
    }
    setIcon(icon);
}

void X11Window::getSyncCounter()
{
    if (!Xcb::Extensions::self()->isSyncAvailable()) {
        return;
    }

    static bool noXsync = qEnvironmentVariableIntValue("KWIN_X11_NO_SYNC_REQUEST") == 1;
    if (noXsync) {
        return;
    }

    Xcb::Property syncProp(false, window(), atoms->net_wm_sync_request_counter, XCB_ATOM_CARDINAL, 0, 1);
    const xcb_sync_counter_t counter = syncProp.value<xcb_sync_counter_t>(XCB_NONE);
    if (counter != XCB_NONE) {
        m_syncRequest.enabled = true;
        m_syncRequest.counter = counter;
        m_syncRequest.value.hi = 0;
        m_syncRequest.value.lo = 0;
        auto *c = kwinApp()->x11Connection();
        xcb_sync_set_counter(c, m_syncRequest.counter, m_syncRequest.value);
        if (m_syncRequest.alarm == XCB_NONE) {
            const uint32_t mask = XCB_SYNC_CA_COUNTER | XCB_SYNC_CA_VALUE_TYPE | XCB_SYNC_CA_TEST_TYPE | XCB_SYNC_CA_EVENTS;
            const uint32_t values[] = {
                m_syncRequest.counter,
                XCB_SYNC_VALUETYPE_RELATIVE,
                XCB_SYNC_TESTTYPE_POSITIVE_TRANSITION,
                1};
            m_syncRequest.alarm = xcb_generate_id(c);
            auto cookie = xcb_sync_create_alarm_checked(c, m_syncRequest.alarm, mask, values);
            UniqueCPtr<xcb_generic_error_t> error(xcb_request_check(c, cookie));
            if (error) {
                m_syncRequest.alarm = XCB_NONE;
            } else {
                xcb_sync_change_alarm_value_list_t value{};
                value.value.hi = 0;
                value.value.lo = 1;
                value.delta.hi = 0;
                value.delta.lo = 1;
                xcb_sync_change_alarm_aux(c, m_syncRequest.alarm, XCB_SYNC_CA_DELTA | XCB_SYNC_CA_VALUE, &value);
            }
        }
    }
}

/**
 * Send the client a _NET_SYNC_REQUEST
 */
void X11Window::sendSyncRequest()
{
    if (!m_syncRequest.enabled || m_syncRequest.pending) {
        return; // do NOT, NEVER send a sync request when there's one on the stack. the clients will just stop responding. FOREVER! ...
    }

    if (!m_syncRequest.timeout) {
        m_syncRequest.timeout = new QTimer(this);
        m_syncRequest.timeout->setSingleShot(true);
        connect(m_syncRequest.timeout, &QTimer::timeout, this, &X11Window::ackSyncTimeout);
    }
    m_syncRequest.timeout->start(ready_for_painting ? 10000 : 1000);

    // We increment before the notify so that after the notify
    // syncCounterSerial will equal the value we are expecting
    // in the acknowledgement
    const uint32_t oldLo = m_syncRequest.value.lo;
    m_syncRequest.value.lo++;
    if (oldLo > m_syncRequest.value.lo) {
        m_syncRequest.value.hi++;
    }
    if (m_syncRequest.lastTimestamp >= xTime()) {
        kwinApp()->updateXTime();
    }

    setAllowCommits(false);
    sendClientMessage(window(), atoms->wm_protocols, atoms->net_wm_sync_request,
                      m_syncRequest.value.lo, m_syncRequest.value.hi);
    m_syncRequest.pending = true;
    m_syncRequest.interactiveResize = isInteractiveResize();
    m_syncRequest.lastTimestamp = xTime();
}

bool X11Window::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus() || info->supportsProtocol(NET::TakeFocusProtocol));
}

bool X11Window::acceptsFocus() const
{
    return info->input();
}

void X11Window::doSetQuickTileMode()
{
    commitTile(requestedTile());
}

void X11Window::updateAllowedActions(bool force)
{
    if (!isManaged() && !force) {
        return;
    }
    NET::Actions old_allowed_actions = NET::Actions(allowed_actions);
    allowed_actions = NET::Actions();
    if (isMovable()) {
        allowed_actions |= NET::ActionMove;
    }
    if (isResizable()) {
        allowed_actions |= NET::ActionResize;
    }
    if (isMinimizable()) {
        allowed_actions |= NET::ActionMinimize;
    }
    // Sticky state not supported
    if (isMaximizable()) {
        allowed_actions |= NET::ActionMax;
    }
    if (isFullScreenable()) {
        allowed_actions |= NET::ActionFullScreen;
    }
    allowed_actions |= NET::ActionChangeDesktop; // Always (Pagers shouldn't show Docks etc.)
    if (isCloseable()) {
        allowed_actions |= NET::ActionClose;
    }
    if (old_allowed_actions == allowed_actions) {
        return;
    }
    // TODO: This could be delayed and compressed - It's only for pagers etc. anyway
    info->setAllowedActions(allowed_actions);
    // ONLY if relevant features have changed (and the window didn't just get/loose moveresize for maximization state changes)
    const NET::Actions relevant = ~(NET::ActionMove | NET::ActionResize);
    if ((allowed_actions & relevant) != (old_allowed_actions & relevant)) {
        if ((allowed_actions & NET::ActionMinimize) != (old_allowed_actions & NET::ActionMinimize)) {
            Q_EMIT minimizeableChanged(allowed_actions & NET::ActionMinimize);
        }
        if ((allowed_actions & NET::ActionMax) != (old_allowed_actions & NET::ActionMax)) {
            Q_EMIT maximizeableChanged(allowed_actions & NET::ActionMax);
        }
        if ((allowed_actions & NET::ActionClose) != (old_allowed_actions & NET::ActionClose)) {
            Q_EMIT closeableChanged(allowed_actions & NET::ActionClose);
        }
    }
}

Xcb::StringProperty X11Window::fetchActivities() const
{
#if KWIN_BUILD_ACTIVITIES
    return Xcb::StringProperty(window(), atoms->activities);
#else
    return Xcb::StringProperty();
#endif
}

void X11Window::readActivities(Xcb::StringProperty &property)
{
#if KWIN_BUILD_ACTIVITIES
    QString prop = QString::fromUtf8(property);
    activitiesDefined = !prop.isEmpty();

    if (prop == Activities::nullUuid()) {
        // copied from setOnAllActivities to avoid a redundant XChangeProperty.
        if (!m_activityList.isEmpty()) {
            m_activityList.clear();
            updateActivities(true);
        }
        return;
    }
    if (prop.isEmpty()) {
        // note: this makes it *act* like it's on all activities but doesn't set the property to 'ALL'
        if (!m_activityList.isEmpty()) {
            m_activityList.clear();
            updateActivities(true);
        }
        return;
    }

    const QStringList newActivitiesList = prop.split(u',');

    if (newActivitiesList == m_activityList) {
        return; // expected change, it's ok.
    }

    setOnActivities(newActivitiesList);
#endif
}

void X11Window::checkActivities()
{
#if KWIN_BUILD_ACTIVITIES
    Xcb::StringProperty property = fetchActivities();
    readActivities(property);
#endif
}

void X11Window::setSessionActivityOverride(bool needed)
{
    sessionActivityOverride = needed;
    updateActivities(false);
}

Xcb::StringProperty X11Window::fetchPreferredColorScheme() const
{
    return Xcb::StringProperty(m_client, atoms->kde_color_sheme);
}

QString X11Window::readPreferredColorScheme(Xcb::StringProperty &property) const
{
    return rules()->checkDecoColor(QString::fromUtf8(property));
}

QString X11Window::preferredColorScheme() const
{
    Xcb::StringProperty property = fetchPreferredColorScheme();
    return readPreferredColorScheme(property);
}

bool X11Window::isClient() const
{
    return !m_unmanaged;
}

bool X11Window::isUnmanaged() const
{
    return m_unmanaged;
}

bool X11Window::isOutline() const
{
    return m_outline;
}

WindowType X11Window::windowType() const
{
    if (m_unmanaged) {
        return WindowType(info->windowType(SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK));
    }

    WindowType wt = WindowType(info->windowType(SUPPORTED_MANAGED_WINDOW_TYPES_MASK));
    // hacks here
    if (wt == WindowType::Unknown) { // this is more or less suggested in NETWM spec
        wt = isTransient() ? WindowType::Dialog : WindowType::Normal;
    }
    return wt;
}

void X11Window::cancelFocusOutTimer()
{
    if (m_focusOutTimer) {
        m_focusOutTimer->stop();
    }
}

xcb_window_t X11Window::window() const
{
    return m_client;
}

QPointF X11Window::framePosToClientPos(const QPointF &point) const
{
    qreal x = point.x();
    qreal y = point.y();

    if (isDecorated()) {
        x += Xcb::nativeRound(borderLeft());
        y += Xcb::nativeRound(borderTop());
    } else {
        x -= m_clientFrameExtents.left();
        y -= m_clientFrameExtents.top();
    }

    return QPointF(x, y);
}

QPointF X11Window::nextFramePosToClientPos(const QPointF &point) const
{
    return framePosToClientPos(point);
}

QPointF X11Window::clientPosToFramePos(const QPointF &point) const
{
    qreal x = point.x();
    qreal y = point.y();

    if (isDecorated()) {
        x -= Xcb::nativeRound(borderLeft());
        y -= Xcb::nativeRound(borderTop());
    } else {
        x += m_clientFrameExtents.left();
        y += m_clientFrameExtents.top();
    }

    return QPointF(x, y);
}

QPointF X11Window::nextClientPosToFramePos(const QPointF &point) const
{
    return clientPosToFramePos(point);
}

QSizeF X11Window::frameSizeToClientSize(const QSizeF &size) const
{
    qreal width = size.width();
    qreal height = size.height();

    if (isDecorated()) {
        // Both frameSize and clientSize are rounded to integral XNative units
        // So their difference must also be rounded to integral XNative units
        // Otherwise we get cycles of rounding that can cause growing window sizes
        width -= Xcb::nativeRound(borderLeft()) + Xcb::nativeRound(borderRight());
        height -= Xcb::nativeRound(borderTop()) + Xcb::nativeRound(borderBottom());
    } else {
        width += m_clientFrameExtents.left() + m_clientFrameExtents.right();
        height += m_clientFrameExtents.top() + m_clientFrameExtents.bottom();
    }

    return QSizeF(width, height);
}

QSizeF X11Window::nextFrameSizeToClientSize(const QSizeF &size) const
{
    return frameSizeToClientSize(size);
}

QSizeF X11Window::clientSizeToFrameSize(const QSizeF &size) const
{
    qreal width = size.width();
    qreal height = size.height();

    if (isDecorated()) {
        // Both frameSize and clientSize are rounded to integral XNative units
        // So their difference must also be rounded to integral XNative units
        // Otherwise we get cycles of rounding that can cause growing window sizes
        width += Xcb::nativeRound(borderLeft()) + Xcb::nativeRound(borderRight());
        height += Xcb::nativeRound(borderTop()) + Xcb::nativeRound(borderBottom());
    } else {
        width -= m_clientFrameExtents.left() + m_clientFrameExtents.right();
        height -= m_clientFrameExtents.top() + m_clientFrameExtents.bottom();
    }

    return QSizeF(width, height);
}

QSizeF X11Window::nextClientSizeToFrameSize(const QSizeF &size) const
{
    return clientSizeToFrameSize(size);
}

QRectF X11Window::nextFrameRectToBufferRect(const QRectF &rect) const
{
    return nextFrameRectToClientRect(rect);
}

pid_t X11Window::pid() const
{
    return m_pid;
}

QString X11Window::windowRole() const
{
    return QString::fromLatin1(info->windowRole());
}

bool X11Window::belongsToSameApplication(const Window *other, SameApplicationChecks checks) const
{
    const X11Window *c2 = dynamic_cast<const X11Window *>(other);
    if (!c2) {
        return false;
    }
    return X11Window::belongToSameApplication(this, c2, checks);
}

QSizeF X11Window::resizeIncrements() const
{
    return Xcb::fromXNative(m_geometryHints.resizeIncrements());
}

Xcb::StringProperty X11Window::fetchApplicationMenuServiceName() const
{
    return Xcb::StringProperty(m_client, atoms->kde_net_wm_appmenu_service_name);
}

void X11Window::readApplicationMenuServiceName(Xcb::StringProperty &property)
{
    updateApplicationMenuServiceName(QString::fromUtf8(property));
}

void X11Window::checkApplicationMenuServiceName()
{
    Xcb::StringProperty property = fetchApplicationMenuServiceName();
    readApplicationMenuServiceName(property);
}

Xcb::StringProperty X11Window::fetchApplicationMenuObjectPath() const
{
    return Xcb::StringProperty(m_client, atoms->kde_net_wm_appmenu_object_path);
}

void X11Window::readApplicationMenuObjectPath(Xcb::StringProperty &property)
{
    updateApplicationMenuObjectPath(QString::fromUtf8(property));
}

void X11Window::checkApplicationMenuObjectPath()
{
    Xcb::StringProperty property = fetchApplicationMenuObjectPath();
    readApplicationMenuObjectPath(property);
}

void X11Window::ackSync()
{
    // Note that a sync request can be ack'ed after the timeout. If that happens, just re-enable
    // XSync back and do nothing more.
    m_syncRequest.pending = false;
    if (!m_syncRequest.enabled) {
        m_syncRequest.enabled = true;
        return;
    }

    m_syncRequest.acked = true;
    if (m_syncRequest.timeout) {
        m_syncRequest.timeout->stop();
    }

    setAllowCommits(true);
}

void X11Window::ackSyncTimeout()
{
    // If a sync request times out, disable XSync temporarily until the client comes back to its senses.
    m_syncRequest.enabled = false;

    finishSync();
    setAllowCommits(true);
}

void X11Window::finishSync()
{
    setReadyForPainting();

    if (m_syncRequest.interactiveResize) {
        m_syncRequest.interactiveResize = false;

        moveResize(moveResizeGeometry());
    }

    m_syncRequest.acked = false;
}

bool X11Window::belongToSameApplication(const X11Window *c1, const X11Window *c2, SameApplicationChecks checks)
{
    bool same_app = false;

    // tests that definitely mean they belong together
    if (c1 == c2) {
        same_app = true;
    } else if (c1->isTransient() && c2->hasTransient(c1, true)) {
        same_app = true; // c1 has c2 as mainwindow
    } else if (c2->isTransient() && c1->hasTransient(c2, true)) {
        same_app = true; // c2 has c1 as mainwindow
    } else if (c1->group() == c2->group()) {
        same_app = true; // same group
    } else if (c1->wmClientLeader() == c2->wmClientLeader()
               && c1->wmClientLeader() != c1->window() // if WM_CLIENT_LEADER is not set, it returns window(),
               && c2->wmClientLeader() != c2->window()) { // don't use in this test then
        same_app = true; // same client leader

        // tests that mean they most probably don't belong together
    } else if ((c1->pid() != c2->pid() && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses))
               || c1->wmClientMachine(false) != c2->wmClientMachine(false)) {
        ; // different processes
    } else if (c1->wmClientLeader() != c2->wmClientLeader()
               && c1->wmClientLeader() != c1->window() // if WM_CLIENT_LEADER is not set, it returns window(),
               && c2->wmClientLeader() != c2->window() // don't use in this test then
               && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        ; // different client leader
    } else if (c1->resourceClass() != c2->resourceClass()) {
        ; // different apps
    } else if (!sameAppWindowRoleMatch(c1, c2, checks.testFlag(SameApplicationCheck::RelaxedForActive))
               && !checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        ; // "different" apps
    } else if (c1->pid() == 0 || c2->pid() == 0) {
        ; // old apps that don't have _NET_WM_PID, consider them different
        // if they weren't found to match above
    } else {
        same_app = true; // looks like it's the same app
    }

    return same_app;
}

// Non-transient windows with window role containing '#' are always
// considered belonging to different applications (unless
// the window role is exactly the same). KMainWindow sets
// window role this way by default, and different KMainWindow
// usually "are" different application from user's point of view.
// This help with no-focus-stealing for e.g. konqy reusing.
// On the other hand, if one of the windows is active, they are
// considered belonging to the same application. This is for
// the cases when opening new mainwindow directly from the application,
// e.g. 'Open New Window' in konqy ( active_hack == true ).
bool X11Window::sameAppWindowRoleMatch(const X11Window *c1, const X11Window *c2, bool active_hack)
{
    if (c1->isTransient()) {
        while (const X11Window *t = dynamic_cast<const X11Window *>(c1->transientFor())) {
            c1 = t;
        }
        if (c1->groupTransient()) {
            return c1->group() == c2->group();
        }
#if 0
        // if a group transient is in its own group, it didn't possibly have a group,
        // and therefore should be considered belonging to the same app like
        // all other windows from the same app
        || c1->group()->leaderClient() == c1 || c2->group()->leaderClient() == c2;
#endif
    }
    if (c2->isTransient()) {
        while (const X11Window *t = dynamic_cast<const X11Window *>(c2->transientFor())) {
            c2 = t;
        }
        if (c2->groupTransient()) {
            return c1->group() == c2->group();
        }
#if 0
        || c1->group()->leaderClient() == c1 || c2->group()->leaderClient() == c2;
#endif
    }
    int pos1 = c1->windowRole().indexOf('#');
    int pos2 = c2->windowRole().indexOf('#');
    if ((pos1 >= 0 && pos2 >= 0)) {
        if (!active_hack) { // without the active hack for focus stealing prevention,
            return c1 == c2; // different mainwindows are always different apps
        }
        if (!c1->isActive() && !c2->isActive()) {
            return c1 == c2;
        } else {
            return true;
        }
    }
    return true;
}

/*

 Transiency stuff: ICCCM 4.1.2.6, NETWM 7.3

 WM_TRANSIENT_FOR is basically means "this is my mainwindow".
 For NET::Unknown windows, transient windows are considered to be NET::Dialog
 windows, for compatibility with non-NETWM clients. KWin may adjust the value
 of this property in some cases (window pointing to itself or creating a loop,
 keeping NET::Splash windows above other windows from the same app, etc.).

 X11Window::transient_for_id is the value of the WM_TRANSIENT_FOR property, after
 possibly being adjusted by KWin. X11Window::transient_for points to the Client
 this Client is transient for, or is NULL. If X11Window::transient_for_id is
 pointing to the root window, the window is considered to be transient
 for the whole window group, as suggested in NETWM 7.3.

 In the case of group transient window, X11Window::transient_for is NULL,
 and X11Window::groupTransient() returns true. Such window is treated as
 if it were transient for every window in its window group that has been
 mapped _before_ it (or, to be exact, was added to the same group before it).
 Otherwise two group transients can create loops, which can lead very very
 nasty things (bug #67914 and all its dupes).

 X11Window::original_transient_for_id is the value of the property, which
 may be different if X11Window::transient_for_id if e.g. forcing NET::Splash
 to be kept on top of its window group, or when the mainwindow is not mapped
 yet, in which case the window is temporarily made group transient,
 and when the mainwindow is mapped, transiency is re-evaluated.

 This can get a bit complicated with with e.g. two Konqueror windows created
 by the same process. They should ideally appear like two independent applications
 to the user. This should be accomplished by all windows in the same process
 having the same window group (needs to be changed in Qt at the moment), and
 using non-group transients pointing to their relevant mainwindow for toolwindows
 etc. KWin should handle both group and non-group transient dialogs well.

 In other words:
 - non-transient windows     : isTransient() == false
 - normal transients         : transientFor() != NULL
 - group transients          : groupTransient() == true

 - list of mainwindows       : mainClients()  (call once and loop over the result)
 - list of transients        : transients()
 - every window in the group : group()->members()
*/

Xcb::TransientFor X11Window::fetchTransient() const
{
    return Xcb::TransientFor(window());
}

void X11Window::readTransientProperty(Xcb::TransientFor &transientFor)
{
    xcb_window_t new_transient_for_id = XCB_WINDOW_NONE;
    if (transientFor.getTransientFor(&new_transient_for_id)) {
        m_originalTransientForId = new_transient_for_id;
        new_transient_for_id = verifyTransientFor(new_transient_for_id, true);
    } else {
        m_originalTransientForId = XCB_WINDOW_NONE;
        new_transient_for_id = verifyTransientFor(XCB_WINDOW_NONE, false);
    }
    setTransient(new_transient_for_id);
}

void X11Window::readTransient()
{
    if (isUnmanaged()) {
        return;
    }
    Xcb::TransientFor transientFor = fetchTransient();
    readTransientProperty(transientFor);
}

void X11Window::setTransient(xcb_window_t new_transient_for_id)
{
    if (new_transient_for_id != m_transientForId) {
        removeFromMainClients();
        X11Window *transient_for = nullptr;
        m_transientForId = new_transient_for_id;
        if (m_transientForId != XCB_WINDOW_NONE && !groupTransient()) {
            transient_for = workspace()->findClient(m_transientForId);
            Q_ASSERT(transient_for != nullptr); // verifyTransient() had to check this
            transient_for->addTransient(this);
        } // checkGroup() will check 'check_active_modal'
        setTransientFor(transient_for);
        checkGroup(nullptr, true); // force, because transiency has changed
        updateLayer();
        Q_EMIT transientChanged();
    }
}

void X11Window::removeFromMainClients()
{
    if (transientFor()) {
        transientFor()->removeTransient(this);
    }
    if (groupTransient()) {
        for (auto it = group()->members().constBegin(); it != group()->members().constEnd(); ++it) {
            (*it)->removeTransient(this);
        }
    }
}

// *sigh* this transiency handling is madness :(
// This one is called when destroying/releasing a window.
// It makes sure this client is removed from all grouping
// related lists.
void X11Window::cleanGrouping()
{
    // We want to break parent-child relationships, but preserve stacking
    // order constraints at the same time for window closing animations.

    if (transientFor()) {
        transientFor()->removeTransientFromList(this);
        setTransientFor(nullptr);
    }

    if (groupTransient()) {
        const auto members = group()->members();
        for (Window *member : members) {
            member->removeTransientFromList(this);
        }
    }

    const auto children = transients();
    for (Window *transient : children) {
        removeTransientFromList(transient);
        transient->setTransientFor(nullptr);
    }

    group()->removeMember(this);
    in_group = nullptr;
    m_transientForId = XCB_WINDOW_NONE;
}

// Make sure that no group transient is considered transient
// for a window that is (directly or indirectly) transient for it
// (including another group transients).
// Non-group transients not causing loops are checked in verifyTransientFor().
void X11Window::checkGroupTransients()
{
    for (auto it1 = group()->members().constBegin(); it1 != group()->members().constEnd(); ++it1) {
        if (!(*it1)->groupTransient()) { // check all group transients in the group
            continue; // TODO optimize to check only the changed ones?
        }
        for (auto it2 = group()->members().constBegin(); it2 != group()->members().constEnd(); ++it2) { // group transients can be transient only for others in the group,
            // so don't make them transient for the ones that are transient for it
            if (*it1 == *it2) {
                continue;
            }
            for (Window *cl = (*it2)->transientFor(); cl != nullptr; cl = cl->transientFor()) {
                if (cl == *it1) {
                    // don't use removeTransient(), that would modify *it2 too
                    (*it2)->removeTransientFromList(*it1);
                    continue;
                }
            }
            // if *it1 and *it2 are both group transients, and are transient for each other,
            // make only *it2 transient for *it1 (i.e. subwindow), as *it2 came later,
            // and should be therefore on top of *it1
            // TODO This could possibly be optimized, it also requires hasTransient() to check for loops.
            if ((*it2)->groupTransient() && (*it1)->hasTransient(*it2, true) && (*it2)->hasTransient(*it1, true)) {
                (*it2)->removeTransientFromList(*it1);
            }
            // if there are already windows W1 and W2, W2 being transient for W1, and group transient W3
            // is added, make it transient only for W2, not for W1, because it's already indirectly
            // transient for it - the indirect transiency actually shouldn't break anything,
            // but it can lead to exponentially expensive operations (#95231)
            // TODO this is pretty slow as well
            for (auto it3 = group()->members().constBegin(); it3 != group()->members().constEnd(); ++it3) {
                if (*it1 == *it2 || *it2 == *it3 || *it1 == *it3) {
                    continue;
                }
                if ((*it2)->hasTransient(*it1, false) && (*it3)->hasTransient(*it1, false)) {
                    if ((*it2)->hasTransient(*it3, true)) {
                        (*it2)->removeTransientFromList(*it1);
                    }
                    if ((*it3)->hasTransient(*it2, true)) {
                        (*it3)->removeTransientFromList(*it1);
                    }
                }
            }
        }
    }
}

/**
 * Check that the window is not transient for itself, and similar nonsense.
 */
xcb_window_t X11Window::verifyTransientFor(xcb_window_t new_transient_for, bool set)
{
    xcb_window_t new_property_value = new_transient_for;
    // make sure splashscreens are shown above all their app's windows, even though
    // they're in Normal layer
    if (isSplash() && new_transient_for == XCB_WINDOW_NONE) {
        new_transient_for = kwinApp()->x11RootWindow();
    }
    if (new_transient_for == XCB_WINDOW_NONE) {
        if (set) { // sometimes WM_TRANSIENT_FOR is set to None, instead of root window
            new_property_value = new_transient_for = kwinApp()->x11RootWindow();
        } else {
            return XCB_WINDOW_NONE;
        }
    }
    if (new_transient_for == window()) { // pointing to self
        // also fix the property itself
        qCWarning(KWIN_CORE) << "Client " << this << " has WM_TRANSIENT_FOR pointing to itself.";
        new_property_value = new_transient_for = kwinApp()->x11RootWindow();
    }
    //  The transient_for window may be embedded in another application,
    //  so kwin cannot see it. Try to find the managed client for the
    //  window and fix the transient_for property if possible.
    xcb_window_t before_search = new_transient_for;
    while (new_transient_for != XCB_WINDOW_NONE
           && new_transient_for != kwinApp()->x11RootWindow()
           && !workspace()->findClient(new_transient_for)) {
        Xcb::Tree tree(new_transient_for);
        if (tree.isNull()) {
            break;
        }
        new_transient_for = tree->parent;
    }
    if (X11Window *new_transient_for_client = workspace()->findClient(new_transient_for)) {
        if (new_transient_for != before_search) {
            qCDebug(KWIN_CORE) << "Client " << this << " has WM_TRANSIENT_FOR pointing to non-toplevel window "
                               << before_search << ", child of " << new_transient_for_client << ", adjusting.";
            new_property_value = new_transient_for; // also fix the property
        }
    } else {
        new_transient_for = before_search; // nice try
    }
    // loop detection
    // group transients cannot cause loops, because they're considered transient only for non-transient
    // windows in the group
    int count = 20;
    xcb_window_t loop_pos = new_transient_for;
    while (loop_pos != XCB_WINDOW_NONE && loop_pos != kwinApp()->x11RootWindow()) {
        X11Window *pos = workspace()->findClient(loop_pos);
        if (pos == nullptr) {
            break;
        }
        loop_pos = pos->m_transientForId;
        if (--count == 0 || pos == this) {
            qCWarning(KWIN_CORE) << "Client " << this << " caused WM_TRANSIENT_FOR loop.";
            new_transient_for = kwinApp()->x11RootWindow();
        }
    }
    if (new_transient_for != kwinApp()->x11RootWindow()
        && workspace()->findClient(new_transient_for) == nullptr) {
        // it's transient for a specific window, but that window is not mapped
        new_transient_for = kwinApp()->x11RootWindow();
    }
    if (new_property_value != m_originalTransientForId) {
        Xcb::setTransientFor(window(), new_property_value);
    }
    return new_transient_for;
}

void X11Window::addTransient(Window *cl)
{
    Window::addTransient(cl);
    if (workspace()->mostRecentlyActivatedWindow() == this && cl->isModal()) {
        check_active_modal = true;
    }
}

// A new window has been mapped. Check if it's not a mainwindow for this already existing window.
void X11Window::checkTransient(xcb_window_t w)
{
    if (m_originalTransientForId != w) {
        return;
    }
    w = verifyTransientFor(w, true);
    setTransient(w);
}

// returns true if cl is the transient_for window for this client,
// or recursively the transient_for window
bool X11Window::hasTransient(const Window *cl, bool indirect) const
{
    if (const X11Window *c = dynamic_cast<const X11Window *>(cl)) {
        // checkGroupTransients() uses this to break loops, so hasTransient() must detect them
        QList<const X11Window *> set;
        return hasTransientInternal(c, indirect, set);
    }
    return false;
}

bool X11Window::hasTransientInternal(const X11Window *cl, bool indirect, QList<const X11Window *> &set) const
{
    if (const X11Window *t = dynamic_cast<const X11Window *>(cl->transientFor())) {
        if (t == this) {
            return true;
        }
        if (!indirect) {
            return false;
        }
        if (set.contains(cl)) {
            return false;
        }
        set.append(cl);
        return hasTransientInternal(t, indirect, set);
    }
    if (!cl->isTransient()) {
        return false;
    }
    if (group() != cl->group()) {
        return false;
    }
    // cl is group transient, search from top
    if (transients().contains(cl)) {
        return true;
    }
    if (!indirect) {
        return false;
    }
    if (set.contains(this)) {
        return false;
    }
    set.append(this);
    for (auto it = transients().constBegin(); it != transients().constEnd(); ++it) {
        const X11Window *c = qobject_cast<const X11Window *>(*it);
        if (!c) {
            continue;
        }
        if (c->hasTransientInternal(cl, indirect, set)) {
            return true;
        }
    }
    return false;
}

QList<Window *> X11Window::mainWindows() const
{
    if (!isTransient()) {
        return QList<Window *>();
    }
    if (const Window *t = transientFor()) {
        return QList<Window *>{const_cast<Window *>(t)};
    }
    QList<Window *> result;
    Q_ASSERT(group());
    for (auto it = group()->members().constBegin(); it != group()->members().constEnd(); ++it) {
        if ((*it)->hasTransient(this, false)) {
            result.append(*it);
        }
    }
    return result;
}

// X11Window::window_group only holds the contents of the hint,
// but it should be used only to find the group, not for anything else
// Argument is only when some specific group needs to be set.
void X11Window::checkGroup(Group *set_group, bool force)
{
    Group *old_group = in_group;
    if (old_group != nullptr) {
        old_group->ref(); // turn off automatic deleting
    }
    if (set_group != nullptr) {
        if (set_group != in_group) {
            if (in_group != nullptr) {
                in_group->removeMember(this);
            }
            in_group = set_group;
            in_group->addMember(this);
        }
    } else if (info->groupLeader() != XCB_WINDOW_NONE) {
        Group *new_group = workspace()->findGroup(info->groupLeader());
        X11Window *t = qobject_cast<X11Window *>(transientFor());
        if (t != nullptr && t->group() != new_group) {
            // move the window to the right group (e.g. a dialog provided
            // by different app, but transient for this one, so make it part of that group)
            new_group = t->group();
        }
        if (new_group == nullptr) { // doesn't exist yet
            new_group = new Group(info->groupLeader());
        }
        if (new_group != in_group) {
            if (in_group != nullptr) {
                in_group->removeMember(this);
            }
            in_group = new_group;
            in_group->addMember(this);
        }
    } else {
        if (X11Window *t = qobject_cast<X11Window *>(transientFor())) {
            // doesn't have window group set, but is transient for something
            // so make it part of that group
            Group *new_group = t->group();
            if (new_group != in_group) {
                if (in_group != nullptr) {
                    in_group->removeMember(this);
                }
                in_group = t->group();
                in_group->addMember(this);
            }
        } else if (groupTransient()) {
            // group transient which actually doesn't have a group :(
            // try creating group with other windows with the same client leader
            Group *new_group = workspace()->findClientLeaderGroup(this);
            if (new_group == nullptr) {
                new_group = new Group(XCB_WINDOW_NONE);
            }
            if (new_group != in_group) {
                if (in_group != nullptr) {
                    in_group->removeMember(this);
                }
                in_group = new_group;
                in_group->addMember(this);
            }
        } else { // Not transient without a group, put it in its client leader group.
            // This might be stupid if grouping was used for e.g. taskbar grouping
            // or minimizing together the whole group, but as long as it is used
            // only for dialogs it's better to keep windows from one app in one group.
            Group *new_group = workspace()->findClientLeaderGroup(this);
            if (in_group != nullptr && in_group != new_group) {
                in_group->removeMember(this);
                in_group = nullptr;
            }
            if (new_group == nullptr) {
                new_group = new Group(XCB_WINDOW_NONE);
            }
            if (in_group != new_group) {
                in_group = new_group;
                in_group->addMember(this);
            }
        }
    }
    if (in_group != old_group || force) {
        for (auto it = transients().constBegin(); it != transients().constEnd();) {
            auto *c = *it;
            // group transients in the old group are no longer transient for it
            if (c->groupTransient() && c->group() != group()) {
                removeTransientFromList(c);
                it = transients().constBegin(); // restart, just in case something more has changed with the list
            } else {
                ++it;
            }
        }
        if (groupTransient()) {
            // no longer transient for ones in the old group
            if (old_group != nullptr) {
                for (auto it = old_group->members().constBegin(); it != old_group->members().constEnd(); ++it) {
                    (*it)->removeTransient(this);
                }
            }
            // and make transient for all in the new group
            for (auto it = group()->members().constBegin(); it != group()->members().constEnd(); ++it) {
                if (*it == this) {
                    break; // this means the window is only transient for windows mapped before it
                }
                (*it)->addTransient(this);
            }
        }
        // group transient splashscreens should be transient even for windows
        // in group mapped later
        for (auto it = group()->members().constBegin(); it != group()->members().constEnd(); ++it) {
            if (!(*it)->isSplash()) {
                continue;
            }
            if (!(*it)->groupTransient()) {
                continue;
            }
            if (*it == this || hasTransient(*it, true)) { // TODO indirect?
                continue;
            }
            addTransient(*it);
        }
    }
    if (old_group != nullptr) {
        old_group->deref(); // can be now deleted if empty
    }
    checkGroupTransients();
    checkActiveModal();
    updateLayer();
}

// used by Workspace::findClientLeaderGroup()
void X11Window::changeClientLeaderGroup(Group *gr)
{
    // transientFor() != NULL are in the group of their mainwindow, so keep them there
    if (transientFor() != nullptr) {
        return;
    }
    // also don't change the group for window which have group set
    if (info->groupLeader()) {
        return;
    }
    checkGroup(gr); // change group
}

bool X11Window::check_active_modal = false;

void X11Window::checkActiveModal()
{
    // if the active window got new modal transient, activate it.
    // cannot be done in AddTransient(), because there may temporarily
    // exist loops, breaking findModal
    X11Window *check_modal = dynamic_cast<X11Window *>(workspace()->mostRecentlyActivatedWindow());
    if (check_modal != nullptr && check_modal->check_active_modal) {
        X11Window *new_modal = dynamic_cast<X11Window *>(check_modal->findModal());
        if (new_modal != nullptr && new_modal != check_modal) {
            if (!new_modal->isManaged()) {
                return; // postpone check until end of manage()
            }
            workspace()->activateWindow(new_modal);
        }
        check_modal->check_active_modal = false;
    }
}

QSizeF X11Window::constrainClientSize(const QSizeF &size, SizeMode mode) const
{
    qreal w = size.width();
    qreal h = size.height();

    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }

    // basesize, minsize, maxsize, paspect and resizeinc have all values defined,
    // even if they're not set in flags - see getWmNormalHints()
    QSizeF min_size = minSize();
    QSizeF max_size = maxSize();
    if (isDecorated()) {
        QSizeF decominsize(0, 0);
        QSizeF border_size(borderLeft() + borderRight(), borderTop() + borderBottom());
        if (border_size.width() > decominsize.width()) { // just in case
            decominsize.setWidth(border_size.width());
        }
        if (border_size.height() > decominsize.height()) {
            decominsize.setHeight(border_size.height());
        }
        if (decominsize.width() > min_size.width()) {
            min_size.setWidth(decominsize.width());
        }
        if (decominsize.height() > min_size.height()) {
            min_size.setHeight(decominsize.height());
        }
    }
    w = std::min(max_size.width(), w);
    h = std::min(max_size.height(), h);
    w = std::max(min_size.width(), w);
    h = std::max(min_size.height(), h);

    if (!rules()->checkStrictGeometry(false)) {
        // Disobey increments and aspect by explicit rule.
        return QSizeF(w, h);
    }

    qreal width_inc = Xcb::fromXNative(m_geometryHints.resizeIncrements()).width();
    qreal height_inc = Xcb::fromXNative(m_geometryHints.resizeIncrements()).height();
    qreal basew_inc = Xcb::fromXNative(m_geometryHints.baseSize()).width();
    qreal baseh_inc = Xcb::fromXNative(m_geometryHints.baseSize()).height();
    if (!m_geometryHints.hasBaseSize()) {
        basew_inc = Xcb::fromXNative(m_geometryHints.minSize()).width();
        baseh_inc = Xcb::fromXNative(m_geometryHints.minSize()).height();
    }

    w = std::floor((w - basew_inc) / width_inc) * width_inc + basew_inc;
    h = std::floor((h - baseh_inc) / height_inc) * height_inc + baseh_inc;

    // code for aspect ratios based on code from FVWM
    /*
     * The math looks like this:
     *
     * minAspectX    dwidth     maxAspectX
     * ---------- <= ------- <= ----------
     * minAspectY    dheight    maxAspectY
     *
     * If that is multiplied out, then the width and height are
     * invalid in the following situations:
     *
     * minAspectX * dheight > minAspectY * dwidth
     * maxAspectX * dheight < maxAspectY * dwidth
     *
     */
    if (m_geometryHints.hasAspect()) {
        double min_aspect_w = m_geometryHints.minAspect().width(); // use doubles, because the values can be MAX_INT
        double min_aspect_h = m_geometryHints.minAspect().height(); // and multiplying would go wrong otherwise
        double max_aspect_w = m_geometryHints.maxAspect().width();
        double max_aspect_h = m_geometryHints.maxAspect().height();
        // According to ICCCM 4.1.2.3 PMinSize should be a fallback for PBaseSize for size increments,
        // but not for aspect ratio. Since this code comes from FVWM, handles both at the same time,
        // and I have no idea how it works, let's hope nobody relies on that.
        const QSizeF baseSize = Xcb::fromXNative(m_geometryHints.baseSize());
        w -= baseSize.width();
        h -= baseSize.height();
        qreal max_width = max_size.width() - baseSize.width();
        qreal min_width = min_size.width() - baseSize.width();
        qreal max_height = max_size.height() - baseSize.height();
        qreal min_height = min_size.height() - baseSize.height();
#define ASPECT_CHECK_GROW_W                                                           \
    if (min_aspect_w * h > min_aspect_h * w) {                                        \
        int delta = int(min_aspect_w * h / min_aspect_h - w) / width_inc * width_inc; \
        if (w + delta <= max_width)                                                   \
            w += delta;                                                               \
    }
#define ASPECT_CHECK_SHRINK_H_GROW_W                                                      \
    if (min_aspect_w * h > min_aspect_h * w) {                                            \
        int delta = int(h - w * min_aspect_h / min_aspect_w) / height_inc * height_inc;   \
        if (h - delta >= min_height)                                                      \
            h -= delta;                                                                   \
        else {                                                                            \
            int delta = int(min_aspect_w * h / min_aspect_h - w) / width_inc * width_inc; \
            if (w + delta <= max_width)                                                   \
                w += delta;                                                               \
        }                                                                                 \
    }
#define ASPECT_CHECK_GROW_H                                                             \
    if (max_aspect_w * h < max_aspect_h * w) {                                          \
        int delta = int(w * max_aspect_h / max_aspect_w - h) / height_inc * height_inc; \
        if (h + delta <= max_height)                                                    \
            h += delta;                                                                 \
    }
#define ASPECT_CHECK_SHRINK_W_GROW_H                                                        \
    if (max_aspect_w * h < max_aspect_h * w) {                                              \
        int delta = int(w - max_aspect_w * h / max_aspect_h) / width_inc * width_inc;       \
        if (w - delta >= min_width)                                                         \
            w -= delta;                                                                     \
        else {                                                                              \
            int delta = int(w * max_aspect_h / max_aspect_w - h) / height_inc * height_inc; \
            if (h + delta <= max_height)                                                    \
                h += delta;                                                                 \
        }                                                                                   \
    }
        switch (mode) {
        case SizeModeAny:
#if 0 // make SizeModeAny equal to SizeModeFixedW - prefer keeping fixed width,
      // so that changing aspect ratio to a different value and back keeps the same size (#87298)
            {
                ASPECT_CHECK_SHRINK_H_GROW_W
                ASPECT_CHECK_SHRINK_W_GROW_H
                ASPECT_CHECK_GROW_H
                ASPECT_CHECK_GROW_W
                break;
            }
#endif
        case SizeModeFixedW: {
            // the checks are order so that attempts to modify height are first
            ASPECT_CHECK_GROW_H
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_GROW_W
            break;
        }
        case SizeModeFixedH: {
            ASPECT_CHECK_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_GROW_H
            break;
        }
        case SizeModeMax: {
            // first checks that try to shrink
            ASPECT_CHECK_SHRINK_H_GROW_W
            ASPECT_CHECK_SHRINK_W_GROW_H
            ASPECT_CHECK_GROW_W
            ASPECT_CHECK_GROW_H
            break;
        }
        }
#undef ASPECT_CHECK_SHRINK_H_GROW_W
#undef ASPECT_CHECK_SHRINK_W_GROW_H
#undef ASPECT_CHECK_GROW_W
#undef ASPECT_CHECK_GROW_H
        w += baseSize.width();
        h += baseSize.height();
    }

    return QSizeF(w, h);
}

xcb_res_query_client_ids_cookie_t X11Window::fetchPid() const
{
    xcb_res_query_client_ids_cookie_t cookie{
        .sequence = 0,
    };

    if (Xcb::Extensions::self()->hasRes()) {
        const xcb_res_client_id_spec_t spec{
            .client = m_client,
            .mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID,
        };
        cookie = xcb_res_query_client_ids(kwinApp()->x11Connection(), 1, &spec);
    }

    return cookie;
}

void X11Window::readPid(xcb_res_query_client_ids_cookie_t cookie)
{
    if (!cookie.sequence) {
        return;
    }

    if (auto clientIds = xcb_res_query_client_ids_reply(kwinApp()->x11Connection(), cookie, nullptr)) {
        xcb_res_client_id_value_iterator_t it = xcb_res_query_client_ids_ids_iterator(clientIds);
        while (it.rem > 0) {
            if ((it.data->spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) && xcb_res_client_id_value_value_length(it.data) > 0) {
                m_pid = *xcb_res_client_id_value_value(it.data);
                break;
            }
        }
        free(clientIds);
    }
}

void X11Window::getResourceClass()
{
    setResourceClass(QString::fromLatin1(info->windowClassName()), QString::fromLatin1(info->windowClassClass()));
}

/**
 * Gets the client's normal WM hints and reconfigures itself respectively.
 */
void X11Window::getWmNormalHints()
{
    if (isUnmanaged()) {
        return;
    }
    const bool hadFixedAspect = m_geometryHints.hasAspect();
    // roundtrip to X server
    m_geometryHints.fetch();
    m_geometryHints.read();

    if (!hadFixedAspect && m_geometryHints.hasAspect()) {
        // align to eventual new constraints
        maximize(max_mode);
    }
    if (isManaged()) {
        // update to match restrictions
        QSizeF new_size = clientSizeToFrameSize(constrainClientSize(clientSize()));
        if (new_size != size() && !isFullScreen()) {
            QRectF origClientGeometry = m_clientGeometry;
            moveResize(resizeWithChecks(moveResizeGeometry(), new_size));
            if ((!isSpecialWindow() || isToolbar()) && !isFullScreen()) {
                // try to keep the window in its xinerama screen if possible,
                // if that fails at least keep it visible somewhere
                QRectF area = workspace()->clientArea(MovementArea, this, moveResizeOutput());
                if (area.contains(origClientGeometry)) {
                    moveResize(keepInArea(moveResizeGeometry(), area));
                }
                area = workspace()->clientArea(WorkArea, this, moveResizeOutput());
                if (area.contains(origClientGeometry)) {
                    moveResize(keepInArea(moveResizeGeometry(), area));
                }
            }
        }
    }
    updateAllowedActions(); // affects isResizeable()
}

QSizeF X11Window::minSize() const
{
    return rules()->checkMinSize(Xcb::fromXNative(m_geometryHints.minSize()));
}

QSizeF X11Window::maxSize() const
{
    return rules()->checkMaxSize(Xcb::fromXNative(m_geometryHints.maxSize()));
}

QSizeF X11Window::basicUnit() const
{
    return QSize(1, 1);
}

/**
 * Auxiliary function to inform the client about the current window
 * configuration.
 */
void X11Window::sendSyntheticConfigureNotify()
{
    // Every X11 event is 32 bytes (see man xcb_send_event), so XCB will copy
    // 32 unconditionally. Use a union to ensure we don't disclose stack memory.
    union {
        xcb_configure_notify_event_t event;
        char buffer[32];
    } u{};
    static_assert(sizeof(u.event) < 32, "wouldn't need the union otherwise");
    xcb_configure_notify_event_t &c = u.event;
    u.event.response_type = XCB_CONFIGURE_NOTIFY;
    u.event.event = window();
    u.event.window = window();
    u.event.x = m_client.x();
    u.event.y = m_client.y();
    u.event.width = m_client.width();
    u.event.height = m_client.height();
    u.event.border_width = 0;
    u.event.above_sibling = XCB_WINDOW_NONE;
    u.event.override_redirect = 0;
    xcb_send_event(kwinApp()->x11Connection(), true, c.event, XCB_EVENT_MASK_STRUCTURE_NOTIFY, reinterpret_cast<const char *>(&u));
    xcb_flush(kwinApp()->x11Connection());
}

void X11Window::handleXwaylandScaleChanged()
{
    // while KWin implicitly considers the window already resized when the scale changes,
    // this is needed to make Xwayland actually resize it as well
    resize(moveResizeGeometry().size());
    getWmOpaqueRegion();
}

void X11Window::handleCommitted()
{
    if (surface()->isMapped()) {
        if (m_syncRequest.acked) {
            finishSync();
        }

        if (!m_syncRequest.enabled) {
            setReadyForPainting();
        }
    }
}

void X11Window::setAllowCommits(bool allow)
{
    static bool disabled = qEnvironmentVariableIntValue("KWIN_NO_XWAYLAND_ALLOW_COMMITS") == 1;
    if (disabled) {
        return;
    }

    uint32_t value = allow;
    xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, window(),
                        atoms->xwayland_allow_commits, XCB_ATOM_CARDINAL, 32, 1, &value);
}

QPointF X11Window::gravityAdjustment(xcb_gravity_t gravity) const
{
    qreal dx = 0;
    qreal dy = 0;

    // dx, dy specify how the client window moves to make space for the frame.
    // In general we have to compute the reference point and from that figure
    // out how much we need to shift the client, however given that we ignore
    // the border width attribute and the extents of the server-side decoration
    // are known in advance, we can simplify the math quite a bit and express
    // the required window gravity adjustment in terms of border sizes.
    switch (gravity) {
    case XCB_GRAVITY_NORTH_WEST: // move down right
    default:
        dx = Xcb::nativeRound(borderLeft());
        dy = Xcb::nativeRound(borderTop());
        break;
    case XCB_GRAVITY_NORTH: // move right
        dx = 0;
        dy = Xcb::nativeRound(borderTop());
        break;
    case XCB_GRAVITY_NORTH_EAST: // move down left
        dx = -Xcb::nativeRound(borderRight());
        dy = Xcb::nativeRound(borderTop());
        break;
    case XCB_GRAVITY_WEST: // move right
        dx = Xcb::nativeRound(borderLeft());
        dy = 0;
        break;
    case XCB_GRAVITY_CENTER:
        dx = Xcb::fromXNative((int(Xcb::toXNative(borderLeft())) - int(Xcb::toXNative(borderRight()))) / 2);
        dy = Xcb::fromXNative((int(Xcb::toXNative(borderTop())) - int(Xcb::toXNative(borderBottom()))) / 2);
        break;
    case XCB_GRAVITY_STATIC: // don't move
        dx = 0;
        dy = 0;
        break;
    case XCB_GRAVITY_EAST: // move left
        dx = -Xcb::nativeRound(borderRight());
        dy = 0;
        break;
    case XCB_GRAVITY_SOUTH_WEST: // move up right
        dx = Xcb::nativeRound(borderLeft());
        dy = -Xcb::nativeRound(borderBottom());
        break;
    case XCB_GRAVITY_SOUTH: // move up
        dx = 0;
        dy = -Xcb::nativeRound(borderBottom());
        break;
    case XCB_GRAVITY_SOUTH_EAST: // move up left
        dx = -Xcb::nativeRound(borderRight());
        dy = -Xcb::nativeRound(borderBottom());
        break;
    }

    return QPointF(dx, dy);
}

const QPointF X11Window::calculateGravitation(bool invert) const
{
    const QPointF adjustment = gravityAdjustment(m_geometryHints.windowGravity());

    // translate from client movement to frame movement
    const qreal dx = adjustment.x() - Xcb::nativeRound(borderLeft());
    const qreal dy = adjustment.y() - Xcb::nativeRound(borderTop());

    if (!invert) {
        return QPointF(x() + dx, y() + dy);
    } else {
        return QPointF(x() - dx, y() - dy);
    }
}

// coordinates are in kwin logical
void X11Window::configureRequest(int value_mask, qreal rx, qreal ry, qreal rw, qreal rh, int gravity, bool from_tool)
{
    const int configurePositionMask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    const int configureSizeMask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const int configureGeometryMask = configurePositionMask | configureSizeMask;

    // "maximized" is a user setting -> we do not allow the client to resize itself
    // away from this & against the users explicit wish
    qCDebug(KWIN_CORE) << this << bool(value_mask & configureGeometryMask) << bool(requestedMaximizeMode() & MaximizeVertical) << bool(requestedMaximizeMode() & MaximizeHorizontal);

    // we want to (partially) ignore the request when the window is somehow maximized or quicktiled
    bool ignore = !app_noborder && (requestedQuickTileMode() != QuickTileMode(QuickTileFlag::None) || requestedMaximizeMode() != MaximizeRestore);
    // however, the user shall be able to force obedience despite and also disobedience in general
    ignore = rules()->checkIgnoreGeometry(ignore);
    if (!ignore) { // either we're not max'd / q'tiled or the user allowed the client to break that - so break it.
        exitQuickTileMode();
    } else if (!app_noborder && requestedQuickTileMode() == QuickTileMode(QuickTileFlag::None) && (requestedMaximizeMode() == MaximizeVertical || requestedMaximizeMode() == MaximizeHorizontal)) {
        // ignoring can be, because either we do, or the user does explicitly not want it.
        // for partially maximized windows we want to allow configures in the other dimension.
        // so we've to ask the user again - to know whether we just ignored for the partial maximization.
        // the problem here is, that the user can explicitly permit configure requests - even for maximized windows!
        // we cannot distinguish that from passing "false" for partially maximized windows.
        ignore = rules()->checkIgnoreGeometry(false);
        if (!ignore) { // the user is not interested, so we fix up dimensions
            if (requestedMaximizeMode() == MaximizeVertical) {
                value_mask &= ~(XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT);
            }
            if (requestedMaximizeMode() == MaximizeHorizontal) {
                value_mask &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH);
            }
            if (!(value_mask & configureGeometryMask)) {
                ignore = true; // the modification turned the request void
            }
        }
    }

    if (ignore) {
        qCDebug(KWIN_CORE) << "DENIED";
        return; // nothing to (left) to do for use - bugs #158974, #252314, #321491
    }

    qCDebug(KWIN_CORE) << "PERMITTED" << this << bool(value_mask & configureGeometryMask);

    if (gravity == 0) { // default (nonsense) value for the argument
        gravity = m_geometryHints.windowGravity();
    }
    if (value_mask & configurePositionMask) {
        QPointF new_pos = framePosToClientPos(pos());
        new_pos -= gravityAdjustment(xcb_gravity_t(gravity));
        if (value_mask & XCB_CONFIG_WINDOW_X) {
            new_pos.setX(rx);
        }
        if (value_mask & XCB_CONFIG_WINDOW_Y) {
            new_pos.setY(ry);
        }
        new_pos += gravityAdjustment(xcb_gravity_t(gravity));
        new_pos = clientPosToFramePos(new_pos);

        qreal nw = clientSize().width();
        qreal nh = clientSize().height();
        if (value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            nw = rw;
        }
        if (value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            nh = rh;
        }
        const QSizeF requestedClientSize = constrainClientSize(QSizeF(nw, nh));
        QSizeF requestedFrameSize = clientSizeToFrameSize(requestedClientSize);
        requestedFrameSize = rules()->checkSize(requestedFrameSize);
        new_pos = rules()->checkPosition(new_pos);

        Output *newOutput = workspace()->outputAt(QRectF(new_pos, requestedFrameSize).center());
        if (newOutput != rules()->checkOutput(newOutput)) {
            return; // not allowed by rule
        }

        QRectF geometry = QRectF(new_pos, requestedFrameSize);
        const QRectF area = workspace()->clientArea(WorkArea, this, geometry.center());
        if (!from_tool && (!isSpecialWindow() || isToolbar()) && !isFullScreen() && area.contains(clientGeometry())) {
            geometry = keepInArea(geometry, area);
        }

        moveResize(geometry);
    }

    if (value_mask & configureSizeMask && !(value_mask & configurePositionMask)) { // pure resize
        qreal nw = clientSize().width();
        qreal nh = clientSize().height();
        if (value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            nw = rw;
        }
        if (value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            nh = rh;
        }

        const QSizeF requestedClientSize = constrainClientSize(QSizeF(nw, nh));
        const QSizeF requestedFrameSize = rules()->checkSize(clientSizeToFrameSize(requestedClientSize));

        if (requestedFrameSize != size()) { // don't restore if some app sets its own size again
            QRectF geometry = resizeWithChecks(moveResizeGeometry(), requestedFrameSize, xcb_gravity_t(gravity));
            if (!from_tool && (!isSpecialWindow() || isToolbar()) && !isFullScreen()) {
                // try to keep the window in its xinerama screen if possible,
                // if that fails at least keep it visible somewhere
                QRectF area = workspace()->clientArea(MovementArea, this, geometry.center());
                if (area.contains(clientGeometry())) {
                    geometry = keepInArea(geometry, area);
                }
                area = workspace()->clientArea(WorkArea, this, geometry.center());
                if (area.contains(clientGeometry())) {
                    geometry = keepInArea(geometry, area);
                }
            }
            moveResize(geometry);
        }
    }
    // No need to send synthetic configure notify event here, either it's sent together
    // with geometry change, or there's no need to send it.
    // Handling of the real ConfigureRequest event forces sending it, as there it's necessary.
}

QRectF X11Window::resizeWithChecks(const QRectF &geometry, qreal w, qreal h, xcb_gravity_t gravity) const
{
    qreal newx = geometry.x();
    qreal newy = geometry.y();
    QRectF area = workspace()->clientArea(WorkArea, this, geometry.center());
    // don't allow growing larger than workarea
    if (w > area.width()) {
        w = area.width();
    }
    if (h > area.height()) {
        h = area.height();
    }
    QSizeF tmp = constrainFrameSize(QSizeF(w, h)); // checks size constraints, including min/max size
    w = tmp.width();
    h = tmp.height();
    if (gravity == 0) {
        gravity = m_geometryHints.windowGravity();
    }
    switch (gravity) {
    case XCB_GRAVITY_NORTH_WEST: // top left corner doesn't move
    default:
        break;
    case XCB_GRAVITY_NORTH: // middle of top border doesn't move
        newx = (newx + geometry.width() / 2) - (w / 2);
        break;
    case XCB_GRAVITY_NORTH_EAST: // top right corner doesn't move
        newx = newx + geometry.width() - w;
        break;
    case XCB_GRAVITY_WEST: // middle of left border doesn't move
        newy = (newy + geometry.height() / 2) - (h / 2);
        break;
    case XCB_GRAVITY_CENTER: // middle point doesn't move
        newx = (newx + geometry.width() / 2) - (w / 2);
        newy = (newy + geometry.height() / 2) - (h / 2);
        break;
    case XCB_GRAVITY_STATIC: // top left corner of _client_ window doesn't move
        // since decoration doesn't change, equal to NorthWestGravity
        break;
    case XCB_GRAVITY_EAST: // // middle of right border doesn't move
        newx = newx + geometry.width() - w;
        newy = (newy + geometry.height() / 2) - (h / 2);
        break;
    case XCB_GRAVITY_SOUTH_WEST: // bottom left corner doesn't move
        newy = newy + geometry.height() - h;
        break;
    case XCB_GRAVITY_SOUTH: // middle of bottom border doesn't move
        newx = (newx + geometry.width() / 2) - (w / 2);
        newy = newy + geometry.height() - h;
        break;
    case XCB_GRAVITY_SOUTH_EAST: // bottom right corner doesn't move
        newx = newx + geometry.width() - w;
        newy = newy + geometry.height() - h;
        break;
    }
    return QRectF{newx, newy, w, h};
}

// _NET_MOVERESIZE_WINDOW
// note coordinates are kwin logical
void X11Window::NETMoveResizeWindow(int flags, qreal x, qreal y, qreal width, qreal height)
{
    int gravity = flags & 0xff;
    int value_mask = 0;
    if (flags & (1 << 8)) {
        value_mask |= XCB_CONFIG_WINDOW_X;
    }
    if (flags & (1 << 9)) {
        value_mask |= XCB_CONFIG_WINDOW_Y;
    }
    if (flags & (1 << 10)) {
        value_mask |= XCB_CONFIG_WINDOW_WIDTH;
    }
    if (flags & (1 << 11)) {
        value_mask |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    configureRequest(value_mask, x, y, width, height, gravity, true);
}

// _GTK_SHOW_WINDOW_MENU
void X11Window::GTKShowWindowMenu(qreal x_root, qreal y_root)
{
    QPoint globalPos(x_root, y_root);
    workspace()->showWindowMenu(QRect(globalPos, globalPos), this);
}

bool X11Window::isMovable() const
{
    if (isUnmanaged()) {
        return false;
    }
    if (!hasNETSupport() && !m_motif.move()) {
        return false;
    }
    if (isFullScreen()) {
        return false;
    }
    if (isSpecialWindow() && !isSplash() && !isToolbar()) { // allow moving of splashscreens :)
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) { // forced position
        return false;
    }
    if (!options->interactiveWindowMoveEnabled()) {
        return false;
    }
    return true;
}

bool X11Window::isMovableAcrossScreens() const
{
    if (isUnmanaged()) {
        return false;
    }
    if (!hasNETSupport() && !m_motif.move()) {
        return false;
    }
    if (isSpecialWindow() && !isSplash() && !isToolbar()) { // allow moving of splashscreens :)
        return false;
    }
    if (rules()->checkPosition(invalidPoint) != invalidPoint) { // forced position
        return false;
    }
    if (!options->interactiveWindowMoveEnabled()) {
        return false;
    }
    return true;
}

bool X11Window::isResizable() const
{
    if (isUnmanaged()) {
        return false;
    }
    if (!hasNETSupport() && !m_motif.resize()) {
        return false;
    }
    if (isFullScreen()) {
        return false;
    }
    if (isSpecialWindow() && !isAppletPopup()) {
        return false;
    }
    if (rules()->checkSize(QSize()).isValid()) { // forced size
        return false;
    }
    const Gravity gravity = interactiveMoveResizeGravity();
    if ((gravity == Gravity::Top || gravity == Gravity::TopLeft || gravity == Gravity::TopRight || gravity == Gravity::Left || gravity == Gravity::BottomLeft) && rules()->checkPosition(invalidPoint) != invalidPoint) {
        return false;
    }

    QSizeF min = minSize();
    QSizeF max = maxSize();
    return min.width() < max.width() || min.height() < max.height();
}

bool X11Window::isMaximizable() const
{
    if (isUnmanaged()) {
        return false;
    }
    if (!isResizable() || isToolbar()) { // SELI isToolbar() ?
        return false;
    }
    if (isAppletPopup()) {
        return false;
    }
    if (rules()->checkMaximize(MaximizeRestore) == MaximizeRestore && rules()->checkMaximize(MaximizeFull) != MaximizeRestore) {
        return true;
    }
    return false;
}

void X11Window::blockGeometryUpdates(bool block)
{
    if (block) {
        ++m_blockGeometryUpdates;
    } else {
        if (--m_blockGeometryUpdates == 0) {
            configure(Xcb::toXNative(m_bufferGeometry));
        }
    }
}

/**
 * Reimplemented to inform the client about the new window position.
 */
void X11Window::moveResizeInternal(const QRectF &rect, MoveResizeMode mode)
{
    if (isUnmanaged()) {
        qCWarning(KWIN_CORE) << "Cannot move or resize unmanaged window" << this;
        return;
    }

    const QRectF frameGeometry = Xcb::fromXNative(Xcb::toXNative(rect));
    const QRectF clientGeometry = nextFrameRectToClientRect(frameGeometry);
    const QRectF bufferGeometry = nextFrameRectToBufferRect(frameGeometry);
    const qreal bufferScale = kwinApp()->xwaylandScale();

    if (m_bufferGeometry == bufferGeometry && m_clientGeometry == clientGeometry && m_frameGeometry == frameGeometry && m_bufferScale == bufferScale) {
        return;
    }

    Q_EMIT frameGeometryAboutToChange();

    const QRectF oldBufferGeometry = m_bufferGeometry;
    const QRectF oldFrameGeometry = m_frameGeometry;
    const QRectF oldClientGeometry = m_clientGeometry;
    const Output *oldOutput = m_output;

    m_frameGeometry = frameGeometry;
    m_clientGeometry = clientGeometry;
    m_bufferGeometry = bufferGeometry;
    m_bufferScale = bufferScale;
    m_output = workspace()->outputAt(frameGeometry.center());

    if (!areGeometryUpdatesBlocked()) {
        configure(Xcb::toXNative(m_bufferGeometry));
    }

    updateWindowRules(Rules::Position | Rules::Size);

    if (isActive()) {
        workspace()->setActiveOutput(output());
    }
    workspace()->updateStackingOrder();

    if (oldBufferGeometry != m_bufferGeometry) {
        Q_EMIT bufferGeometryChanged(oldBufferGeometry);
    }
    if (oldClientGeometry != m_clientGeometry) {
        Q_EMIT clientGeometryChanged(oldClientGeometry);
    }
    if (oldFrameGeometry != m_frameGeometry) {
        Q_EMIT frameGeometryChanged(oldFrameGeometry);
    }
    if (oldOutput != m_output) {
        Q_EMIT outputChanged();
    }
    updateShapeRegion();
}

void X11Window::configure(const QRect &nativeGeometry)
{
    // handle xrandr emulation: If a fullscreen client requests mode changes,
    // - Xwayland will set _XWAYLAND_RANDR_EMU_MONITOR_RECTS on all windows of the client
    // - we need to configure the window to match the emulated size
    // - Xwayland will set up the viewport to present the window properly on the Wayland side
    QRect effectiveGeometry = nativeGeometry;
    if (m_fullscreenMode == X11Window::FullScreenMode::FullScreenNormal) {
        Xcb::Property property(0, m_client, atoms->xwayland_xrandr_emulation, XCB_ATOM_CARDINAL, 0, workspace()->outputs().size() * 4);
        const uint32_t *values = property.value<uint32_t *>();
        if (values) {
            const QPoint xwaylandOutputPos = Xcb::toXNative(m_moveResizeOutput->geometryF().topLeft());
            // "length" is in number of 32 bit words
            const int numOfOutputs = property.data()->value_len / 4;
            for (int i = 0; i < numOfOutputs; i++) {
                // the format of the property is: x,y of the screen + emulated width,height
                const uint32_t x = values[i * 4];
                const uint32_t y = values[i * 4 + 1];
                if (xwaylandOutputPos == QPoint(x, y)) {
                    effectiveGeometry.setWidth(values[i * 4 + 2]);
                    effectiveGeometry.setHeight(values[i * 4 + 3]);
                    break;
                }
            }
        }
    }
    if (m_client.size() != effectiveGeometry.size()) {
        m_client.setGeometry(effectiveGeometry);
    } else if (m_client.position() != effectiveGeometry.topLeft()) {
        m_client.move(effectiveGeometry.topLeft());

        // A synthetic configure notify event has to be sent if the client window is not
        // resized to let the client know about the new position. See ICCCM 4.1.5.
        sendSyntheticConfigureNotify();
    }
}

static bool changeMaximizeRecursion = false;
void X11Window::maximize(MaximizeMode mode, const QRectF &restore)
{
    if (isUnmanaged()) {
        qCWarning(KWIN_CORE) << "Cannot change maximized state of unmanaged window" << this;
        return;
    }

    if (changeMaximizeRecursion) {
        return;
    }

    if (!isMaximizable() && mode != MaximizeRestore) {
        return;
    }

    QRectF clientArea;
    if (isElectricBorderMaximizing()) {
        clientArea = workspace()->clientArea(MaximizeArea, this, interactiveMoveResizeAnchor());
    } else {
        clientArea = workspace()->clientArea(MaximizeArea, this, moveResizeOutput());
    }

    MaximizeMode old_mode = max_mode;

    mode = rules()->checkMaximize(mode);
    if (max_mode == mode) {
        return;
    }

    blockGeometryUpdates(true);

    // maximing one way and unmaximizing the other way shouldn't happen,
    // so restore first and then maximize the other way
    if ((old_mode == MaximizeVertical && mode == MaximizeHorizontal)
        || (old_mode == MaximizeHorizontal && mode == MaximizeVertical)) {
        maximize(MaximizeRestore); // restore
    }

    Q_EMIT maximizedAboutToChange(mode);
    max_mode = mode;

    // save sizes for restoring, if maximalizing
    QSizeF sz = size();

    if (!restore.isNull()) {
        setGeometryRestore(restore);
    } else {
        if (requestedQuickTileMode() == QuickTileMode(QuickTileFlag::None)) {
            QRectF savedGeometry = geometryRestore();
            if (!(old_mode & MaximizeVertical)) {
                savedGeometry.setTop(y());
                savedGeometry.setHeight(sz.height());
            }
            if (!(old_mode & MaximizeHorizontal)) {
                savedGeometry.setLeft(x());
                savedGeometry.setWidth(sz.width());
            }
            setGeometryRestore(savedGeometry);
        }
    }

    // call into decoration update borders
    if (isDecorated() && decoration()->window() && !(options->borderlessMaximizedWindows() && max_mode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->window();
        if ((max_mode & MaximizeVertical) != (old_mode & MaximizeVertical)) {
            Q_EMIT c->maximizedVerticallyChanged(max_mode & MaximizeVertical);
        }
        if ((max_mode & MaximizeHorizontal) != (old_mode & MaximizeHorizontal)) {
            Q_EMIT c->maximizedHorizontallyChanged(max_mode & MaximizeHorizontal);
        }
        if ((max_mode == MaximizeFull) != (old_mode == MaximizeFull)) {
            Q_EMIT c->maximizedChanged(max_mode == MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        // triggers a maximize change.
        // The next setNoBorder iteration will exit since there's no change but the first recursion pullutes the restore geometry
        changeMaximizeRecursion = true;
        setNoBorder(rules()->checkNoBorder(app_noborder || (m_motif.hasDecorationsFlag() && m_motif.noDecorations()) || max_mode == MaximizeFull));
        changeMaximizeRecursion = false;
    }

    switch (max_mode) {

    case MaximizeVertical: {
        if (old_mode & MaximizeHorizontal) { // actually restoring from MaximizeFull
            if (geometryRestore().width() == 0) {
                // needs placement
                const QSizeF constraintedSize = constrainFrameSize(QSizeF(width() * 2 / 3, clientArea.height()), SizeModeFixedH);
                resize(QSizeF(constraintedSize.width(), clientArea.height()));
                if (const auto placement = workspace()->placement()->placeSmart(this, clientArea)) {
                    place(*placement);
                }
            } else {
                moveResize(QRectF(QPointF(geometryRestore().x(), clientArea.top()),
                                  QSize(geometryRestore().width(), clientArea.height())));
            }
        } else {
            moveResize(QRectF(x(), clientArea.top(), width(), clientArea.height()));
        }
        exitQuickTileMode();
        info->setState(NET::MaxVert, NET::Max);
        break;
    }

    case MaximizeHorizontal: {
        if (old_mode & MaximizeVertical) { // actually restoring from MaximizeFull
            if (geometryRestore().height() == 0) {
                // needs placement
                const QSizeF constraintedSize = constrainFrameSize(QSizeF(clientArea.width(), height() * 2 / 3), SizeModeFixedW);
                resize(QSizeF(clientArea.width(), constraintedSize.height()));
                if (const auto placement = workspace()->placement()->placeSmart(this, clientArea)) {
                    place(*placement);
                }
            } else {
                moveResize(QRectF(QPoint(clientArea.left(), geometryRestore().y()),
                                  QSize(clientArea.width(), geometryRestore().height())));
            }
        } else {
            moveResize(QRectF(clientArea.left(), y(), clientArea.width(), height()));
        }
        exitQuickTileMode();
        info->setState(NET::MaxHoriz, NET::Max);
        break;
    }

    case MaximizeRestore: {
        QRectF restore = moveResizeGeometry();
        // when only partially maximized, geom_restore may not have the other dimension remembered
        if (old_mode & MaximizeVertical) {
            restore.setTop(geometryRestore().top());
            restore.setBottom(geometryRestore().bottom());
        }
        if (old_mode & MaximizeHorizontal) {
            restore.setLeft(geometryRestore().left());
            restore.setRight(geometryRestore().right());
        }
        if (!restore.isValid()) {
            QSize s = QSize(clientArea.width() * 2 / 3, clientArea.height() * 2 / 3);
            if (geometryRestore().width() > 0) {
                s.setWidth(geometryRestore().width());
            }
            if (geometryRestore().height() > 0) {
                s.setHeight(geometryRestore().height());
            }
            resize(constrainFrameSize(s));
            if (const auto placement = workspace()->placement()->placeSmart(this, clientArea)) {
                place(*placement);
            }
            restore = moveResizeGeometry();
            if (geometryRestore().width() > 0) {
                restore.moveLeft(geometryRestore().x());
            }
            if (geometryRestore().height() > 0) {
                restore.moveTop(geometryRestore().y());
            }
            setGeometryRestore(restore); // relevant for mouse pos calculation, bug #298646
        }

        restore.setSize(constrainFrameSize(restore.size(), SizeModeAny));

        if (isFullScreen()) {
            if (info->fullscreenMonitors().isSet()) {
                restore = fullscreenMonitorsArea(info->fullscreenMonitors());
            } else {
                restore = workspace()->clientArea(FullScreenArea, this, moveResizeOutput());
            }
        } else if (isInteractiveMove()) {
            const QPointF anchor = interactiveMoveResizeAnchor();
            const QPointF offset = interactiveMoveOffset();
            restore.moveTopLeft(QPointF(anchor.x() - offset.x() * restore.width(),
                                        anchor.y() - offset.y() * restore.height()));
        }

        moveResize(restore);
        info->setState(NET::States(), NET::Max);
        break;
    }

    case MaximizeFull: {
        moveResize(clientArea);
        exitQuickTileMode();
        info->setState(NET::Max, NET::Max);
        break;
    }
    default:
        break;
    }

    markAsPlaced();

    blockGeometryUpdates(false);
    updateAllowedActions();
    updateWindowRules(Rules::MaximizeVert | Rules::MaximizeHoriz | Rules::Position | Rules::Size);

    if (max_mode != old_mode) {
        Q_EMIT maximizedChanged();
    }
}

void X11Window::setFullScreen(bool set)
{
    set = rules()->checkFullScreen(set);

    const bool wasFullscreen = isFullScreen();
    if (wasFullscreen == set) {
        return;
    }
    if (!isFullScreenable()) {
        return;
    }

    if (wasFullscreen) {
        workspace()->updateFocusMousePosition(Cursors::self()->mouse()->pos()); // may cause leave event
    } else {
        setFullscreenGeometryRestore(moveResizeGeometry());
    }

    if (set) {
        m_fullscreenMode = FullScreenNormal;
        workspace()->raiseWindow(this);
    } else {
        m_fullscreenMode = FullScreenNone;
    }

    StackingUpdatesBlocker blocker1(workspace());
    X11GeometryUpdatesBlocker blocker2(this);

    // active fullscreens get different layer
    updateLayer();

    info->setState(isFullScreen() ? NET::FullScreen : NET::States(), NET::FullScreen);
    updateDecoration(false, false);

    if (set) {
        if (info->fullscreenMonitors().isSet()) {
            moveResize(fullscreenMonitorsArea(info->fullscreenMonitors()));
        } else {
            moveResize(workspace()->clientArea(FullScreenArea, this, moveResizeOutput()));
        }
    } else {
        Q_ASSERT(!fullscreenGeometryRestore().isNull());
        moveResize(QRectF(fullscreenGeometryRestore().topLeft(), constrainFrameSize(fullscreenGeometryRestore().size())));
    }

    markAsPlaced();

    updateWindowRules(Rules::Fullscreen | Rules::Position | Rules::Size);
    updateAllowedActions(false);
    Q_EMIT fullScreenChanged();
}

void X11Window::updateFullscreenMonitors(NETFullscreenMonitors topology)
{
    const int outputCount = workspace()->outputs().count();

    //    qDebug() << "incoming request with top: " << topology.top << " bottom: " << topology.bottom
    //                   << " left: " << topology.left << " right: " << topology.right
    //                   << ", we have: " << nscreens << " screens.";

    if (topology.top >= outputCount || topology.bottom >= outputCount || topology.left >= outputCount || topology.right >= outputCount) {
        qCWarning(KWIN_CORE) << "fullscreenMonitors update failed. request higher than number of screens.";
        return;
    }

    info->setFullscreenMonitors(topology);
    if (isFullScreen()) {
        moveResize(fullscreenMonitorsArea(topology));
    }
}

/**
 * Calculates the bounding rectangle defined by the 4 monitor indices indicating the
 * top, bottom, left, and right edges of the window when the fullscreen state is enabled.
 */
QRect X11Window::fullscreenMonitorsArea(NETFullscreenMonitors requestedTopology) const
{
    QRect total;

    if (auto output = workspace()->xineramaIndexToOutput(requestedTopology.top)) {
        total = total.united(output->geometry());
    }
    if (auto output = workspace()->xineramaIndexToOutput(requestedTopology.bottom)) {
        total = total.united(output->geometry());
    }
    if (auto output = workspace()->xineramaIndexToOutput(requestedTopology.left)) {
        total = total.united(output->geometry());
    }
    if (auto output = workspace()->xineramaIndexToOutput(requestedTopology.right)) {
        total = total.united(output->geometry());
    }

    return total;
}

bool X11Window::isWaitingForInteractiveResizeSync() const
{
    return m_syncRequest.enabled && (m_syncRequest.pending || m_syncRequest.acked);
}

void X11Window::doInteractiveResizeSync(const QRectF &rect)
{
    const QRectF moveResizeFrameGeometry = Xcb::fromXNative(Xcb::toXNative(rect));
    const QRectF moveResizeBufferGeometry = nextFrameRectToBufferRect(moveResizeFrameGeometry);
    const QRect nativeBufferGeometry = Xcb::toXNative(moveResizeBufferGeometry);

    if (m_client.geometry() == nativeBufferGeometry) {
        return;
    }

    if (!m_syncRequest.enabled) {
        moveResize(rect);
    } else {
        setMoveResizeGeometry(moveResizeFrameGeometry);
        sendSyncRequest();
        configure(nativeBufferGeometry);
    }
}

void X11Window::applyWindowRules()
{
    Window::applyWindowRules();
    updateAllowedActions();
}

bool X11Window::supportsWindowRules() const
{
    return !isUnmanaged();
}

void X11Window::updateWindowRules(Rules::Types selection)
{
    if (!isManaged()) { // not fully setup yet
        return;
    }
    Window::updateWindowRules(selection);
}

void X11Window::associate(XwaylandSurfaceV1Interface *shellSurface)
{
    if (surface()) {
        disconnect(surface(), &SurfaceInterface::committed, this, &X11Window::handleCommitted);
    }

    setSurface(shellSurface->surface());

    if (surface()->isMapped()) {
        if (m_syncRequest.acked) {
            finishSync();
        }

        if (!m_syncRequest.enabled) {
            setReadyForPainting();
        }
    }

    connect(surface(), &SurfaceInterface::committed, this, &X11Window::handleCommitted);
}

void X11Window::checkOutput()
{
    setOutput(workspace()->outputAt(frameGeometry().center()));
}

void X11Window::getWmOpaqueRegion()
{
    const auto rects = info->opaqueRegion();
    QRegion new_opaque_region;
    for (const auto &r : rects) {
        new_opaque_region += Xcb::fromXNative(QRect(r.pos.x, r.pos.y, r.size.width, r.size.height)).toRect();
    }
    opaque_region = new_opaque_region;
}

QList<QRectF> X11Window::shapeRegion() const
{
    return m_shapeRegion;
}

void X11Window::updateShapeRegion()
{
    const QRectF bufferGeometry = this->bufferGeometry();
    const auto previousRegion = m_shapeRegion;
    if (Xcb::Extensions::self()->hasShape(window())) {
        auto cookie = xcb_shape_get_rectangles_unchecked(kwinApp()->x11Connection(), window(), XCB_SHAPE_SK_BOUNDING);
        UniqueCPtr<xcb_shape_get_rectangles_reply_t> reply(xcb_shape_get_rectangles_reply(kwinApp()->x11Connection(), cookie, nullptr));
        if (reply) {
            m_shapeRegion.clear();
            const xcb_rectangle_t *rects = xcb_shape_get_rectangles_rectangles(reply.get());
            const int rectCount = xcb_shape_get_rectangles_rectangles_length(reply.get());
            for (int i = 0; i < rectCount; ++i) {
                QRectF region = Xcb::fromXNative(QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height));
                // make sure the shape is sane (X is async, maybe even XShape is broken)
                region = region.intersected(QRectF(QPointF(0, 0), bufferGeometry.size()));

                m_shapeRegion += region;
            }
        } else {
            m_shapeRegion = {QRectF(0, 0, bufferGeometry.width(), bufferGeometry.height())};
        }
    } else {
        m_shapeRegion = {QRectF(0, 0, bufferGeometry.width(), bufferGeometry.height())};
    }
    if (m_shapeRegion != previousRegion) {
        Q_EMIT shapeChanged();
    }
}

Xcb::Property X11Window::fetchWmClientLeader() const
{
    return Xcb::Property(false, window(), atoms->wm_client_leader, XCB_ATOM_WINDOW, 0, 10000);
}

void X11Window::readWmClientLeader(Xcb::Property &prop)
{
    m_wmClientLeader = prop.value<xcb_window_t>(window());
}

void X11Window::getWmClientLeader()
{
    if (isUnmanaged()) {
        return;
    }
    auto prop = fetchWmClientLeader();
    readWmClientLeader(prop);
}

int X11Window::desktopId() const
{
    return m_desktops.isEmpty() ? -1 : m_desktops.last()->x11DesktopNumber();
}

/**
 * Returns sessionId for this window,
 * taken either from its window or from the leader window.
 */
QByteArray X11Window::sessionId() const
{
    QByteArray result = Xcb::StringProperty(window(), atoms->sm_client_id);
    if (result.isEmpty() && m_wmClientLeader && m_wmClientLeader != window()) {
        result = Xcb::StringProperty(m_wmClientLeader, atoms->sm_client_id);
    }
    return result;
}

/**
 * Returns command property for this window,
 * taken either from its window or from the leader window.
 */
QString X11Window::wmCommand()
{
    QByteArray result = Xcb::StringProperty(window(), XCB_ATOM_WM_COMMAND);
    if (result.isEmpty() && m_wmClientLeader && m_wmClientLeader != window()) {
        result = Xcb::StringProperty(m_wmClientLeader, XCB_ATOM_WM_COMMAND);
    }
    result.replace(0, ' ');
    return result;
}

/**
 * Returns client leader window for this client.
 * Returns the client window itself if no leader window is defined.
 */
xcb_window_t X11Window::wmClientLeader() const
{
    if (m_wmClientLeader != XCB_WINDOW_NONE) {
        return m_wmClientLeader;
    }
    return window();
}

void X11Window::getWmClientMachine()
{
    clientMachine()->resolve(window(), wmClientLeader());
}

Xcb::Property X11Window::fetchSkipCloseAnimation() const
{
    return Xcb::Property(false, window(), atoms->kde_skip_close_animation, XCB_ATOM_CARDINAL, 0, 1);
}

void X11Window::readSkipCloseAnimation(Xcb::Property &property)
{
    setSkipCloseAnimation(property.toBool());
}

void X11Window::getSkipCloseAnimation()
{
    Xcb::Property property = fetchSkipCloseAnimation();
    readSkipCloseAnimation(property);
}

//********************************************
// Client
//********************************************

void X11Window::updateUserTime(xcb_timestamp_t time)
{
    // copied in Group::updateUserTime
    if (time == XCB_TIME_CURRENT_TIME) {
        kwinApp()->updateXTime();
        time = xTime();
    }
    if (time != -1U
        && (m_userTime == XCB_TIME_CURRENT_TIME
            || NET::timestampCompare(time, m_userTime) > 0)) { // time > user_time
        m_userTime = time;
    }
    group()->updateUserTime(m_userTime);
}

xcb_timestamp_t X11Window::readUserCreationTime() const
{
    Xcb::Property prop(false, window(), atoms->kde_net_wm_user_creation_time, XCB_ATOM_CARDINAL, 0, 1);
    return prop.value<xcb_timestamp_t>(-1);
}

xcb_timestamp_t X11Window::readUserTimeMapTimestamp(const KStartupInfoId *asn_id, const KStartupInfoData *asn_data,
                                                    bool session) const
{
    xcb_timestamp_t time = info->userTime();
    // qDebug() << "User timestamp, initial:" << time;
    //^^ this deadlocks kwin --replace sometimes.

    // newer ASN timestamp always replaces user timestamp, unless user timestamp is 0
    // helps e.g. with konqy reusing
    if (asn_data != nullptr && time != 0) {
        if (asn_id->timestamp() != 0
            && (time == -1U || NET::timestampCompare(asn_id->timestamp(), time) > 0)) {
            time = asn_id->timestamp();
        }
    }
    qCDebug(KWIN_CORE) << "User timestamp, ASN:" << time;
    if (time == -1U) {
        // The window doesn't have any timestamp.
        // If it's the first window for its application
        // (i.e. there's no other window from the same app),
        // use the _KDE_NET_WM_USER_CREATION_TIME trick.
        // Otherwise, refuse activation of a window
        // from already running application if this application
        // is not the active one (unless focus stealing prevention is turned off).
        X11Window *act = dynamic_cast<X11Window *>(workspace()->mostRecentlyActivatedWindow());
        if (act != nullptr && !belongToSameApplication(act, this, SameApplicationCheck::RelaxedForActive)) {
            bool first_window = true;
            auto sameApplicationActiveHackPredicate = [this](const X11Window *cl) {
                // ignore already existing splashes, toolbars, utilities and menus,
                // as the app may show those before the main window
                return !cl->isSplash() && !cl->isToolbar() && !cl->isUtility() && !cl->isMenu()
                    && cl != this && X11Window::belongToSameApplication(cl, this, SameApplicationCheck::RelaxedForActive);
            };
            if (isTransient()) {
                auto clientMainClients = [this]() {
                    QList<X11Window *> ret;
                    const auto mcs = mainWindows();
                    for (auto mc : mcs) {
                        if (X11Window *c = dynamic_cast<X11Window *>(mc)) {
                            ret << c;
                        }
                    }
                    return ret;
                };
                if (act->hasTransient(this, true)) {
                    ; // is transient for currently active window, even though it's not
                    // the same app (e.g. kcookiejar dialog) -> allow activation
                } else if (groupTransient() && findInList<X11Window, X11Window>(clientMainClients(), sameApplicationActiveHackPredicate) == nullptr) {
                    ; // standalone transient
                } else {
                    first_window = false;
                }
            } else {
#if KWIN_BUILD_X11
                if (workspace()->findClient(sameApplicationActiveHackPredicate)) {
                    first_window = false;
                }
#endif
            }
            // don't refuse if focus stealing prevention is turned off
            if (!first_window && rules()->checkFSP(options->focusStealingPreventionLevel()) > FocusStealingPreventionLevel::None) {
                qCDebug(KWIN_CORE) << "User timestamp, already exists:" << 0;
                return 0; // refuse activation
            }
        }
        // Creation time would just mess things up during session startup,
        // as possibly many apps are started up at the same time.
        // If there's no active window yet, no timestamp will be needed,
        // as plain Workspace::allowWindowActivation() will return true
        // in such case. And if there's already active window,
        // it's better not to activate the new one.
        // Unless it was the active window at the time
        // of session saving and there was no user interaction yet,
        // this check will be done in manage().
        if (session) {
            return -1U;
        }
        time = readUserCreationTime();
    }
    qCDebug(KWIN_CORE) << "User timestamp, final:" << this << ":" << time;
    return time;
}

xcb_timestamp_t X11Window::userTime() const
{
    xcb_timestamp_t time = m_userTime;
    if (time == 0) { // doesn't want focus after showing
        return 0;
    }
    Q_ASSERT(group() != nullptr);
    if (time == -1U
        || (group()->userTime() != -1U
            && NET::timestampCompare(group()->userTime(), time) > 0)) {
        time = group()->userTime();
    }
    return time;
}

void X11Window::doSetActive()
{
    if (isDeleted()) {
        return;
    }
    updateUrgency(); // demand attention again if it's still urgent
    info->setState(isActive() ? NET::Focused : NET::States(), NET::Focused);
}

void X11Window::startupIdChanged()
{
    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(window(), asn_id, asn_data);
    if (!asn_valid) {
        return;
    }

    if (RootInfo::desktopEnabled()) {
        // If the ASN contains desktop, move it to the desktop, otherwise move it to the current
        // desktop (since the new ASN should make the window act like if it's a new application
        // launched). However don't affect the window's desktop if it's set to be on all desktops.

        if (asn_data.desktop() != 0 && !isOnAllDesktops()) {
            if (asn_data.desktop() == -1) {
                workspace()->sendWindowToDesktops(this, {}, true);
            } else {
                if (VirtualDesktop *desktop = VirtualDesktopManager::self()->desktopForX11Id(asn_data.desktop())) {
                    workspace()->sendWindowToDesktops(this, {desktop}, true);
                }
            }
        }
    }

    if (asn_data.xinerama() != -1) {
        Output *output = workspace()->xineramaIndexToOutput(asn_data.xinerama());
        if (output) {
            sendToOutput(output);
        }
    }
    const xcb_timestamp_t timestamp = asn_id.timestamp();
    if (timestamp != 0) {
        bool activate = allowWindowActivation(timestamp);
        if (activate) {
            workspace()->activateWindow(this);
        } else {
            demandAttention();
        }
    }
}

void X11Window::updateUrgency()
{
    if (info->urgency()) {
        demandAttention();
    }
}

// focus_in -> the window got FocusIn event
bool X11Window::allowWindowActivation(xcb_timestamp_t time, bool focus_in)
{
    auto window = this;
    // options->focusStealingPreventionLevel :
    // 0 - none    - old KWin behaviour, new windows always get focus
    // 1 - low     - focus stealing prevention is applied normally, when unsure, activation is allowed
    // 2 - normal  - focus stealing prevention is applied normally, when unsure, activation is not allowed,
    //              this is the default
    // 3 - high    - new window gets focus only if it belongs to the active application,
    //              or when no window is currently active
    // 4 - extreme - no window gets focus without user intervention
    if (time == -1U) {
        time = window->userTime();
    }
    const FocusStealingPreventionLevel level = window->rules()->checkFSP(options->focusStealingPreventionLevel());
    if (workspace()->sessionManager()->state() == SessionState::Saving && level <= FocusStealingPreventionLevel::Medium) { // <= normal
        return true;
    }
    Window *ac = workspace()->mostRecentlyActivatedWindow();
    if (focus_in) {
        if (workspace()->inShouldGetFocus(window)) {
            return true; // FocusIn was result of KWin's action
        }
        // Before getting FocusIn, the active Client already
        // got FocusOut, and therefore got deactivated.
        ac = workspace()->lastActiveWindow();
    }
    if (time == 0) { // explicitly asked not to get focus
        if (!window->rules()->checkAcceptFocus(false)) {
            return false;
        }
    }
    const FocusStealingPreventionLevel protection = ac ? ac->rules()->checkFPP(FocusStealingPreventionLevel::Medium) : FocusStealingPreventionLevel::None;

    // stealing is unconditionally allowed (NETWM behavior)
    if (level == FocusStealingPreventionLevel::None || protection == FocusStealingPreventionLevel::None) {
        return true;
    }

    // The active window "grabs" the focus or stealing is generally forbidden
    if (level == FocusStealingPreventionLevel::Extreme || protection == FocusStealingPreventionLevel::Extreme) {
        return false;
    }

    // No active window, it's ok to pass focus
    // NOTICE that extreme protection needs to be handled before to allow protection on unmanged windows
    if (ac == nullptr || ac->isDesktop()) {
        qCDebug(KWIN_CORE) << "Activation: No window active, allowing";
        return true; // no active window -> always allow
    }

    // TODO window urgency  -> return true?

    // Unconditionally allow intra-window passing around for lower stealing protections
    // unless the active window has High interest
    if (Window::belongToSameApplication(window, ac, Window::SameApplicationCheck::RelaxedForActive) && protection < FocusStealingPreventionLevel::High) {
        qCDebug(KWIN_CORE) << "Activation: Belongs to active application";
        return true;
    }

    // High FPS, not intr-window change. Only allow if the active window has only minor interest
    if (level > FocusStealingPreventionLevel::Medium && protection > FocusStealingPreventionLevel::Low) {
        return false;
    }

    if (time == -1U) { // no time known
        qCDebug(KWIN_CORE) << "Activation: No timestamp at all";
        // Only allow for Low protection unless active window has High interest in focus
        if (level < FocusStealingPreventionLevel::Medium && protection < FocusStealingPreventionLevel::High) {
            return true;
        }
        // no timestamp at all, don't activate - because there's also creation timestamp
        // done on CreateNotify, this case should happen only in case application
        // maps again already used window, i.e. this won't happen after app startup
        return false;
    }

    // Low or medium FSP, usertime comparison is possible
    const xcb_timestamp_t user_time = ac->userTime();
    qCDebug(KWIN_CORE) << "Activation, compared:" << window << ":" << time << ":" << user_time
                       << ":" << (NET::timestampCompare(time, user_time) >= 0);
    return NET::timestampCompare(time, user_time) >= 0; // time >= user_time
}

void X11Window::restackWindow(xcb_window_t above, int detail, NET::RequestSource src, xcb_timestamp_t timestamp)
{
    X11Window *other = workspace()->findClient(above);
    if (detail == XCB_STACK_MODE_OPPOSITE) {
        if (!other) {
            workspace()->raiseOrLowerWindow(this);
            return;
        }
        const auto stack = workspace()->stackingOrder();
        for (Window *window : stack) {
            if (window == this) {
                detail = XCB_STACK_MODE_ABOVE;
                break;
            } else if (window == other) {
                detail = XCB_STACK_MODE_BELOW;
                break;
            }
        }
    } else if (detail == XCB_STACK_MODE_TOP_IF) {
        if (other && other->frameGeometry().intersects(frameGeometry())) {
            workspace()->raiseWindowRequest(this, src, timestamp);
        }
        return;
    } else if (detail == XCB_STACK_MODE_BOTTOM_IF) {
        if (other && other->frameGeometry().intersects(frameGeometry())) {
            workspace()->lowerWindowRequest(this, src, timestamp);
        }
        return;
    }

    if (detail == XCB_STACK_MODE_ABOVE) {
        if (other) {
            workspace()->stackAbove(this, other);
        } else {
            workspace()->raiseWindowRequest(this, src, timestamp);
        }
    } else if (detail == XCB_STACK_MODE_BELOW) {
        if (other) {
            workspace()->stackBelow(this, other);
        } else {
            workspace()->lowerWindowRequest(this, src, timestamp);
        }
    }
}

bool X11Window::belongsToDesktop() const
{
    const auto members = group()->members();
    for (const X11Window *window : members) {
        if (window->isDesktop()) {
            return true;
        }
    }
    return false;
}

void X11Window::setShortcutInternal()
{
    updateCaption();
#if 0
    workspace()->windowShortcutUpdated(this);
#else
    // Workaround for kwin<->kglobalaccel deadlock, when KWin has X grab and the kded
    // kglobalaccel module tries to create the key grab. KWin should preferably grab
    // they keys itself anyway :(.
    QTimer::singleShot(0, this, std::bind(&Workspace::windowShortcutUpdated, workspace(), this));
#endif
}

bool X11Window::hitTest(const QPointF &point) const
{
    if (isDecorated()) {
        if (m_decoration.inputRegion.contains(flooredPoint(mapToFrame(point)))) {
            return true;
        }
    }
    if (!m_surface || (m_surface->isMapped() && !m_surface->inputSurfaceAt(mapToLocal(point)))) {
        return false;
    }
    return std::ranges::any_of(m_shapeRegion, [local = mapToLocal(point)](const QRectF &rect) {
        return exclusiveContains(rect, local);
    });
}

} // namespace

#include "moc_x11window.cpp"
