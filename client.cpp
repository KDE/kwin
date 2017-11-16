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
// own
#include "client.h"
// kwin
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "atoms.h"
#include "client_machine.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "focuschain.h"
#include "group.h"
#include "shadow.h"
#include "workspace.h"
#include "screenedge.h"
#include "decorations/decorationbridge.h"
#include "decorations/decoratedclient.h"
#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>
// KDE
#include <KLocalizedString>
#include <KWindowSystem>
#include <KColorScheme>
// Qt
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMouseEvent>
#include <QProcess>
// XLib
#include <X11/Xutil.h>
#include <fixx11h.h>
#include <xcb/xcb_icccm.h>
// system
#include <unistd.h>
#include <signal.h>

// Put all externs before the namespace statement to allow the linker
// to resolve them properly

namespace KWin
{

const long ClientWinMask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                           XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                           XCB_EVENT_MASK_KEYMAP_STATE |
                           XCB_EVENT_MASK_BUTTON_MOTION |
                           XCB_EVENT_MASK_POINTER_MOTION | // need this, too!
                           XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
                           XCB_EVENT_MASK_FOCUS_CHANGE |
                           XCB_EVENT_MASK_EXPOSURE |
                           XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                           XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;

// Creating a client:
//  - only by calling Workspace::createClient()
//      - it creates a new client and calls manage() for it
//
// Destroying a client:
//  - destroyClient() - only when the window itself has been destroyed
//      - releaseWindow() - the window is kept, only the client itself is destroyed

/**
 * \class Client client.h
 * \brief The Client class encapsulates a window decoration frame.
 */

/**
 * This ctor is "dumb" - it only initializes data. All the real initialization
 * is done in manage().
 */
Client::Client()
    : AbstractClient()
    , m_client()
    , m_wrapper()
    , m_frame()
    , m_activityUpdatesBlocked(false)
    , m_blockedActivityUpdatesRequireTransients(false)
    , m_moveResizeGrabWindow()
    , move_resize_has_keyboard_grab(false)
    , m_managed(false)
    , m_transientForId(XCB_WINDOW_NONE)
    , m_originalTransientForId(XCB_WINDOW_NONE)
    , shade_below(NULL)
    , m_motif(atoms->motif_wm_hints)
    , blocks_compositing(false)
    , shadeHoverTimer(NULL)
    , m_colormap(XCB_COLORMAP_NONE)
    , in_group(NULL)
    , tab_group(NULL)
    , ping_timer(NULL)
    , m_killHelperPID(0)
    , m_pingTimestamp(XCB_TIME_CURRENT_TIME)
    , m_userTime(XCB_TIME_CURRENT_TIME)   // Not known yet
    , allowed_actions(0)
    , shade_geometry_change(false)
    , sm_stacking_order(-1)
    , activitiesDefined(false)
    , sessionActivityOverride(false)
    , needsXWindowMove(false)
    , m_decoInputExtent()
    , m_focusOutTimer(nullptr)
    , m_clientSideDecorated(false)
{
    // TODO: Do all as initialization
    syncRequest.counter = syncRequest.alarm = XCB_NONE;
    syncRequest.timeout = syncRequest.failsafeTimeout = NULL;
    syncRequest.lastTimestamp = xTime();
    syncRequest.isPending = false;

    // Set the initial mapping state
    mapping_state = Withdrawn;

    info = NULL;

    shade_mode = ShadeNone;
    deleting = false;
    fullscreen_mode = FullScreenNone;
    hidden = false;
    noborder = false;
    app_noborder = false;
    ignore_focus_stealing = false;
    check_active_modal = false;

    max_mode = MaximizeRestore;

    //Client to workspace connections require that each
    //client constructed be connected to the workspace wrapper

    geom = QRect(0, 0, 100, 100);   // So that decorations don't start with size being (0,0)
    client_size = QSize(100, 100);
    ready_for_painting = false; // wait for first damage or sync reply

    connect(clientMachine(), &ClientMachine::localhostChanged, this, &Client::updateCaption);
    connect(options, &Options::condensedTitleChanged, this, &Client::updateCaption);

    connect(this, &Client::moveResizeCursorChanged, this, [this] (Qt::CursorShape cursor) {
        xcb_cursor_t nativeCursor = Cursor::x11Cursor(cursor);
        m_frame.defineCursor(nativeCursor);
        if (m_decoInputExtent.isValid())
            m_decoInputExtent.defineCursor(nativeCursor);
        if (isMoveResize()) {
            // changing window attributes doesn't change cursor if there's pointer grab active
            xcb_change_active_pointer_grab(connection(), nativeCursor, xTime(),
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW);
        }
    });

    // SELI TODO: Initialize xsizehints??
}

/**
 * "Dumb" destructor.
 */
Client::~Client()
{
    if (m_killHelperPID && !::kill(m_killHelperPID, 0)) { // means the process is alive
        ::kill(m_killHelperPID, SIGTERM);
        m_killHelperPID = 0;
    }
    //SWrapper::Client::clientRelease(this);
    if (syncRequest.alarm != XCB_NONE)
        xcb_sync_destroy_alarm(connection(), syncRequest.alarm);
    assert(!isMoveResize());
    assert(m_client == XCB_WINDOW_NONE);
    assert(m_wrapper == XCB_WINDOW_NONE);
    //assert( frameId() == None );
    assert(!check_active_modal);
    for (auto it = m_connections.constBegin(); it != m_connections.constEnd(); ++it) {
        disconnect(*it);
    }
}

// Use destroyClient() or releaseWindow(), Client instances cannot be deleted directly
void Client::deleteClient(Client* c)
{
    delete c;
}

/**
 * Releases the window. The client has done its job and the window is still existing.
 */
void Client::releaseWindow(bool on_shutdown)
{
    assert(!deleting);
    deleting = true;
    destroyWindowManagementInterface();
    Deleted* del = NULL;
    if (!on_shutdown) {
        del = Deleted::create(this);
    }
    if (isMoveResize())
        emit clientFinishUserMovedResized(this);
    emit windowClosed(this, del);
    finishCompositing();
    RuleBook::self()->discardUsed(this, true);   // Remove ForceTemporarily rules
    StackingUpdatesBlocker blocker(workspace());
    if (isMoveResize())
        leaveMoveResize();
    finishWindowRules();
    blockGeometryUpdates();
    if (isOnCurrentDesktop() && isShown(true))
        addWorkspaceRepaint(visibleRect());
    // Grab X during the release to make removing of properties, setting to withdrawn state
    // and repareting to root an atomic operation (http://lists.kde.org/?l=kde-devel&m=116448102901184&w=2)
    grabXServer();
    exportMappingState(WithdrawnState);
    setModal(false);   // Otherwise its mainwindow wouldn't get focus
    hidden = true; // So that it's not considered visible anymore (can't use hideClient(), it would set flags)
    if (!on_shutdown)
        workspace()->clientHidden(this);
    m_frame.unmap();  // Destroying decoration would cause ugly visual effect
    destroyDecoration();
    cleanGrouping();
    if (!on_shutdown) {
        workspace()->removeClient(this);
        // Only when the window is being unmapped, not when closing down KWin (NETWM sections 5.5,5.7)
        info->setDesktop(0);
        info->setState(0, info->state());  // Reset all state flags
    } else
        untab();
    xcb_connection_t *c = connection();
    m_client.deleteProperty(atoms->kde_net_wm_user_creation_time);
    m_client.deleteProperty(atoms->net_frame_extents);
    m_client.deleteProperty(atoms->kde_net_wm_frame_strut);
    m_client.reparent(rootWindow(), x(), y());
    xcb_change_save_set(c, XCB_SET_MODE_DELETE, m_client);
    m_client.selectInput(XCB_EVENT_MASK_NO_EVENT);
    if (on_shutdown)
        // Map the window, so it can be found after another WM is started
        m_client.map();
    // TODO: Preserve minimized, shaded etc. state?
    else // Make sure it's not mapped if the app unmapped it (#65279). The app
        // may do map+unmap before we initially map the window by calling rawShow() from manage().
        m_client.unmap();
    m_client.reset();
    m_wrapper.reset();
    m_frame.reset();
    //frame = None;
    unblockGeometryUpdates(); // Don't use GeometryUpdatesBlocker, it would now set the geometry
    if (!on_shutdown) {
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteClient(this);
    ungrabXServer();
}

/**
 * Like releaseWindow(), but this one is called when the window has been already destroyed
 * (E.g. The application closed it)
 */
void Client::destroyClient()
{
    assert(!deleting);
    deleting = true;
    destroyWindowManagementInterface();
    Deleted* del = Deleted::create(this);
    if (isMoveResize())
        emit clientFinishUserMovedResized(this);
    emit windowClosed(this, del);
    finishCompositing(ReleaseReason::Destroyed);
    RuleBook::self()->discardUsed(this, true);   // Remove ForceTemporarily rules
    StackingUpdatesBlocker blocker(workspace());
    if (isMoveResize())
        leaveMoveResize();
    finishWindowRules();
    blockGeometryUpdates();
    if (isOnCurrentDesktop() && isShown(true))
        addWorkspaceRepaint(visibleRect());
    setModal(false);
    hidden = true; // So that it's not considered visible anymore
    workspace()->clientHidden(this);
    destroyDecoration();
    cleanGrouping();
    workspace()->removeClient(this);
    m_client.reset(); // invalidate
    m_wrapper.reset();
    m_frame.reset();
    //frame = None;
    unblockGeometryUpdates(); // Don't use GeometryUpdatesBlocker, it would now set the geometry
    disownDataPassedToDeleted();
    del->unrefWindow();
    deleteClient(this);
}

void Client::updateInputWindow()
{
    if (!Xcb::Extensions::self()->isShapeInputAvailable())
        return;

    QRegion region;

    if (!noBorder() && isDecorated()) {
        const QMargins &r = decoration()->resizeOnlyBorders();
        const int left   = r.left();
        const int top    = r.top();
        const int right  = r.right();
        const int bottom = r.bottom();
        if (left != 0 || top != 0 || right != 0 || bottom != 0) {
            region = QRegion(-left,
                             -top,
                             decoration()->size().width() + left + right,
                             decoration()->size().height() + top + bottom);
            region = region.subtracted(decoration()->rect());
        }
    }

    if (region.isEmpty()) {
        m_decoInputExtent.reset();
        return;
    }

    QRect bounds = region.boundingRect();
    input_offset = bounds.topLeft();

    // Move the bounding rect to screen coordinates
    bounds.translate(geometry().topLeft());

    // Move the region to input window coordinates
    region.translate(-input_offset);

    if (!m_decoInputExtent.isValid()) {
        const uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        const uint32_t values[] = {true,
            XCB_EVENT_MASK_ENTER_WINDOW   |
            XCB_EVENT_MASK_LEAVE_WINDOW   |
            XCB_EVENT_MASK_BUTTON_PRESS   |
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION
        };
        m_decoInputExtent.create(bounds, XCB_WINDOW_CLASS_INPUT_ONLY, mask, values);
        if (mapping_state == Mapped)
            m_decoInputExtent.map();
    } else {
        m_decoInputExtent.setGeometry(bounds);
    }

    const QVector<xcb_rectangle_t> rects = Xcb::regionToRects(region);
    xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED,
                         m_decoInputExtent, 0, 0, rects.count(), rects.constData());
}

void Client::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force &&
            ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder())))
        return;
    QRect oldgeom = geometry();
    QRect oldClientGeom = oldgeom.adjusted(borderLeft(), borderTop(), -borderRight(), -borderBottom());
    blockGeometryUpdates(true);
    if (force)
        destroyDecoration();
    if (!noBorder()) {
        createDecoration(oldgeom);
    } else
        destroyDecoration();
    getShadow();
    if (check_workspace_pos)
        checkWorkspacePosition(oldgeom, -2, oldClientGeom);
    updateInputWindow();
    blockGeometryUpdates(false);
    updateFrameExtents();
}

