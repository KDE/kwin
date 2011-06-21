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

#ifndef KWIN_CLIENT_H
#define KWIN_CLIENT_H

#include <config-X11.h>

#include <QFrame>
#include <QPixmap>
#include <netwm.h>
#include <kdebug.h>
#include <assert.h>
#include <kshortcut.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fixx11h.h>

#include <QScriptValue>
#include "scripting/client.h"

#include "utils.h"
#include "options.h"
#include "workspace.h"
#include "kdecoration.h"
#include "rules.h"
#include "toplevel.h"
#include "clientgroup.h"

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

// TODO: Cleanup the order of things in this .h file

class QProcess;
class QTimer;
class KStartupInfoData;

namespace SWrapper
{
class Client;
}

typedef QPair<SWrapper::Client*, QScriptValue> ClientResolution;

namespace KWin
{
namespace TabBox
{

class TabBoxClientImpl;
}


class Workspace;
class Client;
class Bridge;
class PaintRedirector;

class Client
    : public Toplevel
{
    Q_OBJECT
public:
    Client(Workspace* ws);
    Window wrapperId() const;
    Window decorationId() const;

    const Client* transientFor() const;
    Client* transientFor();
    bool isTransient() const;
    bool groupTransient() const;
    bool wasOriginallyGroupTransient() const;
    ClientList mainClients() const; // Call once before loop , is not indirect
    ClientList allMainClients() const; // Call once before loop , is indirect
    bool hasTransient(const Client* c, bool indirect) const;
    const ClientList& transients() const; // Is not indirect
    void checkTransient(Window w);
    Client* findModal(bool allow_itself = false);
    const Group* group() const;
    Group* group();
    void checkGroup(Group* gr = NULL, bool force = false);
    void changeClientLeaderGroup(Group* gr);
    const WindowRules* rules() const;
    void removeRule(Rules* r);
    void setupWindowRules(bool ignore_temporary);
    void applyWindowRules();
    void updateWindowRules();
    void updateFullscreenMonitors(NETFullscreenMonitors topology);

    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    bool isSpecialWindow() const;
    bool hasNETSupport() const;

    /**
      * This is a public object with no wrappers or anything to keep it fast,
      * so in essence, direct access is allowed. Please be very careful while
      * using this object
      */
    QHash<QScriptEngine*, ClientResolution>* scriptCache;

    QSize minSize() const;
    QSize maxSize() const;
    virtual QPoint clientPos() const; // Inside of geometry()
    virtual QSize clientSize() const;
    virtual QRect visibleRect() const;

    bool windowEvent(XEvent* e);
    virtual bool eventFilter(QObject* o, QEvent* e);
#ifdef HAVE_XSYNC
    void syncEvent(XSyncAlarmNotifyEvent* e);
#endif

    bool manage(Window w, bool isMapped);
    void releaseWindow(bool on_shutdown = false);
    void destroyClient();

    /// How to resize the window in order to obey constains (mainly aspect ratios)
    enum Sizemode {
        SizemodeAny,
        SizemodeFixedW, ///< Try not to affect width
        SizemodeFixedH, ///< Try not to affect height
        SizemodeMax ///< Try not to make it larger in either direction
    };
    QSize adjustedSize(const QSize&, Sizemode mode = SizemodeAny) const;
    QSize adjustedSize() const;

    QPixmap icon() const;
    QPixmap icon(const QSize& size) const;
    QPixmap miniIcon() const;
    QPixmap bigIcon() const;
    QPixmap hugeIcon() const;

    bool isActive() const;
    void setActive(bool);

    virtual int desktop() const;
    void setDesktop(int);
    void setOnAllDesktops(bool set);

    virtual QStringList activities() const;
    void setOnActivity(const QString &activity, bool enable);
    void setOnAllActivities(bool set);
    void setOnActivities(QStringList newActivitiesList);
    void updateActivities(bool includeTransients);

    /// Is not minimized and not hidden. I.e. normally visible on some virtual desktop.
    bool isShown(bool shaded_is_shown) const;
    bool isHiddenInternal() const; // For compositing

    bool isShade() const; // True only for ShadeNormal
    ShadeMode shadeMode() const; // Prefer isShade()
    void setShade(ShadeMode mode);
    bool isShadeable() const;

    bool isMinimized() const;
    bool isMaximizable() const;
    QRect geometryRestore() const;
    MaximizeMode maximizeModeRestore() const;
    MaximizeMode maximizeMode() const;
    bool isMinimizable() const;
    void setMaximize(bool vertically, bool horizontally);
    QRect iconGeometry() const;

    void setFullScreen(bool set, bool user);
    bool isFullScreen() const;
    bool isFullScreenable(bool fullscreen_hack = false) const;
    bool isActiveFullScreen() const;
    bool userCanSetFullScreen() const;
    QRect geometryFSRestore() const {
        return geom_fs_restore;    // Only for session saving
    }
    int fullScreenMode() const {
        return fullscreen_mode;    // only for session saving
    }

    bool noBorder() const;
    void setNoBorder(bool set);
    bool userCanSetNoBorder() const;
    void checkNoBorder();

    bool skipTaskbar(bool from_outside = false) const;
    void setSkipTaskbar(bool set, bool from_outside);

    bool skipPager() const;
    void setSkipPager(bool);

    bool skipSwitcher() const;
    void setSkipSwitcher(bool set);

    bool keepAbove() const;
    void setKeepAbove(bool);
    bool keepBelow() const;
    void setKeepBelow(bool);
    Layer layer() const;
    Layer belongsToLayer() const;
    void invalidateLayer();
    int sessionStackingOrder() const;

    void setModal(bool modal);
    bool isModal() const;

    // Auxiliary functions, depend on the windowType
    bool wantsTabFocus() const;
    bool wantsInput() const;

    bool isResizable() const;
    bool isMovable() const;
    bool isMovableAcrossScreens() const;
    bool isCloseable() const; ///< May be closed by the user (May have a close button)

    void takeActivity(int flags, bool handled, allowed_t);   // Takes ActivityFlags as arg (in utils.h)
    void takeFocus(allowed_t);
    void demandAttention(bool set = true);

    void setMask(const QRegion& r, int mode = X::Unsorted);
    QRegion mask() const;

    void updateDecoration(bool check_workspace_pos, bool force = false);
    bool checkBorderSizes(bool also_resize);
    void triggerDecorationRepaint();

    void updateShape();

    void setGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet, bool emitJs = true);
    void setGeometry(const QRect& r, ForceGeometry_t force = NormalGeometrySet, bool emitJs = true);
    void move(int x, int y, ForceGeometry_t force = NormalGeometrySet);
    void move(const QPoint& p, ForceGeometry_t force = NormalGeometrySet);
    /// plainResize() simply resizes
    void plainResize(int w, int h, ForceGeometry_t force = NormalGeometrySet, bool emitJs = true);
    void plainResize(const QSize& s, ForceGeometry_t force = NormalGeometrySet, bool emitJs = true);
    /// resizeWithChecks() resizes according to gravity, and checks workarea position
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet);
    void resizeWithChecks(const QSize& s, ForceGeometry_t force = NormalGeometrySet);
    void keepInArea(QRect area, bool partial = false);
    void setElectricBorderMode(QuickTileMode mode);
    QuickTileMode electricBorderMode() const;
    void setElectricBorderMaximizing(bool maximizing);
    bool isElectricBorderMaximizing() const;
    QRect electricBorderMaximizeGeometry(QPoint pos, int desktop);
    QSize sizeForClientSize(const QSize&, Sizemode mode = SizemodeAny, bool noframe = false) const;

    /** Set the quick tile mode ("snap") of this window.
     * This will also handle preserving and restoring of window geometry as necessary.
     * @param mode The tile mode (left/right) to give this window.
     */
    void setQuickTileMode(QuickTileMode mode, bool keyboard = false);

    void growHorizontal();
    void shrinkHorizontal();
    void growVertical();
    void shrinkVertical();

    bool providesContextHelp() const;
    KShortcut shortcut() const;
    void setShortcut(const QString& cut);

    WindowOperation mouseButtonToWindowOperation(Qt::MouseButtons button);
    bool performMouseCommand(Options::MouseCommand, const QPoint& globalPos, bool handled = false);

    QRect adjustedClientArea(const QRect& desktop, const QRect& area) const;

    Colormap colormap() const;

    /// Updates visibility depending on being shaded, virtual desktop, etc.
    void updateVisibility();
    /// Hides a client - Basically like minimize, but without effects, it's simply hidden
    void hideClient(bool hide);
    bool hiddenPreview() const; ///< Window is mapped in order to get a window pixmap

    virtual void setupCompositing();
    virtual void finishCompositing();
    inline bool isBlockingCompositing() { return blocks_compositing; }
    void updateCompositeBlocking(bool readProperty = false);

    QString caption(bool full = true) const;
    void updateCaption();

    void keyPressEvent(uint key_code);   // FRAME ??
    void updateMouseGrab();
    Window moveResizeGrabWindow() const;

    const QPoint calculateGravitation(bool invert, int gravity = 0) const;   // FRAME public?

    void NETMoveResize(int x_root, int y_root, NET::Direction direction);
    void NETMoveResizeWindow(int flags, int x, int y, int width, int height);
    void restackWindow(Window above, int detail, NET::RequestSource source, Time timestamp,
                       bool send_event = false);

    void gotPing(Time timestamp);

    void checkWorkspacePosition();
    void updateUserTime(Time time = CurrentTime);
    Time userTime() const;
    bool hasUserTimeSupport() const;
    bool ignoreFocusStealing() const;

    /// Does 'delete c;'
    static void deleteClient(Client* c, allowed_t);

    static bool belongToSameApplication(const Client* c1, const Client* c2, bool active_hack = false);
    static bool sameAppWindowRoleMatch(const Client* c1, const Client* c2, bool active_hack);
    static void readIcons(Window win, QPixmap* icon, QPixmap* miniicon, QPixmap* bigicon, QPixmap* hugeicon);

    void minimize(bool avoid_animation = false);
    void unminimize(bool avoid_animation = false);
    void closeWindow();
    void killWindow();
    void maximize(MaximizeMode);
    void toggleShade();
    void showContextHelp();
    void cancelShadeHoverTimer();
    void cancelAutoRaise();
    void checkActiveModal();
    StrutRect strutRect(StrutArea area) const;
    StrutRects strutRects() const;
    bool hasStrut() const;

    // Tabbing functions
    ClientGroup* clientGroup() const; // Returns a pointer to client_group
    void setClientGroup(ClientGroup* group);
    /*
    *   If shown is true the client is mapped and raised, if false
    *   the client is unmapped and hidden, this function is called
    *   when the tabbing group of the client switches its visible
    *   client.
    */
    void setClientShown(bool shown);
    /*
    *   When a click is done in the decoration and it calls the group
    *   to change the visible client it starts to move-resize the new
    *   client, this function stops it.
    */
    void dontMoveResize();

    /**
     * Whether or not the window has a strut that expands through the invisible area of
     * an xinerama setup where the monitors are not the same resolution.
     */
    bool hasOffscreenXineramaStrut() const;

    bool isMove() const {
        return moveResizeMode && mode == PositionCenter;
    }
    bool isResize() const {
        return moveResizeMode && mode != PositionCenter;
    }

    // Decorations <-> Effects
    const QPixmap *topDecoPixmap() const {
        return &decorationPixmapTop;
    }
    const QPixmap *leftDecoPixmap() const {
        return &decorationPixmapLeft;
    }
    const QPixmap *bottomDecoPixmap() const {
        return &decorationPixmapBottom;
    }
    const QPixmap *rightDecoPixmap() const {
        return &decorationPixmapRight;
    }

    int paddingLeft() const {
        return padding_left;
    }
    int paddingRight() const {
        return padding_right;
    }
    int paddingTop() const {
        return padding_top;
    }
    int paddingBottom() const {
        return padding_bottom;
    }

    bool decorationPixmapRequiresRepaint();
    void ensureDecorationPixmapsPainted();

    QRect decorationRect() const;

    QRect transparentRect() const;

    QRegion decorationPendingRegion() const;

    enum CoordinateMode {
        DecorationRelative, // Relative to the top left corner of the decoration
        WindowRelative      // Relative to the top left corner of the window
    };
    void layoutDecorationRects(QRect &left, QRect &top, QRect &right, QRect &bottom, CoordinateMode mode) const;
    virtual void addRepaintFull();

    TabBox::TabBoxClientImpl* tabBoxClient() const {
        return m_tabBoxClient;
    }

    //sets whether the client should be treated as a SessionInteract window
    void setSessionInteract(bool needed);

