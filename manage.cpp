/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// This file contains things relevant to handling incoming events.

#include "client.h"

#include <kstartupinfo.h>

#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "cursor.h"
#include "rules.h"
#include "group.h"
#include "netinfo.h"
#include "screens.h"
#include "workspace.h"
#include "xcbutils.h"

#include <xcb/shape.h>

namespace KWin
{

/**
 * Manages the clients. This means handling the very first maprequest:
 * reparenting, initial geometry, initial state, placement, etc.
 * Returns false if KWin is not going to manage this window.
 */
bool Client::manage(xcb_window_t w, bool isMapped)
{
    StackingUpdatesBlocker stacking_blocker(workspace());

    Xcb::WindowAttributes attr(w);
    Xcb::WindowGeometry windowGeometry(w);
    if (attr.isNull() || windowGeometry.isNull()) {
        return false;
    }

    // From this place on, manage() must not return false
    blockGeometryUpdates();
    setPendingGeometryUpdate(PendingGeometryForced); // Force update when finishing with geometry changes

    embedClient(w, attr->visual, attr->colormap, windowGeometry->depth);

    m_visual = attr->visual;
    bit_depth = windowGeometry->depth;

    // SELI TODO: Order all these things in some sane manner

    const NET::Properties properties =
        NET::WMDesktop |
        NET::WMState |
        NET::WMWindowType |
        NET::WMStrut |
        NET::WMName |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMIconName;
    const NET::Properties2 properties2 =
        NET::WM2BlockCompositing |
        NET::WM2WindowClass |
        NET::WM2WindowRole |
        NET::WM2UserTime |
        NET::WM2StartupId |
        NET::WM2ExtendedStrut |
        NET::WM2Opacity |
        NET::WM2FullscreenMonitors |
        NET::WM2FrameOverlap |
        NET::WM2GroupLeader |
        NET::WM2Urgency |
        NET::WM2Input |
        NET::WM2Protocols |
        NET::WM2InitialMappingState |
        NET::WM2IconPixmap |
        NET::WM2OpaqueRegion |
        NET::WM2DesktopFileName;

    auto wmClientLeaderCookie = fetchWmClientLeader();
    auto skipCloseAnimationCookie = fetchSkipCloseAnimation();
    auto gtkFrameExtentsCookie = fetchGtkFrameExtents();
    auto showOnScreenEdgeCookie = fetchShowOnScreenEdge();
    auto colorSchemeCookie = fetchColorScheme();
    auto firstInTabBoxCookie = fetchFirstInTabBox();
    auto transientCookie = fetchTransient();
    auto activitiesCookie = fetchActivities();
    m_geometryHints.init(window());
    m_motif.init(window());
    info = new WinInfo(this, m_client, rootWindow(), properties, properties2);

    if (isDesktop() && bit_depth == 32) {
        // force desktop windows to be opaque. It's a desktop after all, there is no window below
        bit_depth = 24;
    }

    // If it's already mapped, ignore hint
    bool init_minimize = !isMapped && (info->initialMappingState() == NET::Iconic);

    m_colormap = attr->colormap;

    getResourceClass();
    readWmClientLeader(wmClientLeaderCookie);
    getWmClientMachine();
    getSyncCounter();
    // First only read the caption text, so that setupWindowRules() can use it for matching,
    // and only then really set the caption using setCaption(), which checks for duplicates etc.
    // and also relies on rules already existing
    cap_normal = readName();
    setupWindowRules(false);
    setCaption(cap_normal, true);

    if (Xcb::Extensions::self()->isShapeAvailable())
        xcb_shape_select_input(connection(), window(), true);
    detectShape(window());
    readGtkFrameExtents(gtkFrameExtentsCookie);
    detectNoBorder();
    fetchIconicName();

    // Needs to be done before readTransient() because of reading the group
    checkGroup();
    updateUrgency();
    updateAllowedActions(); // Group affects isMinimizable()

    setModal((info->state() & NET::Modal) != 0);   // Needs to be valid before handling groups
    readTransientProperty(transientCookie);
    setDesktopFileName(QByteArray(info->desktopFileName()));
    getIcons();
    connect(this, &Client::desktopFileNameChanged, this, &Client::getIcons);

    m_geometryHints.read();
    getMotifHints();
    getWmOpaqueRegion();
    readSkipCloseAnimation(skipCloseAnimationCookie);

    // TODO: Try to obey all state information from info->state()

    setOriginalSkipTaskbar((info->state() & NET::SkipTaskbar) != 0);
    setSkipPager((info->state() & NET::SkipPager) != 0);
    readFirstInTabBox(firstInTabBoxCookie);

    setupCompositing();

    KStartupInfoId asn_id;
    KStartupInfoData asn_data;
    bool asn_valid = workspace()->checkStartupNotification(window(), asn_id, asn_data);

    // Make sure that the input window is created before we update the stacking order
    updateInputWindow();

    workspace()->updateClientLayer(this);

    SessionInfo* session = workspace()->takeSessionInfo(this);
    if (session) {
        init_minimize = session->minimized;
        noborder = session->noBorder;
    }

    setShortcut(rules()->checkShortcut(session ? session->shortcut : QString(), true));

    init_minimize = rules()->checkMinimize(init_minimize, !isMapped);
    noborder = rules()->checkNoBorder(noborder, !isMapped);

    readActivities(activitiesCookie);

    // Initial desktop placement
    int desk = 0;
    if (session) {
        desk = session->desktop;
        if (session->onAllDesktops)
            desk = NET::OnAllDesktops;
        setOnActivities(session->activities);
    } else {
        // If this window is transient, ensure that it is opened on the
        // same window as its parent.  this is necessary when an application
        // starts up on a different desktop than is currently displayed
        if (isTransient()) {
            auto mainclients = mainClients();
            bool on_current = false;
            bool on_all = false;
            AbstractClient* maincl = nullptr;
            // This is slightly duplicated from Placement::placeOnMainWindow()
            for (auto it = mainclients.constBegin();
                    it != mainclients.constEnd();
                    ++it) {
                if (mainclients.count() > 1 &&      // A group-transient
                    (*it)->isSpecialWindow() &&     // Don't consider toolbars etc when placing
                    !(info->state() & NET::Modal))  // except when it's modal (blocks specials as well)
                    continue;
                maincl = *it;
                if ((*it)->isOnCurrentDesktop())
                    on_current = true;
                if ((*it)->isOnAllDesktops())
                    on_all = true;
            }
            if (on_all)
                desk = NET::OnAllDesktops;
            else if (on_current)
                desk = VirtualDesktopManager::self()->current();
            else if (maincl != NULL)
                desk = maincl->desktop();

            if (maincl)
                setOnActivities(maincl->activities());
        } else { // a transient shall appear on its leader and not drag that around
            if (info->desktop())
                desk = info->desktop(); // Window had the initial desktop property, force it
            if (desktop() == 0 && asn_valid && asn_data.desktop() != 0)
                desk = asn_data.desktop();
        }
#ifdef KWIN_BUILD_ACTIVITIES
        if (Activities::self() && !isMapped && !noborder && isNormalWindow() && !activitiesDefined) {
            //a new, regular window, when we're not recovering from a crash,
            //and it hasn't got an activity. let's try giving it the current one.
            //TODO: decide whether to keep this before the 4.6 release
            //TODO: if we are keeping it (at least as an option), replace noborder checking
            //with a public API for setting windows to be on all activities.
            //something like KWindowSystem::setOnAllActivities or
            //KActivityConsumer::setOnAllActivities
            setOnActivity(Activities::self()->current(), true);
        }
#endif
    }

    if (desk == 0)   // Assume window wants to be visible on the current desktop
        desk = isDesktop() ? static_cast<int>(NET::OnAllDesktops) : VirtualDesktopManager::self()->current();
    desk = rules()->checkDesktop(desk, !isMapped);
    if (desk != NET::OnAllDesktops)   // Do range check
        desk = qBound(1, desk, static_cast<int>(VirtualDesktopManager::self()->count()));
    setDesktop(desk);
    info->setDesktop(desk);
    workspace()->updateOnAllDesktopsOfTransients(this);   // SELI TODO
    //onAllDesktopsChange(); // Decoration doesn't exist here yet

    QString activitiesList;
    activitiesList = rules()->checkActivity(activitiesList, !isMapped);
    if (!activitiesList.isEmpty())
        setOnActivities(activitiesList.split(QStringLiteral(",")));

    QRect geom(windowGeometry.rect());
    bool placementDone = false;

    if (session)
        geom = session->geometry;

    QRect area;
    bool partial_keep_in_area = isMapped || session;
    if (isMapped || session) {
        area = workspace()->clientArea(FullArea, geom.center(), desktop());
        checkOffscreenPosition(&geom, area);
    } else {
        int screen = asn_data.xinerama() == -1 ? screens()->current() : asn_data.xinerama();
        screen = rules()->checkScreen(screen, !isMapped);
        area = workspace()->clientArea(PlacementArea, screens()->geometry(screen).center(), desktop());
    }

    if (int type = checkFullScreenHack(geom)) {
        fullscreen_mode = FullScreenHack;
        if (rules()->checkStrictGeometry(false)) {
            geom = type == 2 // 1 = It's xinerama-aware fullscreen hack, 2 = It's full area
                   ? workspace()->clientArea(FullArea, geom.center(), desktop())
                   : workspace()->clientArea(ScreenArea, geom.center(), desktop());
        } else
            geom = workspace()->clientArea(FullScreenArea, geom.center(), desktop());
        placementDone = true;
    }

    if (isDesktop())
        // KWin doesn't manage desktop windows
        placementDone = true;

    bool usePosition = false;
    if (isMapped || session || placementDone)
        placementDone = true; // Use geometry
    else if (isTransient() && !isUtility() && !isDialog() && !isSplash())
        usePosition = true;
    else if (isTransient() && !hasNETSupport())
        usePosition = true;
    else if (isDialog() && hasNETSupport()) {
        // If the dialog is actually non-NETWM transient window, don't try to apply placement to it,
        // it breaks with too many things (xmms, display)
        if (mainClients().count() >= 1) {
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
        } else
            usePosition = true;
    } else if (isSplash())
        ; // Force using placement policy
    else
        usePosition = true;
    if (!rules()->checkIgnoreGeometry(!usePosition, true)) {
        if (m_geometryHints.hasPosition()) {
            placementDone = true;
            // Disobey xinerama placement option for now (#70943)
            area = workspace()->clientArea(PlacementArea, geom.center(), desktop());
        }
    }
    //if ( true ) // Size is always obeyed for now, only with constraints applied
    //    if (( xSizeHint.flags & USSize ) || ( xSizeHint.flags & PSize ))
    //        {
    //        // Keep in mind that we now actually have a size :-)
    //        }

    if (m_geometryHints.hasMaxSize())
        geom.setSize(geom.size().boundedTo(
                         rules()->checkMaxSize(m_geometryHints.maxSize())));
    if (m_geometryHints.hasMinSize())
        geom.setSize(geom.size().expandedTo(
                         rules()->checkMinSize(m_geometryHints.minSize())));

    if (isMovable() && (geom.x() > area.right() || geom.y() > area.bottom()))
        placementDone = false; // Weird, do not trust.

    if (placementDone)
        move(geom.x(), geom.y());   // Before gravitating

    // Create client group if the window will have a decoration
    bool dontKeepInArea = false;
    setTabGroup(NULL);
    if (!noBorder() && false) {
        const bool autogrouping = rules()->checkAutogrouping(options->isAutogroupSimilarWindows());
        const bool autogroupInFg = rules()->checkAutogroupInForeground(options->isAutogroupInForeground());
        // Automatically add to previous groups on session restore
        if (session && session->tabGroupClient && !workspace()->hasClient(session->tabGroupClient))
            session->tabGroupClient = NULL;
        if (session && session->tabGroupClient && session->tabGroupClient != this) {
            tabBehind(session->tabGroupClient, autogroupInFg);
        } else if (isMapped && autogrouping) {
            // If the window is already mapped (Restarted KWin) add any windows that already have the
            // same geometry to the same client group. (May incorrectly handle maximized windows)
            foreach (Client *other, workspace()->clientList()) {
                if (other->maximizeMode() != MaximizeFull &&
                    geom == QRect(other->pos(), other->clientSize()) &&
                    desk == other->desktop() && activities() == other->activities()) {

                    tabBehind(other, autogroupInFg);
                    break;

                }
            }
        }
        if (!(tab_group || isMapped || session)) {
            // Attempt to automatically group similar windows
            Client* similar = findAutogroupCandidate();
            if (similar && !similar->noBorder()) {
                if (autogroupInFg) {
                    similar->setDesktop(desk); // can happen when grouping by id. ...
                    similar->setMinimized(false); // ... or anyway - still group, but "here" and visible
                }
                if (!similar->isMinimized()) { // do not attempt to tab in background of a hidden group
                    geom = QRect(similar->pos() + similar->clientPos(), similar->clientSize());
                    updateDecoration(false);
                    if (tabBehind(similar, autogroupInFg)) {
                        // Don't move entire group
                        geom = QRect(similar->pos() + similar->clientPos(), similar->clientSize());
                        placementDone = true;
                        dontKeepInArea = true;
                    }
                }
            }
        }
    }

    readColorScheme(colorSchemeCookie);

    updateDecoration(false);   // Also gravitates
    // TODO: Is CentralGravity right here, when resizing is done after gravitating?
    plainResize(rules()->checkSize(sizeForClientSize(geom.size()), !isMapped));

    QPoint forced_pos = rules()->checkPosition(invalidPoint, !isMapped);
    if (forced_pos != invalidPoint) {
        move(forced_pos);
        placementDone = true;
        // Don't keep inside workarea if the window has specially configured position
        partial_keep_in_area = true;
        area = workspace()->clientArea(FullArea, geom.center(), desktop());
    }
    if (!placementDone) {
        // Placement needs to be after setting size
        Placement::self()->place(this, area);
        dontKeepInArea = true;
        placementDone = true;
    }

    // bugs #285967, #286146, #183694
    // geometry() now includes the requested size and the decoration and is at the correct screen/position (hopefully)
    // Maximization for oversized windows must happen NOW.
    // If we effectively pass keepInArea(), the window will resizeWithChecks() - i.e. constrained
    // to the combo of all screen MINUS all struts on the edges
    // If only one screen struts, this will affect screens as a side-effect, the window is artificailly shrinked
    // below the screen size and as result no more maximized what breaks KMainWindow's stupid width+1, height+1 hack
    // TODO: get KMainWindow a correct state storage what will allow to store the restore size as well.

    if (!session) { // has a better handling of this
        geom_restore = geometry(); // Remember restore geometry
        if (isMaximizable() && (width() >= area.width() || height() >= area.height())) {
            // Window is too large for the screen, maximize in the
            // directions necessary
            const QSize ss = workspace()->clientArea(ScreenArea, area.center(), desktop()).size();
            const QRect fsa = workspace()->clientArea(FullArea, geom.center(), desktop());
            const QSize cs = clientSize();
            int pseudo_max = ((info->state() & NET::MaxVert) ? MaximizeVertical : 0) |
                             ((info->state() & NET::MaxHoriz) ? MaximizeHorizontal : 0);
            if (width() >= area.width())
                pseudo_max |=  MaximizeHorizontal;
            if (height() >= area.height())
                pseudo_max |=  MaximizeVertical;

            // heuristics:
            // if decorated client is smaller than the entire screen, the user might want to move it around (multiscreen)
            // in this case, if the decorated client is bigger than the screen (+1), we don't take this as an
            // attempt for maximization, but just constrain the size (the window simply wants to be bigger)
            // NOTICE
            // i intended a second check on cs < area.size() ("the managed client ("minus border") is smaller
            // than the workspace") but gtk / gimp seems to store it's size including the decoration,
            // thus a former maximized window wil become non-maximized
            bool keepInFsArea = false;
            if (width() < fsa.width() && (cs.width() > ss.width()+1)) {
                pseudo_max &= ~MaximizeHorizontal;
                keepInFsArea = true;
            }
            if (height() < fsa.height() && (cs.height() > ss.height()+1)) {
                pseudo_max &= ~MaximizeVertical;
                keepInFsArea = true;
            }

            if (pseudo_max != MaximizeRestore) {
                maximize((MaximizeMode)pseudo_max);
                // from now on, care about maxmode, since the maximization call will override mode for fix aspects
                dontKeepInArea |= (max_mode == MaximizeFull);
                geom_restore = QRect(); // Use placement when unmaximizing ...
                if (!(max_mode & MaximizeVertical)) {
                    geom_restore.setY(y());   // ...but only for horizontal direction
                    geom_restore.setHeight(height());
                }
                if (!(max_mode & MaximizeHorizontal)) {
                    geom_restore.setX(x());   // ...but only for vertical direction
                    geom_restore.setWidth(width());
                }
            }
            if (keepInFsArea)
                keepInArea(fsa, partial_keep_in_area);
        }
    }

    if ((!isSpecialWindow() || isToolbar()) && isMovable() && !dontKeepInArea)
        keepInArea(area, partial_keep_in_area);

    updateShape();

    // CT: Extra check for stupid jdk 1.3.1. But should make sense in general
    // if client has initial state set to Iconic and is transient with a parent
    // window that is not Iconic, set init_state to Normal
    if (init_minimize && isTransient()) {
        auto mainclients = mainClients();
        for (auto it = mainclients.constBegin();
                it != mainclients.constEnd();
                ++it)
            if ((*it)->isShown(true))
                init_minimize = false; // SELI TODO: Even e.g. for NET::Utility?
    }
    // If a dialog is shown for minimized window, minimize it too
    if (!init_minimize && isTransient() && mainClients().count() > 0 && !workspace()->sessionSaving()) {
        bool visible_parent = false;
        // Use allMainClients(), to include also main clients of group transients
        // that have been optimized out in Client::checkGroupTransients()
        auto mainclients = allMainClients();
        for (auto it = mainclients.constBegin();
                it != mainclients.constEnd();
                ++it)
            if ((*it)->isShown(true))
                visible_parent = true;
        if (!visible_parent) {
            init_minimize = true;
            demandAttention();
        }
    }

    if (init_minimize)
        minimize(true);   // No animation

    // Other settings from the previous session
    if (session) {
        // Session restored windows are not considered to be new windows WRT rules,
        // I.e. obey only forcing rules
        setKeepAbove(session->keepAbove);
        setKeepBelow(session->keepBelow);
        setOriginalSkipTaskbar(session->skipTaskbar);
        setSkipPager(session->skipPager);
        setSkipSwitcher(session->skipSwitcher);
        setShade(session->shaded ? ShadeNormal : ShadeNone);
        setOpacity(session->opacity);
        geom_restore = session->restore;
        if (session->maximized != MaximizeRestore) {
            maximize(MaximizeMode(session->maximized));
        }
        if (session->fullscreen == FullScreenHack)
            ; // Nothing, this should be already set again above
        else if (session->fullscreen != FullScreenNone) {
            setFullScreen(true, false);
            geom_fs_restore = session->fsrestore;
        }
        checkOffscreenPosition(&geom_restore, area);
        checkOffscreenPosition(&geom_fs_restore, area);
    } else {
        // Window may want to be maximized
        // done after checking that the window isn't larger than the workarea, so that
        // the restore geometry from the checks above takes precedence, and window
        // isn't restored larger than the workarea
        MaximizeMode maxmode = static_cast<MaximizeMode>(
                                   ((info->state() & NET::MaxVert) ? MaximizeVertical : 0) |
                                   ((info->state() & NET::MaxHoriz) ? MaximizeHorizontal : 0));
        MaximizeMode forced_maxmode = rules()->checkMaximize(maxmode, !isMapped);

        // Either hints were set to maximize, or is forced to maximize,
        // or is forced to non-maximize and hints were set to maximize
        if (forced_maxmode != MaximizeRestore || maxmode != MaximizeRestore)
            maximize(forced_maxmode);

        // Read other initial states
        setShade(rules()->checkShade(info->state() & NET::Shaded ? ShadeNormal : ShadeNone, !isMapped));
        setKeepAbove(rules()->checkKeepAbove(info->state() & NET::KeepAbove, !isMapped));
        setKeepBelow(rules()->checkKeepBelow(info->state() & NET::KeepBelow, !isMapped));
        setOriginalSkipTaskbar(rules()->checkSkipTaskbar(info->state() & NET::SkipTaskbar, !isMapped));
        setSkipPager(rules()->checkSkipPager(info->state() & NET::SkipPager, !isMapped));
        setSkipSwitcher(rules()->checkSkipSwitcher(false, !isMapped));
        if (info->state() & NET::DemandsAttention)
            demandAttention();
        if (info->state() & NET::Modal)
            setModal(true);
        if (fullscreen_mode != FullScreenHack)
            setFullScreen(rules()->checkFullScreen(info->state() & NET::FullScreen, !isMapped), false);
    }

    updateAllowedActions(true);

    // Set initial user time directly
    m_userTime = readUserTimeMapTimestamp(asn_valid ? &asn_id : NULL, asn_valid ? &asn_data : NULL, session);
    group()->updateUserTime(m_userTime);   // And do what Client::updateUserTime() does

    // This should avoid flicker, because real restacking is done
    // only after manage() finishes because of blocking, but the window is shown sooner
    m_frame.lower();
    if (session && session->stackingOrder != -1) {
        sm_stacking_order = session->stackingOrder;
        workspace()->restoreSessionStackingOrder(this);
    }

    if (compositing())
        // Sending ConfigureNotify is done when setting mapping state below,
        // Getting the first sync response means window is ready for compositing
        sendSyncRequest();
    else
        ready_for_painting = true; // set to true in case compositing is turned on later. bug #160393

    if (isShown(true)) {
        bool allow;
        if (session)
            allow = session->active &&
                    (!workspace()->wasUserInteraction() || workspace()->activeClient() == NULL ||
                     workspace()->activeClient()->isDesktop());
        else
            allow = workspace()->allowClientActivation(this, userTime(), false);

        if (!(isMapped || session)) {
            if (workspace()->sessionSaving()) {
                /*
                 * If we get a new window during session saving, we assume it's some 'save file?' dialog
                 * which the user really needs to see (to know why logout's stalled).
                 *
                 * Given the current session management protocol, I can't see a nicer way of doing this.
                 * Someday I'd like to see a protocol that tells the windowmanager who's doing SessionInteract.
                 */
                needsSessionInteract = true;
                //show the parent too
                auto mainclients = mainClients();
                for (auto it = mainclients.constBegin();
                        it != mainclients.constEnd(); ++it) {
                    if (Client *mc = dynamic_cast<Client*>((*it))) {
                        mc->setSessionInteract(true);
                    }
                    (*it)->unminimize();
                }
            } else if (allow) {
                // also force if activation is allowed
                if (!isOnCurrentDesktop() && options->focusPolicyIsReasonable()) {
                    VirtualDesktopManager::self()->setCurrent(desktop());
                }
                /*if (!isOnCurrentActivity()) {
                    workspace()->setCurrentActivity( activities().first() );
                } FIXME no such method*/
            }
        }

        if (isOnCurrentDesktop() && !isMapped && !allow && (!session || session->stackingOrder < 0))
            workspace()->restackClientUnderActive(this);

        updateVisibility();

        if (!isMapped) {
            if (allow && isOnCurrentDesktop()) {
                if (!isSpecialWindow())
                    if (options->focusPolicyIsReasonable() && wantsTabFocus())
                        workspace()->requestFocus(this);
            } else if (!session && !isSpecialWindow())
                demandAttention();
        }
    } else
        updateVisibility();
    assert(mapping_state != Withdrawn);
    m_managed = true;
    blockGeometryUpdates(false);

    if (m_userTime == XCB_TIME_CURRENT_TIME || m_userTime == -1U) {
        // No known user time, set something old
        m_userTime = xTime() - 1000000;
        if (m_userTime == XCB_TIME_CURRENT_TIME || m_userTime == -1U)   // Let's be paranoid
            m_userTime = xTime() - 1000000 + 10;
    }

    //sendSyntheticConfigureNotify(); // Done when setting mapping state

    delete session;

    client_rules.discardTemporary();
    applyWindowRules(); // Just in case
    RuleBook::self()->discardUsed(this, false);   // Remove ApplyNow rules
    updateWindowRules(Rules::All); // Was blocked while !isManaged()

    setBlockingCompositing(info->isBlockingCompositing());
    readShowOnScreenEdge(showOnScreenEdgeCookie);

    // TODO: there's a small problem here - isManaged() depends on the mapping state,
    // but this client is not yet in Workspace's client list at this point, will
    // be only done in addClient()
    emit clientManaging(this);
    return true;
}

// Called only from manage()
void Client::embedClient(xcb_window_t w, xcb_visualid_t visualid, xcb_colormap_t colormap, uint8_t depth)
{
    assert(m_client == XCB_WINDOW_NONE);
    assert(frameId() == XCB_WINDOW_NONE);
    assert(m_wrapper == XCB_WINDOW_NONE);
    m_client.reset(w, false);

    const uint32_t zero_value = 0;

    xcb_connection_t *conn = connection();

    // We don't want the window to be destroyed when we quit
    xcb_change_save_set(conn, XCB_SET_MODE_INSERT, m_client);

    m_client.selectInput(zero_value);
    m_client.unmap();
    m_client.setBorderWidth(zero_value);

    // Note: These values must match the order in the xcb_cw_t enum
    const uint32_t cw_values[] = {
        0,                                // back_pixmap
        0,                                // border_pixel
        colormap,                    // colormap
        Cursor::x11Cursor(Qt::ArrowCursor)
    };

    const uint32_t cw_mask = XCB_CW_BACK_PIXMAP | XCB_CW_BORDER_PIXEL |
                             XCB_CW_COLORMAP | XCB_CW_CURSOR;

    const uint32_t common_event_mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                                       XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
                                       XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                       XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION |
                                       XCB_EVENT_MASK_KEYMAP_STATE |
                                       XCB_EVENT_MASK_FOCUS_CHANGE |
                                       XCB_EVENT_MASK_EXPOSURE |
                                       XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

    const uint32_t frame_event_mask   = common_event_mask | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_VISIBILITY_CHANGE;
    const uint32_t wrapper_event_mask = common_event_mask | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

    const uint32_t client_event_mask = XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE |
                                       XCB_EVENT_MASK_COLOR_MAP_CHANGE |
                                       XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
                                       XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;

    // Create the frame window
    xcb_window_t frame = xcb_generate_id(conn);
    xcb_create_window(conn, depth, frame, rootWindow(), 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visualid, cw_mask, cw_values);
    m_frame.reset(frame);

    setWindowHandles(m_client);

    // Create the wrapper window
    xcb_window_t wrapperId = xcb_generate_id(conn);
    xcb_create_window(conn, depth, wrapperId, frame, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visualid, cw_mask, cw_values);
    m_wrapper.reset(wrapperId);

    m_client.reparent(m_wrapper);

    // We could specify the event masks when we create the windows, but the original
    // Xlib code didn't.  Let's preserve that behavior here for now so we don't end up
    // receiving any unexpected events from the wrapper creation or the reparenting.
    m_frame.selectInput(frame_event_mask);
    m_wrapper.selectInput(wrapper_event_mask);
    m_client.selectInput(client_event_mask);

    updateMouseGrab();
}

// To accept "mainwindow#1" to "mainwindow#2"
static QByteArray truncatedWindowRole(QByteArray a)
{
    int i = a.indexOf('#');
    if (i == -1)
        return a;
    QByteArray b(a);
    b.truncate(i);
    return b;
}

Client* Client::findAutogroupCandidate() const
{
    // Attempt to find a similar window to the input. If we find multiple possibilities that are in
    // different groups then ignore all of them. This function is for automatic window grouping.
    Client *found = NULL;

    // See if the window has a group ID to match with
    QString wGId = rules()->checkAutogroupById(QString());
    if (!wGId.isEmpty()) {
        foreach (Client *c, workspace()->clientList()) {
            if (activities() != c->activities())
                continue; // don't cross activities
            if (wGId == c->rules()->checkAutogroupById(QString())) {
                if (found && found->tabGroup() != c->tabGroup()) { // We've found two, ignore both
                    found = NULL;
                    break; // Continue to the next test
                }
                found = c;
            }
        }
        if (found)
            return found;
    }

    // If this is a transient window don't take a guess
    if (isTransient())
        return NULL;

    // If we don't have an ID take a guess
    if (rules()->checkAutogrouping(options->isAutogroupSimilarWindows())) {
        QByteArray wRole = truncatedWindowRole(windowRole());
        foreach (Client *c, workspace()->clientList()) {
            if (desktop() != c->desktop() || activities() != c->activities())
                continue;
            QByteArray wRoleB = truncatedWindowRole(c->windowRole());
            if (resourceClass() == c->resourceClass() &&  // Same resource class
                    wRole == wRoleB && // Same window role
                    c->isNormalWindow()) { // Normal window TODO: Can modal windows be "normal"?
                if (found && found->tabGroup() != c->tabGroup())   // We've found two, ignore both
                    return NULL;
                found = c;
            }
        }
    }

    return found;
}

} // namespace