void Client::createDecoration(const QRect& oldgeom)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::getShadow);
        connect(decoration, &KDecoration2::Decoration::resizeOnlyBordersChanged, this, &Client::updateInputWindow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this,
            [this]() {
                updateFrameExtents();
                GeometryUpdatesBlocker blocker(this);
                // TODO: this is obviously idempotent
                // calculateGravitation(true) would have to operate on the old border sizes
//                 move(calculateGravitation(true));
//                 move(calculateGravitation(false));
                QRect oldgeom = geometry();
                plainResize(sizeForClientSize(clientSize()), ForceGeometrySet);
                if (!isShade())
                    checkWorkspacePosition(oldgeom);
                emit geometryShapeChanged(this, oldgeom);
            }
        );
        connect(decoratedClient()->decoratedClient(), &KDecoration2::DecoratedClient::widthChanged, this, &Client::updateInputWindow);
        connect(decoratedClient()->decoratedClient(), &KDecoration2::DecoratedClient::heightChanged, this, &Client::updateInputWindow);
    }
    setDecoration(decoration);

    move(calculateGravitation(false));
    plainResize(sizeForClientSize(clientSize()), ForceGeometrySet);
    if (Compositor::compositing()) {
        discardWindowPixmap();
    }
    emit geometryShapeChanged(this, oldgeom);
}

void Client::destroyDecoration()
{
    QRect oldgeom = geometry();
    if (isDecorated()) {
        QPoint grav = calculateGravitation(true);
        AbstractClient::destroyDecoration();
        plainResize(sizeForClientSize(clientSize()), ForceGeometrySet);
        move(grav);
        if (compositing())
            discardWindowPixmap();
        if (!deleting) {
            emit geometryShapeChanged(this, oldgeom);
        }
    }
    m_decoInputExtent.reset();
}

void Client::layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom) const
{
    if (!isDecorated()) {
        return;
    }
    QRect r = decoration()->rect();

    NETStrut strut = info->frameOverlap();

    // Ignore the overlap strut when compositing is disabled
    if (!compositing())
        strut.left = strut.top = strut.right = strut.bottom = 0;
    else if (strut.left == -1 && strut.top == -1 && strut.right == -1 && strut.bottom == -1) {
        top = QRect(r.x(), r.y(), r.width(), r.height() / 3);
        left = QRect(r.x(), r.y() + top.height(), width() / 2, r.height() / 3);
        right = QRect(r.x() + left.width(), r.y() + top.height(), r.width() - left.width(), left.height());
        bottom = QRect(r.x(), r.y() + top.height() + left.height(), r.width(), r.height() - left.height() - top.height());
        return;
    }

    top = QRect(r.x(), r.y(), r.width(), borderTop() + strut.top);
    bottom = QRect(r.x(), r.y() + r.height() - borderBottom() - strut.bottom,
                   r.width(), borderBottom() + strut.bottom);
    left = QRect(r.x(), r.y() + top.height(),
                 borderLeft() + strut.left, r.height() - top.height() - bottom.height());
    right = QRect(r.x() + r.width() - borderRight() - strut.right, r.y() + top.height(),
                  borderRight() + strut.right, r.height() - top.height() - bottom.height());
}

QRect Client::transparentRect() const
{
    if (isShade())
        return QRect();

    NETStrut strut = info->frameOverlap();
    // Ignore the strut when compositing is disabled or the decoration doesn't support it
    if (!compositing())
        strut.left = strut.top = strut.right = strut.bottom = 0;
    else if (strut.left == -1 && strut.top == -1 && strut.right == -1 && strut.bottom == -1)
        return QRect();

    const QRect r = QRect(clientPos(), clientSize())
                    .adjusted(strut.left, strut.top, -strut.right, -strut.bottom);
    if (r.isValid())
        return r;

    return QRect();
}

void Client::detectNoBorder()
{
    if (shape()) {
        noborder = true;
        app_noborder = true;
        return;
    }
    switch(windowType()) {
    case NET::Desktop :
    case NET::Dock :
    case NET::TopMenu :
    case NET::Splash :
    case NET::Notification :
    case NET::OnScreenDisplay :
        noborder = true;
        app_noborder = true;
        break;
    case NET::Unknown :
    case NET::Normal :
    case NET::Toolbar :
    case NET::Menu :
    case NET::Dialog :
    case NET::Utility :
        noborder = false;
        break;
    default:
        abort();
    }
    // NET::Override is some strange beast without clear definition, usually
    // just meaning "noborder", so let's treat it only as such flag, and ignore it as
    // a window type otherwise (SUPPORTED_WINDOW_TYPES_MASK doesn't include it)
    if (info->windowType(NET::OverrideMask) == NET::Override) {
        noborder = true;
        app_noborder = true;
    }
}

void Client::updateFrameExtents()
{
    NETStrut strut;
    strut.left = borderLeft();
    strut.right = borderRight();
    strut.top = borderTop();
    strut.bottom = borderBottom();
    info->setFrameExtents(strut);
}

Xcb::Property Client::fetchGtkFrameExtents() const
{
    return Xcb::Property(false, m_client, atoms->gtk_frame_extents, XCB_ATOM_CARDINAL, 0, 4);
}

void Client::readGtkFrameExtents(Xcb::Property &prop)
{
    m_clientSideDecorated = !prop.isNull() && prop->type != 0;
    emit clientSideDecoratedChanged();
}

void Client::detectGtkFrameExtents()
{
    Xcb::Property prop = fetchGtkFrameExtents();
    readGtkFrameExtents(prop);
}