private slots:
    void autoRaise();
    void shadeHover();
    void shadeUnhover();
    void shortcutActivated();
    void delayedMoveResize();

private:
    friend class Bridge; // FRAME
    virtual void processMousePressEvent(QMouseEvent* e);

private:
    // Use Workspace::createClient()
    virtual ~Client(); ///< Use destroyClient() or releaseWindow()

    Position mousePosition(const QPoint&) const;
    void updateCursor();

    // Handlers for X11 events
    bool mapRequestEvent(XMapRequestEvent* e);
    void unmapNotifyEvent(XUnmapEvent* e);
    void destroyNotifyEvent(XDestroyWindowEvent* e);
    void configureRequestEvent(XConfigureRequestEvent* e);
    virtual void propertyNotifyEvent(XPropertyEvent* e);
    void clientMessageEvent(XClientMessageEvent* e);
    void enterNotifyEvent(XCrossingEvent* e);
    void leaveNotifyEvent(XCrossingEvent* e);
    void focusInEvent(XFocusInEvent* e);
    void focusOutEvent(XFocusOutEvent* e);
#ifdef HAVE_XDAMAGE
    virtual void damageNotifyEvent(XDamageNotifyEvent* e);
#endif

    bool buttonPressEvent(Window w, int button, int state, int x, int y, int x_root, int y_root);
    bool buttonReleaseEvent(Window w, int button, int state, int x, int y, int x_root, int y_root);
    bool motionNotifyEvent(Window w, int state, int x, int y, int x_root, int y_root);
    void checkQuickTilingMaximizationZones(int xroot, int yroot);

    bool processDecorationButtonPress(int button, int state, int x, int y, int x_root, int y_root,
                                      bool ignoreMenu = false);