/**
 * Resizes the decoration, and makes sure the decoration widget gets resize event
 * even if the size hasn't changed. This is needed to make sure the decoration
 * re-layouts (e.g. when maximization state changes,
 * the decoration may alter some borders, but the actual size
 * of the decoration stays the same).
 */
void Client::resizeDecoration()
{
    triggerDecorationRepaint();
    updateInputWindow();
}

bool Client::noBorder() const
{
    return noborder || isFullScreen();
}

bool Client::userCanSetNoBorder() const
{
    return !isFullScreen() && !isShade() && !tabGroup();
}

void Client::setNoBorder(bool set)
{
    if (!userCanSetNoBorder())
        return;
    set = rules()->checkNoBorder(set);
    if (noborder == set)
        return;
    noborder = set;
    updateDecoration(true, false);
    updateWindowRules(Rules::NoBorder);
}

void Client::checkNoBorder()
{
    setNoBorder(app_noborder);
}

bool Client::wantsShadowToBeRendered() const
{
    return !isFullScreen() && maximizeMode() != MaximizeFull;
}

void Client::updateShape()
{
    if (shape()) {
        // Workaround for #19644 - Shaped windows shouldn't have decoration
        if (!app_noborder) {
            // Only when shape is detected for the first time, still let the user to override
            app_noborder = true;
            noborder = rules()->checkNoBorder(true);
            updateDecoration(true);
        }
        if (noBorder()) {
            xcb_shape_combine(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, XCB_SHAPE_SK_BOUNDING,
                              frameId(), clientPos().x(), clientPos().y(), window());
        }
    } else if (app_noborder) {
        xcb_shape_mask(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, frameId(), 0, 0, XCB_PIXMAP_NONE);
        detectNoBorder();
        app_noborder = noborder;
        noborder = rules()->checkNoBorder(noborder || m_motif.noBorder());
        updateDecoration(true);
    }

    // Decoration mask (i.e. 'else' here) setting is done in setMask()
    // when the decoration calls it or when the decoration is created/destroyed
    updateInputShape();
    if (compositing()) {
        addRepaintFull();
        addWorkspaceRepaint(visibleRect());   // In case shape change removes part of this window
    }
    emit geometryShapeChanged(this, geometry());
}

static Xcb::Window shape_helper_window(XCB_WINDOW_NONE);

void Client::cleanupX11()
{
    shape_helper_window.reset();
}

void Client::updateInputShape()
{
    if (hiddenPreview())   // Sets it to none, don't change
        return;
    if (Xcb::Extensions::self()->isShapeInputAvailable()) {
        // There appears to be no way to find out if a window has input
        // shape set or not, so always propagate the input shape
        // (it's the same like the bounding shape by default).
        // Also, build the shape using a helper window, not directly
        // in the frame window, because the sequence set-shape-to-frame,
        // remove-shape-of-client, add-input-shape-of-client has the problem
        // that after the second step there's a hole in the input shape
        // until the real shape of the client is added and that can make
        // the window lose focus (which is a problem with mouse focus policies)
        // TODO: It seems there is, after all - XShapeGetRectangles() - but maybe this is better
        if (!shape_helper_window.isValid())
            shape_helper_window.create(QRect(0, 0, 1, 1));
        shape_helper_window.resize(width(), height());
        xcb_connection_t *c = connection();
        xcb_shape_combine(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_SHAPE_SK_BOUNDING,
                          shape_helper_window, 0, 0, frameId());
        xcb_shape_combine(c, XCB_SHAPE_SO_SUBTRACT, XCB_SHAPE_SK_INPUT, XCB_SHAPE_SK_BOUNDING,
                          shape_helper_window, clientPos().x(), clientPos().y(), window());
        xcb_shape_combine(c, XCB_SHAPE_SO_UNION, XCB_SHAPE_SK_INPUT, XCB_SHAPE_SK_INPUT,
                          shape_helper_window, clientPos().x(), clientPos().y(), window());
        xcb_shape_combine(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_SHAPE_SK_INPUT,
                          frameId(), 0, 0, shape_helper_window);
    }
}

void Client::hideClient(bool hide)
{
    if (hidden == hide)
        return;
    hidden = hide;
    updateVisibility();
}

/**
 * Returns whether the window is minimizable or not
 */
bool Client::isMinimizable() const
{
    if (isSpecialWindow() && !isTransient())
        return false;
    if (!rules()->checkMinimize(true))
        return false;

    if (isTransient()) {
        // #66868 - Let other xmms windows be minimized when the mainwindow is minimized
        bool shown_mainwindow = false;
        auto mainclients = mainClients();
        for (auto it = mainclients.constBegin();
                it != mainclients.constEnd();
                ++it)
            if ((*it)->isShown(true))
                shown_mainwindow = true;
        if (!shown_mainwindow)
            return true;
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
    if (!wantsTabFocus())   // SELI, TODO: - NET::Utility? why wantsTabFocus() - skiptaskbar? ?
        return false;
    return true;
}

void Client::doMinimize()
{
    updateVisibility();
    updateAllowedActions();
    workspace()->updateMinimizedOfTransients(this);
    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Minimized);
}

QRect Client::iconGeometry() const
{
    NETRect r = info->iconGeometry();
    QRect geom(r.pos.x, r.pos.y, r.size.width, r.size.height);
    if (geom.isValid())
        return geom;
    else {
        // Check all mainwindows of this window (recursively)
        foreach (AbstractClient * amainwin, mainClients()) {
            Client *mainwin = dynamic_cast<Client*>(amainwin);
            if (!mainwin) {
                continue;
            }
            geom = mainwin->iconGeometry();
            if (geom.isValid())
                return geom;
        }
        // No mainwindow (or their parents) with icon geometry was found
        return AbstractClient::iconGeometry();
    }
}

bool Client::isShadeable() const
{
    return !isSpecialWindow() && !noBorder() && (rules()->checkShade(ShadeNormal) != rules()->checkShade(ShadeNone));
}

void Client::setShade(ShadeMode mode)
{
    if (mode == ShadeHover && isMove())
        return; // causes geometry breaks and is probably nasty
    if (isSpecialWindow() || noBorder())
        mode = ShadeNone;
    mode = rules()->checkShade(mode);
    if (shade_mode == mode)
        return;
    bool was_shade = isShade();
    ShadeMode was_shade_mode = shade_mode;
    shade_mode = mode;

    // Decorations may turn off some borders when shaded
    // this has to happen _before_ the tab alignment since it will restrict the minimum geometry
#if 0
    if (decoration)
        decoration->borders(border_left, border_right, border_top, border_bottom);
#endif

    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Shaded);

    if (was_shade == isShade()) {
        // Decoration may want to update after e.g. hover-shade changes
        emit shadeChanged();
        return; // No real change in shaded state
    }

    assert(isDecorated());   // noborder windows can't be shaded
    GeometryUpdatesBlocker blocker(this);

    // TODO: All this unmapping, resizing etc. feels too much duplicated from elsewhere
    if (isShade()) {
        // shade_mode == ShadeNormal
        addWorkspaceRepaint(visibleRect());
        // Shade
        shade_geometry_change = true;
        QSize s(sizeForClientSize(QSize(clientSize())));
        s.setHeight(borderTop() + borderBottom());
        m_wrapper.selectInput(ClientWinMask);   // Avoid getting UnmapNotify
        m_wrapper.unmap();
        m_client.unmap();
        m_wrapper.selectInput(ClientWinMask | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
        exportMappingState(IconicState);
        plainResize(s);
        shade_geometry_change = false;
        if (was_shade_mode == ShadeHover) {
            if (shade_below && workspace()->stackingOrder().indexOf(shade_below) > -1)
                    workspace()->restack(this, shade_below, true);
            if (isActive())
                workspace()->activateNextClient(this);
        } else if (isActive()) {
            workspace()->focusToNull();
        }
    } else {
        shade_geometry_change = true;
        if (decoratedClient())
            decoratedClient()->signalShadeChange();
        QSize s(sizeForClientSize(clientSize()));
        shade_geometry_change = false;
        plainResize(s);
        geom_restore = geometry();
        if ((shade_mode == ShadeHover || shade_mode == ShadeActivated) && rules()->checkAcceptFocus(info->input()))
            setActive(true);
        if (shade_mode == ShadeHover) {
            ToplevelList order = workspace()->stackingOrder();
            // invalidate, since "this" could be the topmost toplevel and shade_below dangeling
            shade_below = NULL;
            // this is likely related to the index parameter?!
            for (int idx = order.indexOf(this) + 1; idx < order.count(); ++idx) {
                shade_below = qobject_cast<Client*>(order.at(idx));
                if (shade_below) {
                    break;
                }
            }
            if (shade_below && shade_below->isNormalWindow())
                workspace()->raiseClient(this);
            else
                shade_below = NULL;
        }
        m_wrapper.map();
        m_client.map();
        exportMappingState(NormalState);
        if (isActive())
            workspace()->requestFocus(this);
    }
    info->setState(isShade() ? NET::Shaded : NET::States(0), NET::Shaded);
    info->setState(isShown(false) ? NET::States(0) : NET::Hidden, NET::Hidden);
    discardWindowPixmap();
    updateVisibility();
    updateAllowedActions();
    updateWindowRules(Rules::Shade);

    emit shadeChanged();
}

void Client::shadeHover()
{
    setShade(ShadeHover);
    cancelShadeHoverTimer();
}

void Client::shadeUnhover()
{
    if (!tabGroup() || tabGroup()->current() == this ||
        tabGroup()->current()->shadeMode() == ShadeNormal)
        setShade(ShadeNormal);
    cancelShadeHoverTimer();
}

void Client::cancelShadeHoverTimer()
{
    delete shadeHoverTimer;
    shadeHoverTimer = 0;
}

void Client::toggleShade()
{
    // If the mode is ShadeHover or ShadeActive, cancel shade too
    setShade(shade_mode == ShadeNone ? ShadeNormal : ShadeNone);
}

void Client::updateVisibility()
{
    if (deleting)
        return;
    if (hidden && isCurrentTab()) {
        info->setState(NET::Hidden, NET::Hidden);
        setSkipTaskbar(true);   // Also hide from taskbar
        if (compositing() && options->hiddenPreviews() == HiddenPreviewsAlways)
            internalKeep();
        else
            internalHide();
        return;
    }
    if (isCurrentTab())
        setSkipTaskbar(originalSkipTaskbar());   // Reset from 'hidden'
    if (isMinimized()) {
        info->setState(NET::Hidden, NET::Hidden);
        if (compositing() && options->hiddenPreviews() == HiddenPreviewsAlways)
            internalKeep();
        else
            internalHide();
        return;
    }
    info->setState(0, NET::Hidden);
    if (!isOnCurrentDesktop()) {
        if (compositing() && options->hiddenPreviews() != HiddenPreviewsNever)
            internalKeep();
        else
            internalHide();
        return;
    }
    if (!isOnCurrentActivity()) {
        if (compositing() && options->hiddenPreviews() != HiddenPreviewsNever)
            internalKeep();
        else
            internalHide();
        return;
    }
    internalShow();
}


/**
 * Sets the client window's mapping state. Possible values are
 * WithdrawnState, IconicState, NormalState.
 */
void Client::exportMappingState(int s)
{
    assert(m_client != XCB_WINDOW_NONE);
    assert(!deleting || s == WithdrawnState);
    if (s == WithdrawnState) {
        m_client.deleteProperty(atoms->wm_state);
        return;
    }
    assert(s == NormalState || s == IconicState);

    int32_t data[2];
    data[0] = s;
    data[1] = XCB_NONE;
    m_client.changeProperty(atoms->wm_state, atoms->wm_state, 32, 2, data);
}

void Client::internalShow()
{
    if (mapping_state == Mapped)
        return;
    MappingState old = mapping_state;
    mapping_state = Mapped;
    if (old == Unmapped || old == Withdrawn)
        map();
    if (old == Kept) {
        m_decoInputExtent.map();
        updateHiddenPreview();
    }
    emit windowShown(this);
}

void Client::internalHide()
{
    if (mapping_state == Unmapped)
        return;
    MappingState old = mapping_state;
    mapping_state = Unmapped;
    if (old == Mapped || old == Kept)
        unmap();
    if (old == Kept)
        updateHiddenPreview();
    addWorkspaceRepaint(visibleRect());
    workspace()->clientHidden(this);
    emit windowHidden(this);
}

void Client::internalKeep()
{
    assert(compositing());
    if (mapping_state == Kept)
        return;
    MappingState old = mapping_state;
    mapping_state = Kept;
    if (old == Unmapped || old == Withdrawn)
        map();
    m_decoInputExtent.unmap();
    if (isActive())
        workspace()->focusToNull(); // get rid of input focus, bug #317484
    updateHiddenPreview();
    addWorkspaceRepaint(visibleRect());
    workspace()->clientHidden(this);
}

/**
 * Maps (shows) the client. Note that it is mapping state of the frame,
 * not necessarily the client window itself (i.e. a shaded window is here
 * considered mapped, even though it is in IconicState).
 */
void Client::map()
{
    // XComposite invalidates backing pixmaps on unmap (minimize, different
    // virtual desktop, etc.).  We kept the last known good pixmap around
    // for use in effects, but now we want to have access to the new pixmap
    if (compositing())
        discardWindowPixmap();
    m_frame.map();
    if (!isShade()) {
        m_wrapper.map();
        m_client.map();
        m_decoInputExtent.map();
        exportMappingState(NormalState);
    } else
        exportMappingState(IconicState);
    addLayerRepaint(visibleRect());
}

/**
 * Unmaps the client. Again, this is about the frame.
 */
void Client::unmap()
{
    // Here it may look like a race condition, as some other client might try to unmap
    // the window between these two XSelectInput() calls. However, they're supposed to
    // use XWithdrawWindow(), which also sends a synthetic event to the root window,
    // which won't be missed, so this shouldn't be a problem. The chance the real UnmapNotify
    // will be missed is also very minimal, so I don't think it's needed to grab the server
    // here.
    m_wrapper.selectInput(ClientWinMask);   // Avoid getting UnmapNotify
    m_frame.unmap();
    m_wrapper.unmap();
    m_client.unmap();
    m_decoInputExtent.unmap();
    m_wrapper.selectInput(ClientWinMask | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
    exportMappingState(IconicState);
}

/**
 * XComposite doesn't keep window pixmaps of unmapped windows, which means
 * there wouldn't be any previews of windows that are minimized or on another
 * virtual desktop. Therefore rawHide() actually keeps such windows mapped.
 * However special care needs to be taken so that such windows don't interfere.
 * Therefore they're put very low in the stacking order and they have input shape
 * set to none, which hopefully is enough. If there's no input shape available,
 * then it's hoped that there will be some other desktop above it *shrug*.
 * Using normal shape would be better, but that'd affect other things, e.g. painting
 * of the actual preview.
 */
void Client::updateHiddenPreview()
{
    if (hiddenPreview()) {
        workspace()->forceRestacking();
        if (Xcb::Extensions::self()->isShapeInputAvailable()) {
            xcb_shape_rectangles(connection(), XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT,
                                 XCB_CLIP_ORDERING_UNSORTED, frameId(), 0, 0, 0, NULL);
        }
    } else {
        workspace()->forceRestacking();
        updateInputShape();
    }
}

void Client::sendClientMessage(xcb_window_t w, xcb_atom_t a, xcb_atom_t protocol, uint32_t data1, uint32_t data2, uint32_t data3, xcb_timestamp_t timestamp)
{
    xcb_client_message_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = w;
    ev.type = a;
    ev.format = 32;
    ev.data.data32[0] = protocol;
    ev.data.data32[1] = timestamp;
    ev.data.data32[2] = data1;
    ev.data.data32[3] = data2;
    ev.data.data32[4] = data3;
    uint32_t eventMask = 0;
    if (w == rootWindow()) {
        eventMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT; // Magic!
    }
    xcb_send_event(connection(), false, w, eventMask, reinterpret_cast<const char*>(&ev));
    xcb_flush(connection());
}

/**
 * Returns whether the window may be closed (have a close button)
 */
bool Client::isCloseable() const
{
    return rules()->checkCloseable(m_motif.close() && !isSpecialWindow());
}

/**
 * Closes the window by either sending a delete_window message or using XKill.
 */
void Client::closeWindow()
{
    if (!isCloseable())
        return;

    // Update user time, because the window may create a confirming dialog.
    updateUserTime();

    if (info->supportsProtocol(NET::DeleteWindowProtocol)) {
        sendClientMessage(window(), atoms->wm_protocols, atoms->wm_delete_window);
        pingWindow();
    } else // Client will not react on wm_delete_window. We have not choice
        // but destroy his connection to the XServer.
        killWindow();
}


/**
 * Kills the window via XKill
 */
void Client::killWindow()
{
    qCDebug(KWIN_CORE) << "Client::killWindow():" << caption();
    killProcess(false);
    m_client.kill();  // Always kill this client at the server
    destroyClient();
}

/**
 * Send a ping to the window using _NET_WM_PING if possible if it
 * doesn't respond within a reasonable time, it will be killed.
 */
void Client::pingWindow()
{
    if (!info->supportsProtocol(NET::PingProtocol))
        return; // Can't ping :(
    if (options->killPingTimeout() == 0)
        return; // Turned off
    if (ping_timer != NULL)
        return; // Pinging already
    ping_timer = new QTimer(this);
    connect(ping_timer, &QTimer::timeout, this,
        [this]() {
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
        }
    );
    ping_timer->setSingleShot(true);
    // we'll run the timer twice, at first we'll desaturate the window
    // and the second time we'll show the "do you want to kill" prompt
    ping_timer->start(options->killPingTimeout() / 2);
    m_pingTimestamp = xTime();
    workspace()->sendPingToWindow(window(), m_pingTimestamp);
}

void Client::gotPing(xcb_timestamp_t timestamp)
{
    // Just plain compare is not good enough because of 64bit and truncating and whatnot
    if (NET::timestampCompare(timestamp, m_pingTimestamp) != 0)
        return;
    delete ping_timer;
    ping_timer = NULL;

    setUnresponsive(false);

    if (m_killHelperPID && !::kill(m_killHelperPID, 0)) { // means the process is alive
        ::kill(m_killHelperPID, SIGTERM);
        m_killHelperPID = 0;
    }
}

void Client::killProcess(bool ask, xcb_timestamp_t timestamp)
{
    if (m_killHelperPID && !::kill(m_killHelperPID, 0)) // means the process is alive
        return;
    Q_ASSERT(!ask || timestamp != XCB_TIME_CURRENT_TIME);
    pid_t pid = info->pid();
    if (pid <= 0 || clientMachine()->hostName().isEmpty())  // Needed properties missing
        return;
    qCDebug(KWIN_CORE) << "Kill process:" << pid << "(" << clientMachine()->hostName() << ")";
    if (!ask) {
        if (!clientMachine()->isLocal()) {
            QStringList lst;
            lst << QString::fromUtf8(clientMachine()->hostName()) << QStringLiteral("kill") << QString::number(pid);
            QProcess::startDetached(QStringLiteral("xon"), lst);
        } else
            ::kill(pid, SIGTERM);
    } else {
        QString hostname = clientMachine()->isLocal() ? QStringLiteral("localhost") : QString::fromUtf8(clientMachine()->hostName());
        QProcess::startDetached(QStringLiteral(KWIN_KILLER_BIN),
                                QStringList() << QStringLiteral("--pid") << QString::number(unsigned(pid)) << QStringLiteral("--hostname") << hostname
                                << QStringLiteral("--windowname") << captionNormal()
                                << QStringLiteral("--applicationname") << QString::fromUtf8(resourceClass())
                                << QStringLiteral("--wid") << QString::number(window())
                                << QStringLiteral("--timestamp") << QString::number(timestamp),
                                QString(), &m_killHelperPID);
    }
}

void Client::doSetSkipTaskbar()
{
    info->setState(skipTaskbar() ? NET::SkipTaskbar : NET::States(0), NET::SkipTaskbar);
}

void Client::doSetSkipPager()
{
    info->setState(skipPager() ? NET::SkipPager : NET::States(0), NET::SkipPager);
}

void Client::doSetDesktop(int desktop, int was_desk)
{
    Q_UNUSED(desktop)
    Q_UNUSED(was_desk)
    updateVisibility();

    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Desktop);
}