protected:
    virtual void debug(QDebug& stream) const;
    virtual bool shouldUnredirect() const;

private slots:
    void pingTimeout();
    void processKillerExited();
    void demandAttentionKNotify();
    void syncTimeout();
    void delayedSetShortcut();
    void repaintDecorationPending();

    //Signals for the scripting interface
    //Signals make an excellent way for communication
    //in between objects as compared to simple function
    //calls
signals:
    void s_clientMoved();
    void clientManaging(KWin::Client*);
    void s_minimized();
    void s_unminimized();
    void maximizeSet(QPair<bool, bool>);
    void s_activated();
    void s_fullScreenSet(bool, bool);
    void clientMaximizedStateChanged(KWin::Client*, KDecorationDefines::MaximizeMode);
    void clientMinimized(KWin::Client* client, bool animate);
    void clientUnminimized(KWin::Client* client, bool animate);
    void clientStartUserMovedResized(KWin::Client*);
    void clientStepUserMovedResized(KWin::Client *, const QRect&);
    void clientFinishUserMovedResized(KWin::Client*);

    // To make workspace-client calls, a few slots are also
    // required
public slots:
    void sl_activated();

private:
    void exportMappingState(int s);   // ICCCM 4.1.3.1, 4.1.4, NETWM 2.5.1
    bool isManaged() const; ///< Returns false if this client is not yet managed
    void updateAllowedActions(bool force = false);
    QRect fullscreenMonitorsArea(NETFullscreenMonitors topology) const;
    void changeMaximize(bool horizontal, bool vertical, bool adjust);
    int checkFullScreenHack(const QRect& geom) const;   // 0 - None, 1 - One xinerama screen, 2 - Full area
    void updateFullScreenHack(const QRect& geom);
    void getWmNormalHints();
    void getMotifHints();
    void getIcons();
    void fetchName();
    void fetchIconicName();
    QString readName() const;
    void setCaption(const QString& s, bool force = false);
    bool hasTransientInternal(const Client* c, bool indirect, ConstClientList& set) const;
    void finishWindowRules();
    void setShortcutInternal(const KShortcut& cut);

    void checkDirection(int new_diff, int old_diff, QRect& rect, const QRect& area);
    void configureRequest(int value_mask, int rx, int ry, int rw, int rh, int gravity, bool from_tool);
    NETExtendedStrut strut() const;
    int checkShadeGeometry(int w, int h);
    void blockGeometryUpdates(bool block);
    void getSyncCounter();
    void sendSyncRequest();

    bool startMoveResize();
    void finishMoveResize(bool cancel);
    void leaveMoveResize();
    void checkUnrestrictedMoveResize();
    void handleMoveResize(int x, int y, int x_root, int y_root);
    void startDelayedMoveResize();
    void stopDelayedMoveResize();
    void positionGeometryTip();
    void grabButton(int mod);
    void ungrabButton(int mod);
    void resetMaximize();
    void resizeDecoration(const QSize& s);
    void performMoveResize();

    void pingWindow();
    void killProcess(bool ask, Time timestamp = CurrentTime);
    void updateUrgency();
    static void sendClientMessage(Window w, Atom a, Atom protocol,
                                  long data1 = 0, long data2 = 0, long data3 = 0);

    void embedClient(Window w, const XWindowAttributes& attr);
    void detectNoBorder();
    void destroyDecoration();
    void updateFrameExtents();

    void internalShow(allowed_t);
    void internalHide(allowed_t);
    void internalKeep(allowed_t);
    void map(allowed_t);
    void unmap(allowed_t);
    void updateHiddenPreview();

    void updateInputShape();
    void repaintDecorationPixmap(QPixmap& pix, const QRect& r, const QPixmap& src, QRegion reg);
    void resizeDecorationPixmaps();

    Time readUserTimeMapTimestamp(const KStartupInfoId* asn_id, const KStartupInfoData* asn_data,
                                  bool session) const;
    Time readUserCreationTime() const;
    void startupIdChanged();

    Window client;
    Window wrapper;
    KDecoration* decoration;
    Bridge* bridge;
    int desk;
    QStringList activityList;
    bool buttonDown;
    bool moveResizeMode;
    Window move_resize_grab_window;
    bool move_resize_has_keyboard_grab;
    bool unrestrictedMoveResize;

    Position mode;
    QPoint moveOffset;
    QPoint invertedMoveOffset;
    QRect moveResizeGeom;
    QRect initialMoveResizeGeom;
    XSizeHints xSizeHint;
    void sendSyntheticConfigureNotify();
    enum MappingState {
        Withdrawn, ///< Not handled, as per ICCCM WithdrawnState
        Mapped, ///< The frame is mapped
        Unmapped, ///< The frame is not mapped
        Kept ///< The frame should be unmapped, but is kept (For compositing)
    };
    MappingState mapping_state;

    /** The quick tile mode of this window.
     */
    int quick_tile_mode;
    QRect geom_pretile;

    void readTransient();
    Window verifyTransientFor(Window transient_for, bool set);
    void addTransient(Client* cl);
    void removeTransient(Client* cl);
    void removeFromMainClients();
    void cleanGrouping();
    void checkGroupTransients();
    void setTransient(Window new_transient_for_id);
    Client* transient_for;
    Window transient_for_id;
    Window original_transient_for_id;
    ClientList transients_list; // SELI TODO: Make this ordered in stacking order?
    ShadeMode shade_mode;
    uint active : 1;
    uint deleting : 1; ///< True when doing cleanup and destroying the client
    uint keep_above : 1; ///< NET::KeepAbove (was stays_on_top)
    uint skip_taskbar : 1;
    uint original_skip_taskbar : 1; ///< Unaffected by KWin
    uint Pdeletewindow : 1; ///< Does the window understand the DeleteWindow protocol?
    uint Ptakefocus : 1;///< Does the window understand the TakeFocus protocol?
    uint Ptakeactivity : 1; ///< Does it support _NET_WM_TAKE_ACTIVITY
    uint Pcontexthelp : 1; ///< Does the window understand the ContextHelp protocol?
    uint Pping : 1; ///< Does it support _NET_WM_PING?
    uint input : 1; ///< Does the window want input in its wm_hints
    uint skip_pager : 1;
    uint skip_switcher : 1;
    uint motif_may_resize : 1;
    uint motif_may_move : 1;
    uint motif_may_close : 1;
    uint keep_below : 1; ///< NET::KeepBelow
    uint minimized : 1;
    uint hidden : 1; ///< Forcibly hidden by calling hide()
    uint modal : 1; ///< NET::Modal
    uint noborder : 1;
    uint app_noborder : 1; ///< App requested no border via window type, shape extension, etc.
    uint motif_noborder : 1; ///< App requested no border via Motif WM hints
    uint urgency : 1; ///< XWMHints, UrgencyHint
    uint ignore_focus_stealing : 1; ///< Don't apply focus stealing prevention to this client
    uint demands_attention : 1;
    bool blocks_compositing;
    WindowRules client_rules;
    void getWMHints();
    void readIcons();
    void getWindowProtocols();
    QPixmap icon_pix;
    QPixmap miniicon_pix;
    QPixmap bigicon_pix;
    QPixmap hugeicon_pix;
    QCursor cursor;
    // DON'T reorder - Saved to config files !!!
    enum FullScreenMode {
        FullScreenNone,
        FullScreenNormal,
        FullScreenHack ///< Non-NETWM fullscreen (noborder and size of desktop)
    };
    FullScreenMode fullscreen_mode;
    MaximizeMode max_mode;
    QRect geom_restore;
    QRect geom_fs_restore;
    MaximizeMode maxmode_restore;
    QTimer* autoRaiseTimer;
    QTimer* shadeHoverTimer;
    QTimer* delayedMoveResizeTimer;
    Colormap cmap;
    QString cap_normal, cap_iconic, cap_suffix;
    Group* in_group;
    Window window_group;
    ClientGroup* client_group;
    Layer in_layer;
    QTimer* ping_timer;
    QProcess* process_killer;
    Time ping_timestamp;
    Time user_time;
    unsigned long allowed_actions;
    QSize client_size;
    int block_geometry_updates; // > 0 = New geometry is remembered, but not actually set
    enum PendingGeometry_t {
        PendingGeometryNone,
        PendingGeometryNormal,
        PendingGeometryForced
    };
    PendingGeometry_t pending_geometry_update;
    QRect geom_before_block;
    QRect deco_rect_before_block;
    bool shade_geometry_change;