/**
 * Sets whether the client is on @p activity.
 * If you remove it from its last activity, then it's on all activities.
 *
 * Note: If it was on all activities and you try to remove it from one, nothing will happen;
 * I don't think that's an important enough use case to handle here.
 */
void Client::setOnActivity(const QString &activity, bool enable)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (! Activities::self()) {
        return;
    }
    QStringList newActivitiesList = activities();
    if (newActivitiesList.contains(activity) == enable)   //nothing to do
        return;
    if (enable) {
        QStringList allActivities = Activities::self()->all();
        if (!allActivities.contains(activity))   //bogus ID
            return;
        newActivitiesList.append(activity);
    } else
        newActivitiesList.removeOne(activity);
    setOnActivities(newActivitiesList);
#else
    Q_UNUSED(activity)
    Q_UNUSED(enable)
#endif
}

/**
 * set exactly which activities this client is on
 */
void Client::setOnActivities(QStringList newActivitiesList)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    QString joinedActivitiesList = newActivitiesList.join(QStringLiteral(","));
    joinedActivitiesList = rules()->checkActivity(joinedActivitiesList, false);
    newActivitiesList = joinedActivitiesList.split(u',', QString::SkipEmptyParts);

    QStringList allActivities = Activities::self()->all();

    auto it = newActivitiesList.begin();
    while (it != newActivitiesList.end()) {
        if (! allActivities.contains(*it)) {
            it = newActivitiesList.erase(it);
        } else {
            it++;
        }
    }

    if (// If we got the request to be on all activities explicitly
        newActivitiesList.isEmpty() || joinedActivitiesList == Activities::nullUuid() ||
        // If we got a list of activities that covers all activities
        (newActivitiesList.count() > 1 && newActivitiesList.count() == allActivities.count())) {

        activityList.clear();
        const QByteArray nullUuid = Activities::nullUuid().toUtf8();
        m_client.changeProperty(atoms->activities, XCB_ATOM_STRING, 8, nullUuid.length(), nullUuid.constData());

    } else {
        QByteArray joined = joinedActivitiesList.toAscii();
        activityList = newActivitiesList;
        m_client.changeProperty(atoms->activities, XCB_ATOM_STRING, 8, joined.length(), joined.constData());
    }

    updateActivities(false);
#else
    Q_UNUSED(newActivitiesList)
#endif
}

void Client::blockActivityUpdates(bool b)
{
    if (b) {
        ++m_activityUpdatesBlocked;
    } else {
        Q_ASSERT(m_activityUpdatesBlocked);
        --m_activityUpdatesBlocked;
        if (!m_activityUpdatesBlocked)
            updateActivities(m_blockedActivityUpdatesRequireTransients);
    }
}

/**
 * update after activities changed
 */
void Client::updateActivities(bool includeTransients)
{
    if (m_activityUpdatesBlocked) {
        m_blockedActivityUpdatesRequireTransients |= includeTransients;
        return;
    }
    emit activitiesChanged(this);
    m_blockedActivityUpdatesRequireTransients = false; // reset
    FocusChain::self()->update(this, FocusChain::MakeFirst);
    updateVisibility();
    updateWindowRules(Rules::Activity);

    // Update states of all other windows in this group
    if (tabGroup())
        tabGroup()->updateStates(this, TabGroup::Activity);
}

/**
 * Returns the list of activities the client window is on.
 * if it's on all activities, the list will be empty.
 * Don't use this, use isOnActivity() and friends (from class Toplevel)
 */
QStringList Client::activities() const
{
    if (sessionActivityOverride) {
        return QStringList();
    }
    return activityList;
}

/**
 * if @p on is true, sets on all activities.
 * if it's false, sets it to only be on the current activity
 */
void Client::setOnAllActivities(bool on)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (on == isOnAllActivities())
        return;
    if (on) {
        setOnActivities(QStringList());

    } else {
        setOnActivity(Activities::self()->current(), true);
    }
#else
    Q_UNUSED(on)
#endif
}

/**
 * Performs the actual focusing of the window using XSetInputFocus and WM_TAKE_FOCUS
 */
void Client::takeFocus()
{
    if (rules()->checkAcceptFocus(info->input()))
        m_client.focus();
    else
        demandAttention(false); // window cannot take input, at least withdraw urgency
    if (info->supportsProtocol(NET::TakeFocusProtocol)) {
        sendClientMessage(window(), atoms->wm_protocols, atoms->wm_take_focus, 0, 0, 0, XCB_CURRENT_TIME);
    }
    workspace()->setShouldGetFocus(this);

    bool breakShowingDesktop = !keepAbove();
    if (breakShowingDesktop) {
        foreach (const Client *c, group()->members()) {
            if (c->isDesktop()) {
                breakShowingDesktop = false;
                break;
            }
        }
    }
    if (breakShowingDesktop)
        workspace()->setShowingDesktop(false);
}

/**
 * Returns whether the window provides context help or not. If it does,
 * you should show a help menu item or a help button like '?' and call
 * contextHelp() if this is invoked.
 *
 * \sa contextHelp()
 */
bool Client::providesContextHelp() const
{
    return info->supportsProtocol(NET::ContextHelpProtocol);
}

/**
 * Invokes context help on the window. Only works if the window
 * actually provides context help.
 *
 * \sa providesContextHelp()
 */
void Client::showContextHelp()
{
    if (info->supportsProtocol(NET::ContextHelpProtocol)) {
        sendClientMessage(window(), atoms->wm_protocols, atoms->net_wm_context_help);
    }
}

/**
 * Fetches the window's caption (WM_NAME property). It will be
 * stored in the client's caption().
 */
void Client::fetchName()
{
    setCaption(readName());
}

static inline QString readNameProperty(xcb_window_t w, xcb_atom_t atom)
{
    const auto cookie = xcb_icccm_get_text_property_unchecked(connection(), w, atom);
    xcb_icccm_get_text_property_reply_t reply;
    if (xcb_icccm_get_wm_name_reply(connection(), cookie, &reply, nullptr)) {
        QString retVal;
        if (reply.encoding == atoms->utf8_string) {
            retVal = QString::fromUtf8(QByteArray(reply.name, reply.name_len));
        } else if (reply.encoding == XCB_ATOM_STRING) {
            retVal = QString::fromLocal8Bit(QByteArray(reply.name, reply.name_len));
        }
        xcb_icccm_get_text_property_reply_wipe(&reply);
        return retVal.simplified();
    }
    return QString();
}

QString Client::readName() const
{
    if (info->name() && info->name()[0] != '\0')
        return QString::fromUtf8(info->name()).simplified();
    else {
        return readNameProperty(window(), XCB_ATOM_WM_NAME);
    }
}

// The list is taken from http://www.unicode.org/reports/tr9/ (#154840)
static const QChar LRM(0x200E);

void Client::setCaption(const QString& _s, bool force)
{
    if (!force && _s == cap_normal)
        return;
    QString s(_s);
    for (int i = 0; i < s.length(); ++i)
        if (!s[i].isPrint())
            s[i] = QChar(u' ');
    const bool changed = (s != cap_normal);
    cap_normal = s;
    if (!force && !changed) {
        emit captionChanged();
        return;
    }

    bool reset_name = force;
    bool was_suffix = (!cap_suffix.isEmpty());
    cap_suffix.clear();
    QString machine_suffix;
    if (!options->condensedTitle()) { // machine doesn't qualify for "clean"
        if (clientMachine()->hostName() != ClientMachine::localhost() && !clientMachine()->isLocal())
            machine_suffix = QLatin1String(" <@") + QString::fromUtf8(clientMachine()->hostName()) + QLatin1Char('>') + LRM;
    }
    QString shortcut_suffix = shortcutCaptionSuffix();
    cap_suffix = machine_suffix + shortcut_suffix;
    if ((!isSpecialWindow() || isToolbar()) && findClientWithSameCaption()) {
        int i = 2;
        do {
            cap_suffix = machine_suffix + QLatin1String(" <") + QString::number(i) + QLatin1Char('>') + LRM;
            i++;
        } while (findClientWithSameCaption());
        info->setVisibleName(caption().toUtf8().constData());
        reset_name = false;
    }
    if ((was_suffix && cap_suffix.isEmpty()) || reset_name) {
        // If it was new window, it may have old value still set, if the window is reused
        info->setVisibleName("");
        info->setVisibleIconName("");
    } else if (!cap_suffix.isEmpty() && !cap_iconic.isEmpty())
        // Keep the same suffix in iconic name if it's set
        info->setVisibleIconName(QString(cap_iconic + cap_suffix).toUtf8().constData());

    emit captionChanged();
}

void Client::updateCaption()
{
    setCaption(cap_normal, true);
}

void Client::fetchIconicName()
{
    QString s;
    if (info->iconName() && info->iconName()[0] != '\0')
        s = QString::fromUtf8(info->iconName());
    else
        s = readNameProperty(window(), XCB_ATOM_WM_ICON_NAME);
    if (s != cap_iconic) {
        bool was_set = !cap_iconic.isEmpty();
        cap_iconic = s;
        if (!cap_suffix.isEmpty()) {
            if (!cap_iconic.isEmpty())  // Keep the same suffix in iconic name if it's set
                info->setVisibleIconName(QString(s + cap_suffix).toUtf8().constData());
            else if (was_set)
                info->setVisibleIconName("");
        }
    }
}

bool Client::tabTo(Client *other, bool behind, bool activate)
{
    Q_ASSERT(other && other != this);

    if (tab_group && tab_group == other->tabGroup()) { // special case: move inside group
        tab_group->move(this, other, behind);
        return true;
    }

    GeometryUpdatesBlocker blocker(this);
    const bool wasBlocking = signalsBlocked();
    blockSignals(true); // prevent client emitting "retabbed to nowhere" cause it's about to be entabbed the next moment
    untab();
    blockSignals(wasBlocking);

    TabGroup *newGroup = other->tabGroup() ? other->tabGroup() : new TabGroup(other);

    if (!newGroup->add(this, other, behind, activate)) {
        if (newGroup->count() < 2) { // adding "c" to "to" failed for whatever reason
            newGroup->remove(other);
            delete newGroup;
        }
        return false;
    }
    return true;
}

bool Client::untab(const QRect &toGeometry, bool clientRemoved)
{
    TabGroup *group = tab_group;
    if (group && group->remove(this)) { // remove sets the tabgroup to "0", therefore the pointer is cached
        if (group->isEmpty()) {
            delete group;
        }
        if (clientRemoved)
            return true; // there's been a broadcast signal that this client is now removed - don't touch it
        setClientShown(!(isMinimized() || isShade()));
        bool keepSize = toGeometry.size() == size();
        bool changedSize = false;
        if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
            changedSize = true;
            setQuickTileMode(QuickTileFlag::None); // if we leave a quicktiled group, assume that the user wants to untile
        }
        if (toGeometry.isValid()) {
            if (maximizeMode() != MaximizeRestore) {
                changedSize = true;
                maximize(MaximizeRestore); // explicitly calling for a geometry -> unmaximize
            }
            if (keepSize && changedSize) {
                geom_restore = geometry(); // checkWorkspacePosition() invokes it
                QPoint cpoint = Cursor::pos();
                QPoint point = cpoint;
                point.setX((point.x() - toGeometry.x()) * geom_restore.width() / toGeometry.width());
                point.setY((point.y() - toGeometry.y()) * geom_restore.height() / toGeometry.height());
                geom_restore.moveTo(cpoint-point);
            } else {
                geom_restore = toGeometry; // checkWorkspacePosition() invokes it
            }
            setGeometry(geom_restore);
            checkWorkspacePosition();
        }
        return true;
    }
    return false;
}

void Client::setTabGroup(TabGroup *group)
{
    tab_group = group;
    if (group) {
        unsigned long data[] = {qHash(group)}; //->id();
        m_client.changeProperty(atoms->kde_net_wm_tab_group, XCB_ATOM_CARDINAL, 32, 1, data);
    }
    else
        m_client.deleteProperty(atoms->kde_net_wm_tab_group);
    emit tabGroupChanged();
}

bool Client::isCurrentTab() const
{
    return !tab_group || tab_group->current() == this;
}

void Client::syncTabGroupFor(QString property, bool fromThisClient)
{
    if (tab_group)
        tab_group->sync(property.toAscii().data(), fromThisClient ? this : tab_group->current());
}

void Client::setClientShown(bool shown)
{
    if (deleting)
        return; // Don't change shown status if this client is being deleted
    if (shown != hidden)
        return; // nothing to change
    hidden = !shown;
    if (options->isInactiveTabsSkipTaskbar())
        setSkipTaskbar(hidden); // TODO: Causes reshuffle of the taskbar
    if (shown) {
        map();
        takeFocus();
        autoRaise();
        FocusChain::self()->update(this, FocusChain::MakeFirst);
    } else {
        unmap();
        // Don't move tabs to the end of the list when another tab get's activated
        if (isCurrentTab())
            FocusChain::self()->update(this, FocusChain::MakeLast);
        addWorkspaceRepaint(visibleRect());
    }
}

void Client::getMotifHints()
{
    const bool wasClosable = m_motif.close();
    const bool wasNoBorder = m_motif.noBorder();
    if (m_managed) // only on property change, initial read is prefetched
        m_motif.fetch();
    m_motif.read();
    if (m_motif.hasDecoration() && m_motif.noBorder() != wasNoBorder) {
        // If we just got a hint telling us to hide decorations, we do so.
        if (m_motif.noBorder())
            noborder = rules()->checkNoBorder(true);
        // If the Motif hint is now telling us to show decorations, we only do so if the app didn't
        // instruct us to hide decorations in some other way, though.
        else if (!app_noborder)
            noborder = rules()->checkNoBorder(false);
    }

    // mminimize; - Ignore, bogus - E.g. shading or sending to another desktop is "minimizing" too
    // mmaximize; - Ignore, bogus - Maximizing is basically just resizing
    const bool closabilityChanged = wasClosable != m_motif.close();
    if (isManaged())
        updateDecoration(true);   // Check if noborder state has changed
    if (closabilityChanged) {
        emit closeableChanged(isCloseable());
    }
}