#ifdef HAVE_XSYNC
    XSyncCounter sync_counter;
    XSyncValue sync_counter_value;
    XSyncAlarm sync_alarm;
#endif
    QTimer* sync_timeout;
    bool sync_resize_pending;
    int border_left, border_right, border_top, border_bottom;
    int padding_left, padding_right, padding_top, padding_bottom;
    QRegion _mask;
    static bool check_active_modal; ///< \see Client::checkActiveModal()
    KShortcut _shortcut;
    int sm_stacking_order;
    friend struct FetchNameInternalPredicate;
    friend struct CheckIgnoreFocusStealingProcedure;
    friend struct ResetupRulesProcedure;
    friend class GeometryUpdatesBlocker;
    QTimer* demandAttentionKNotifyTimer;
    QPixmap decorationPixmapLeft, decorationPixmapRight, decorationPixmapTop, decorationPixmapBottom;
    // we (instead of Qt) initialize the Pixmaps, and have to free them
    bool m_responsibleForDecoPixmap;
    PaintRedirector* paintRedirector;
    TabBox::TabBoxClientImpl* m_tabBoxClient;

    bool electricMaximizing;
    QuickTileMode electricMode;

    friend bool performTransiencyCheck();
    friend class SWrapper::Client;

    void checkActivities();
    bool activitiesDefined; //whether the x property was actually set

    bool needsSessionInteract;
};