void Client::getIcons()
{
    // First read icons from the window itself
    const QString themedIconName = iconFromDesktopFile();
    if (!themedIconName.isEmpty()) {
        setIcon(QIcon::fromTheme(themedIconName));
        return;
    }
    QIcon icon;
    auto readIcon = [this, &icon](int size, bool scale = true) {
        const QPixmap pix = KWindowSystem::icon(window(), size, size, scale, KWindowSystem::NETWM | KWindowSystem::WMHints, info);
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
        // Then mainclients
        auto mainclients = mainClients();
        for (auto it = mainclients.constBegin();
                it != mainclients.constEnd() && icon.isNull();
                ++it) {
            if (!(*it)->icon().isNull()) {
                icon = (*it)->icon();
                break;
            }
        }
    }
    if (icon.isNull()) {
        // And if nothing else, load icon from classhint or xapp icon
        icon.addPixmap(KWindowSystem::icon(window(),  32,  32,  true, KWindowSystem::ClassHint | KWindowSystem::XApp, info));
        icon.addPixmap(KWindowSystem::icon(window(),  16,  16,  true, KWindowSystem::ClassHint | KWindowSystem::XApp, info));
        icon.addPixmap(KWindowSystem::icon(window(),  64,  64, false, KWindowSystem::ClassHint | KWindowSystem::XApp, info));
        icon.addPixmap(KWindowSystem::icon(window(), 128, 128, false, KWindowSystem::ClassHint | KWindowSystem::XApp, info));
    }
    setIcon(icon);
}

void Client::getSyncCounter()
{
    // TODO: make sync working on XWayland
    static const bool isX11 = kwinApp()->operationMode() == Application::OperationModeX11;
    if (!Xcb::Extensions::self()->isSyncAvailable() || !isX11)
        return;

    Xcb::Property syncProp(false, window(), atoms->net_wm_sync_request_counter, XCB_ATOM_CARDINAL, 0, 1);
    const xcb_sync_counter_t counter = syncProp.value<xcb_sync_counter_t>(XCB_NONE);
    if (counter != XCB_NONE) {
        syncRequest.counter = counter;
        syncRequest.value.hi = 0;
        syncRequest.value.lo = 0;
        auto *c = connection();
        xcb_sync_set_counter(c, syncRequest.counter, syncRequest.value);
        if (syncRequest.alarm == XCB_NONE) {
            const uint32_t mask = XCB_SYNC_CA_COUNTER | XCB_SYNC_CA_VALUE_TYPE | XCB_SYNC_CA_TEST_TYPE | XCB_SYNC_CA_EVENTS;
            const uint32_t values[] = {
                syncRequest.counter,
                XCB_SYNC_VALUETYPE_RELATIVE,
                XCB_SYNC_TESTTYPE_POSITIVE_TRANSITION,
                1
            };
            syncRequest.alarm = xcb_generate_id(c);
            auto cookie = xcb_sync_create_alarm_checked(c, syncRequest.alarm, mask, values);
            ScopedCPointer<xcb_generic_error_t> error(xcb_request_check(c, cookie));
            if (!error.isNull()) {
                syncRequest.alarm = XCB_NONE;
            } else {
                xcb_sync_change_alarm_value_list_t value;
                memset(&value, 0, sizeof(value));
                value.value.hi = 0;
                value.value.lo = 1;
                value.delta.hi = 0;
                value.delta.lo = 1;
                xcb_sync_change_alarm_aux(c, syncRequest.alarm, XCB_SYNC_CA_DELTA | XCB_SYNC_CA_VALUE, &value);
            }
        }
    }
}

/**
 * Send the client a _NET_SYNC_REQUEST
 */
void Client::sendSyncRequest()
{
    if (syncRequest.counter == XCB_NONE || syncRequest.isPending)
        return; // do NOT, NEVER send a sync request when there's one on the stack. the clients will just stop respoding. FOREVER! ...

    if (!syncRequest.failsafeTimeout) {
        syncRequest.failsafeTimeout = new QTimer(this);
        connect(syncRequest.failsafeTimeout, &QTimer::timeout, this,
            [this]() {
                // client does not respond to XSYNC requests in reasonable time, remove support
                if (!ready_for_painting) {
                    // failed on initial pre-show request
                    setReadyForPainting();
                    setupWindowManagementInterface();
                    return;
                }
                // failed during resize
                syncRequest.isPending = false;
                syncRequest.counter = syncRequest.alarm = XCB_NONE;
                delete syncRequest.timeout; delete syncRequest.failsafeTimeout;
                syncRequest.timeout = syncRequest.failsafeTimeout = nullptr;
                syncRequest.lastTimestamp = XCB_CURRENT_TIME;
            }
        );
        syncRequest.failsafeTimeout->setSingleShot(true);
    }
    // if there's no response within 10 seconds, sth. went wrong and we remove XSYNC support from this client.
    // see events.cpp Client::syncEvent()
    syncRequest.failsafeTimeout->start(ready_for_painting ? 10000 : 1000);

    // We increment before the notify so that after the notify
    // syncCounterSerial will equal the value we are expecting
    // in the acknowledgement
    const uint32_t oldLo = syncRequest.value.lo;
    syncRequest.value.lo++;;
    if (oldLo > syncRequest.value.lo) {
        syncRequest.value.hi++;
    }
    if (syncRequest.lastTimestamp >= xTime()) {
        updateXTime();
    }

    // Send the message to client
    sendClientMessage(window(), atoms->wm_protocols, atoms->net_wm_sync_request, syncRequest.value.lo, syncRequest.value.hi);
    syncRequest.isPending = true;
    syncRequest.lastTimestamp = xTime();
}

bool Client::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus() || info->supportsProtocol(NET::TakeFocusProtocol));
}

bool Client::acceptsFocus() const
{
    return info->input();
}

void Client::setBlockingCompositing(bool block)
{
    const bool usedToBlock = blocks_compositing;
    blocks_compositing = rules()->checkBlockCompositing(block && options->windowsBlockCompositing());
    if (usedToBlock != blocks_compositing) {
        emit blockingCompositingChanged(blocks_compositing ? this : 0);
    }
}

void Client::updateAllowedActions(bool force)
{
    if (!isManaged() && !force)
        return;
    NET::Actions old_allowed_actions = NET::Actions(allowed_actions);
    allowed_actions = 0;
    if (isMovable())
        allowed_actions |= NET::ActionMove;
    if (isResizable())
        allowed_actions |= NET::ActionResize;
    if (isMinimizable())
        allowed_actions |= NET::ActionMinimize;
    if (isShadeable())
        allowed_actions |= NET::ActionShade;
    // Sticky state not supported
    if (isMaximizable())
        allowed_actions |= NET::ActionMax;
    if (userCanSetFullScreen())
        allowed_actions |= NET::ActionFullScreen;
    allowed_actions |= NET::ActionChangeDesktop; // Always (Pagers shouldn't show Docks etc.)
    if (isCloseable())
        allowed_actions |= NET::ActionClose;
    if (old_allowed_actions == allowed_actions)
        return;
    // TODO: This could be delayed and compressed - It's only for pagers etc. anyway
    info->setAllowedActions(allowed_actions);

    // ONLY if relevant features have changed (and the window didn't just get/loose moveresize for maximization state changes)
    const NET::Actions relevant = ~(NET::ActionMove|NET::ActionResize);
    if ((allowed_actions & relevant) != (old_allowed_actions & relevant)) {
        if ((allowed_actions & NET::ActionMinimize) != (old_allowed_actions & NET::ActionMinimize)) {
            emit minimizeableChanged(allowed_actions & NET::ActionMinimize);
        }
        if ((allowed_actions & NET::ActionShade) != (old_allowed_actions & NET::ActionShade)) {
            emit shadeableChanged(allowed_actions & NET::ActionShade);
        }
        if ((allowed_actions & NET::ActionMax) != (old_allowed_actions & NET::ActionMax)) {
            emit maximizeableChanged(allowed_actions & NET::ActionMax);
        }
    }
}

void Client::debug(QDebug& stream) const
{
    print<QDebug>(stream);
}

Xcb::StringProperty Client::fetchActivities() const
{
#ifdef KWIN_BUILD_ACTIVITIES
    return Xcb::StringProperty(window(), atoms->activities);
#else
    return Xcb::StringProperty();
#endif
}

void Client::readActivities(Xcb::StringProperty &property)
{
#ifdef KWIN_BUILD_ACTIVITIES
    QStringList newActivitiesList;
    QString prop = QString::fromUtf8(property);
    activitiesDefined = !prop.isEmpty();

    if (prop == Activities::nullUuid()) {
        //copied from setOnAllActivities to avoid a redundant XChangeProperty.
        if (!activityList.isEmpty()) {
            activityList.clear();
            updateActivities(true);
        }
        return;
    }
    if (prop.isEmpty()) {
        //note: this makes it *act* like it's on all activities but doesn't set the property to 'ALL'
        if (!activityList.isEmpty()) {
            activityList.clear();
            updateActivities(true);
        }
        return;
    }

    newActivitiesList = prop.split(u',');

    if (newActivitiesList == activityList)
        return; //expected change, it's ok.

    //otherwise, somebody else changed it. we need to validate before reacting.
    //if the activities are not synced, and there are existing clients with
    //activities specified, somebody has restarted kwin. we can not validate
    //activities in this case. we need to trust the old values.
    if (Activities::self() && Activities::self()->serviceStatus() != KActivities::Consumer::Unknown) {
        QStringList allActivities = Activities::self()->all();
        if (allActivities.isEmpty()) {
            qCDebug(KWIN_CORE) << "no activities!?!?";
            //don't touch anything, there's probably something bad going on and we don't wanna make it worse
            return;
        }


        for (int i = 0; i < newActivitiesList.size(); ++i) {
            if (! allActivities.contains(newActivitiesList.at(i))) {
                qCDebug(KWIN_CORE) << "invalid:" << newActivitiesList.at(i);
                newActivitiesList.removeAt(i--);
            }
        }
    }
    setOnActivities(newActivitiesList);
#else
    Q_UNUSED(property)
#endif
}

void Client::checkActivities()
{
#ifdef KWIN_BUILD_ACTIVITIES
    Xcb::StringProperty property = fetchActivities();
    readActivities(property);
#endif
}

void Client::setSessionActivityOverride(bool needed)
{
    sessionActivityOverride = needed;
    updateActivities(false);
}

QRect Client::decorationRect() const
{
    return QRect(0, 0, width(), height());
}

Xcb::Property Client::fetchFirstInTabBox() const
{
    return Xcb::Property(false, m_client, atoms->kde_first_in_window_list,
                         atoms->kde_first_in_window_list, 0, 1);
}

void Client::readFirstInTabBox(Xcb::Property &property)
{
    setFirstInTabBox(property.toBool(32, atoms->kde_first_in_window_list));
}

void Client::updateFirstInTabBox()
{
    // TODO: move into KWindowInfo
    Xcb::Property property = fetchFirstInTabBox();
    readFirstInTabBox(property);
}

Xcb::StringProperty Client::fetchColorScheme() const
{
    return Xcb::StringProperty(m_client, atoms->kde_color_sheme);
}

void Client::readColorScheme(Xcb::StringProperty &property)
{
    AbstractClient::updateColorScheme(rules()->checkDecoColor(QString::fromUtf8(property)));
}

void Client::updateColorScheme()
{
    Xcb::StringProperty property = fetchColorScheme();
    readColorScheme(property);
}

bool Client::isClient() const
{
    return true;
}

NET::WindowType Client::windowType(bool direct, int supportedTypes) const
{
    // TODO: does it make sense to cache the returned window type for SUPPORTED_MANAGED_WINDOW_TYPES_MASK?
    if (supportedTypes == 0) {
        supportedTypes = SUPPORTED_MANAGED_WINDOW_TYPES_MASK;
    }
    NET::WindowType wt = info->windowType(NET::WindowTypes(supportedTypes));
    if (direct) {
        return wt;
    }
    NET::WindowType wt2 = rules()->checkType(wt);
    if (wt != wt2) {
        wt = wt2;
        info->setWindowType(wt);   // force hint change
    }
    // hacks here
    if (wt == NET::Unknown)   // this is more or less suggested in NETWM spec
        wt = isTransient() ? NET::Dialog : NET::Normal;
    return wt;
}

void Client::cancelFocusOutTimer()
{
    if (m_focusOutTimer) {
        m_focusOutTimer->stop();
    }
}

xcb_window_t Client::frameId() const
{
    return m_frame;
}

Xcb::Property Client::fetchShowOnScreenEdge() const
{
    return Xcb::Property(false, window(), atoms->kde_screen_edge_show, XCB_ATOM_CARDINAL, 0, 1);
}

void Client::readShowOnScreenEdge(Xcb::Property &property)
{
    //value comes in two parts, edge in the lower byte
    //then the type in the upper byte
    // 0 = autohide
    // 1 = raise in front on activate

    const uint32_t value = property.value<uint32_t>(ElectricNone);
    ElectricBorder border = ElectricNone;
    switch (value & 0xFF) {
    case 0:
        border = ElectricTop;
        break;
    case 1:
        border = ElectricRight;
        break;
    case 2:
        border = ElectricBottom;
        break;
    case 3:
        border = ElectricLeft;
        break;
    }
    if (border != ElectricNone) {
        disconnect(m_edgeRemoveConnection);
        disconnect(m_edgeGeometryTrackingConnection);
        bool successfullyHidden = false;

        if (((value >> 8) & 0xFF) == 1) {
            setKeepBelow(true);
            successfullyHidden = keepBelow(); //request could have failed due to user kwin rules

            m_edgeRemoveConnection = connect(this, &AbstractClient::keepBelowChanged, this, [this](){
                if (!keepBelow()) {
                    ScreenEdges::self()->reserve(this, ElectricNone);
                }
            });
        } else {
            hideClient(true);
            successfullyHidden = isHiddenInternal();

            m_edgeGeometryTrackingConnection = connect(this, &Client::geometryChanged, this, [this, border](){
                hideClient(true);
                ScreenEdges::self()->reserve(this, border);
            });
        }

        if (successfullyHidden) {
            ScreenEdges::self()->reserve(this, border);
        } else {
            ScreenEdges::self()->reserve(this, ElectricNone);
        }
    } else if (!property.isNull() && property->type != XCB_ATOM_NONE) {
        // property value is incorrect, delete the property
        // so that the client knows that it is not hidden
        xcb_delete_property(connection(), window(), atoms->kde_screen_edge_show);
    } else {
        // restore
        // TODO: add proper unreserve

        //this will call showOnScreenEdge to reset the state
        disconnect(m_edgeGeometryTrackingConnection);
        ScreenEdges::self()->reserve(this, ElectricNone);
    }
}

void Client::updateShowOnScreenEdge()
{
    Xcb::Property property = fetchShowOnScreenEdge();
    readShowOnScreenEdge(property);
}

void Client::showOnScreenEdge()
{
    disconnect(m_edgeRemoveConnection);

    hideClient(false);
    setKeepBelow(false);
    xcb_delete_property(connection(), window(), atoms->kde_screen_edge_show);
}

void Client::addDamage(const QRegion &damage)
{
    if (!ready_for_painting) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest.counter == XCB_NONE) {  // cannot detect complete redraw, consider done now
            setReadyForPainting();
            setupWindowManagementInterface();
        }
    }
    repaints_region += damage;
    Toplevel::addDamage(damage);
}

bool Client::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
{
    const Client *c2 = dynamic_cast<const Client*>(other);
    if (!c2) {
        return false;
    }
    return Client::belongToSameApplication(this, c2, checks);
}

void Client::updateTabGroupStates(TabGroup::States states)
{
    if (auto t = tabGroup()) {
        t->updateStates(this, states);
    }
}

QSize Client::resizeIncrements() const
{
    return m_geometryHints.resizeIncrements();
}

Xcb::StringProperty Client::fetchApplicationMenuServiceName() const
{
    return Xcb::StringProperty(m_client, atoms->kde_net_wm_appmenu_service_name);
}

void Client::readApplicationMenuServiceName(Xcb::StringProperty &property)
{
    updateApplicationMenuServiceName(QString::fromUtf8(property));
}

void Client::checkApplicationMenuServiceName()
{
    Xcb::StringProperty property = fetchApplicationMenuServiceName();
    readApplicationMenuServiceName(property);
}

Xcb::StringProperty Client::fetchApplicationMenuObjectPath() const
{
    return Xcb::StringProperty(m_client, atoms->kde_net_wm_appmenu_object_path);
}

void Client::readApplicationMenuObjectPath(Xcb::StringProperty &property)
{
    updateApplicationMenuObjectPath(QString::fromUtf8(property));
}

void Client::checkApplicationMenuObjectPath()
{
    Xcb::StringProperty property = fetchApplicationMenuObjectPath();
    readApplicationMenuObjectPath(property);
}

void Client::handleSync()
{
    setReadyForPainting();
    setupWindowManagementInterface();
    syncRequest.isPending = false;
    if (syncRequest.failsafeTimeout)
        syncRequest.failsafeTimeout->stop();
    if (isResize()) {
        if (syncRequest.timeout)
            syncRequest.timeout->stop();
        performMoveResize();
    } else // setReadyForPainting does as well, but there's a small chance for resize syncs after the resize ended
        addRepaintFull();
}

} // namespace