/**
 * Helper for Client::blockGeometryUpdates() being called in pairs (true/false)
 */
class GeometryUpdatesBlocker
{
public:
    GeometryUpdatesBlocker(Client* c)
        : cl(c) {
        cl->blockGeometryUpdates(true);
    }
    ~GeometryUpdatesBlocker() {
        cl->blockGeometryUpdates(false);
    }

private:
    Client* cl;
};

/**
 * NET WM Protocol handler class
 */
class WinInfo : public NETWinInfo2
{
private:
    typedef KWin::Client Client; // Because of NET::Client

public:
    WinInfo(Client* c, Display * display, Window window,
            Window rwin, const unsigned long pr[], int pr_size);
    virtual void changeDesktop(int desktop);
    virtual void changeFullscreenMonitors(NETFullscreenMonitors topology);
    virtual void changeState(unsigned long state, unsigned long mask);
    void disable();

private:
    Client * m_client;
};

inline Window Client::wrapperId() const
{
    return wrapper;
}

inline Window Client::decorationId() const
{
    return decoration != NULL ? decoration->widget()->winId() : None;
}

inline const Client* Client::transientFor() const
{
    return transient_for;
}

inline Client* Client::transientFor()
{
    return transient_for;
}

inline bool Client::groupTransient() const
{
    return transient_for_id == rootWindow();
}

// Needed because verifyTransientFor() may set transient_for_id to root window,
// if the original value has a problem (window doesn't exist, etc.)
inline bool Client::wasOriginallyGroupTransient() const
{
    return original_transient_for_id == rootWindow();
}

inline bool Client::isTransient() const
{
    return transient_for_id != None;
}

inline const ClientList& Client::transients() const
{
    return transients_list;
}

inline const Group* Client::group() const
{
    return in_group;
}

inline Group* Client::group()
{
    return in_group;
}

inline void Client::setClientGroup(ClientGroup* group)
{
    client_group = group;
}

inline ClientGroup* Client::clientGroup() const
{
    return client_group;
}

inline bool Client::isMinimized() const
{
    return minimized;
}

inline bool Client::isActive() const
{
    return active;
}

inline bool Client::isShown(bool shaded_is_shown) const
{
    return !isMinimized() && (!isShade() || shaded_is_shown) && !hidden &&
           (clientGroup() == NULL || clientGroup()->visible() == this);
}

inline bool Client::isHiddenInternal() const
{
    return hidden;
}

inline bool Client::isShade() const
{
    return shade_mode == ShadeNormal;
}

inline ShadeMode Client::shadeMode() const
{
    return shade_mode;
}

inline QPixmap Client::icon() const
{
    return icon_pix;
}

inline QPixmap Client::miniIcon() const
{
    return miniicon_pix;
}

inline QPixmap Client::bigIcon() const
{
    return bigicon_pix;
}

inline QPixmap Client::hugeIcon() const
{
    return hugeicon_pix;
}

inline QRect Client::geometryRestore() const
{
    return geom_restore;
}

inline Client::MaximizeMode Client::maximizeModeRestore() const
{
    return maxmode_restore;
}

inline Client::MaximizeMode Client::maximizeMode() const
{
    return max_mode;
}

inline bool Client::skipTaskbar(bool from_outside) const
{
    return from_outside ? original_skip_taskbar : skip_taskbar;
}

inline bool Client::skipPager() const
{
    return skip_pager;
}

inline bool Client::skipSwitcher() const
{
    return skip_switcher;
}

inline bool Client::keepAbove() const
{
    return keep_above;
}

inline bool Client::keepBelow() const
{
    return keep_below;
}

inline bool Client::isFullScreen() const
{
    return fullscreen_mode != FullScreenNone;
}

inline bool Client::isModal() const
{
    return modal;
}

inline bool Client::hasNETSupport() const
{
    return info->hasNETSupport();
}

inline Colormap Client::colormap() const
{
    return cmap;
}

inline void Client::invalidateLayer()
{
    in_layer = UnknownLayer;
}

inline int Client::sessionStackingOrder() const
{
    return sm_stacking_order;
}

inline bool Client::isManaged() const
{
    return mapping_state != Withdrawn;
}

inline QPoint Client::clientPos() const
{
    return QPoint(border_left, border_top);
}

inline QSize Client::clientSize() const
{
    return client_size;
}

inline QRect Client::visibleRect() const
{
    return Toplevel::visibleRect().adjusted(-padding_left, -padding_top, padding_right, padding_bottom);
}

inline void Client::setGeometry(const QRect& r, ForceGeometry_t force, bool emitJs)
{
    setGeometry(r.x(), r.y(), r.width(), r.height(), force, emitJs);
}

inline void Client::move(const QPoint& p, ForceGeometry_t force)
{
    move(p.x(), p.y(), force);
}

inline void Client::plainResize(const QSize& s, ForceGeometry_t force, bool emitJs)
{
    plainResize(s.width(), s.height(), force, emitJs);
}

inline void Client::resizeWithChecks(const QSize& s, ForceGeometry_t force)
{
    resizeWithChecks(s.width(), s.height(), force);
}

inline bool Client::hasUserTimeSupport() const
{
    return info->userTime() != -1U;
}

inline bool Client::ignoreFocusStealing() const
{
    return ignore_focus_stealing;
}

inline const WindowRules* Client::rules() const
{
    return &client_rules;
}

KWIN_PROCEDURE(CheckIgnoreFocusStealingProcedure, Client, cl->ignore_focus_stealing = options->checkIgnoreFocusStealing(cl));

inline Window Client::moveResizeGrabWindow() const
{
    return move_resize_grab_window;
}

inline KShortcut Client::shortcut() const
{
    return _shortcut;
}

inline void Client::removeRule(Rules* rule)
{
    client_rules.remove(rule);
}

inline bool Client::hiddenPreview() const
{
    return mapping_state == Kept;
}

KWIN_COMPARE_PREDICATE(WrapperIdMatchPredicate, Client, Window, cl->wrapperId() == value);

} // namespace

#endif
