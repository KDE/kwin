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

#include "utils.h"
#include "options.h"
#include "workspace.h"
#include "kdecoration.h"
#include "rules.h"
#include "toplevel.h"
#include "tabgroup.h"

#ifdef HAVE_XSYNC
#include <X11/extensions/sync.h>
#endif

// TODO: Cleanup the order of things in this .h file

class QProcess;
class QTimer;
class KStartupInfoData;

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
    /**
     * Whether this Client is active or not. Use Workspace::activateClient() to activate a Client.
     * @see Workspace::activateClient
     **/
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    /**
     * The Caption of the Client. Read from WM_NAME property together with a suffix for hostname and shortcut.
     * To read only the caption as provided by WM_NAME, use the getter with an additional @c false value.
     **/
    Q_PROPERTY(QString caption READ caption NOTIFY captionChanged)
    /**
     * Whether the window can be closed by the user. The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     **/
    Q_PROPERTY(bool closeable READ isCloseable)
    /**
     * The desktop this Client is on. If the Client is on all desktops the property has value -1.
     **/
    Q_PROPERTY(int desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)
    /**
     * Whether this Client is fullScreen. A Client might either be fullScreen due to the _NET_WM property
     * or through a legacy support hack. The fullScreen state can only be changed if the Client does not
     * use the legacy hack. To be sure whether the state changed, connect to the notify signal.
     **/
    Q_PROPERTY(bool fullScreen READ isFullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
    /**
     * Whether the Client can be set to fullScreen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     **/
    Q_PROPERTY(bool fullScreenable READ isFullScreenable)
    /**
     * The geometry of this Client. Be aware that depending on resize mode the geometryChanged signal
     * might be emitted at each resize step or only at the end of the resize operation.
     **/
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry)
    /**
     * Whether the Client is set to be kept above other windows.
     **/
    Q_PROPERTY(bool keepAbove READ keepAbove WRITE setKeepAbove NOTIFY keepAboveChanged)
    /**
     * Whether the Client is set to be kept below other windows.
     **/
    Q_PROPERTY(bool keepBelow READ keepBelow WRITE setKeepBelow NOTIFY keepBelowChanged)
    /**
     * Whether the Client can be maximized both horizontally and vertically.
     * The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     **/
    Q_PROPERTY(bool maximizable READ isMaximizable)
    /**
     * Whether the Client can be minimized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     **/
    Q_PROPERTY(bool minimizable READ isMinimizable)
    /**
     * Whether the Client is minimized.
     **/
    Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized NOTIFY minimizedChanged)
    /**
     * Whether the Client represents a modal window.
     **/
    Q_PROPERTY(bool modal READ isModal NOTIFY modalChanged)
    /**
     * Whether the Client is moveable. Even if it is not moveable, it might be possible to move
     * it to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveableAcrossScreens
     **/
    Q_PROPERTY(bool moveable READ isMovable)
    /**
     * Whether the Client can be moved to another screen. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     * @see moveable
     **/
    Q_PROPERTY(bool moveableAcrossScreens READ isMovableAcrossScreens)
    /**
     * Whether the Client provides context help. Mostly needed by decorations to decide whether to
     * show the help button or not.
     **/
    Q_PROPERTY(bool providesContextHelp READ providesContextHelp CONSTANT)
    /**
     * Whether the Client can be resized. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     **/
    Q_PROPERTY(bool resizeable READ isResizable)
    /**
     * Whether the Client can be shaded. The property is evaluated each time it is invoked.
     * Because of that there is no notify signal.
     **/
    Q_PROPERTY(bool shadeable READ isShadeable)
    /**
     * Whether the Client is shaded.
     **/
    Q_PROPERTY(bool shade READ isShade WRITE setShade NOTIFY shadeChanged)
    /**
     * Whether the Client is a transient Window to another Window.
     * @see transientFor
     **/
    Q_PROPERTY(bool transient READ isTransient NOTIFY transientChanged)
    /**
     * The Client to which this Client is a transient if any.
     **/
    Q_PROPERTY(KWin::Client *transientFor READ transientFor NOTIFY transientChanged)
    /**
     * By how much the window wishes to grow/shrink at least. Usually QSize(1,1).
     * MAY BE DISOBEYED BY THE WM! It's only for information, do NOT rely on it at all.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     */
    Q_PROPERTY(QSize basicUnit READ basicUnit)
    /**
     * Whether the Client is currently being moved by the user.
     * Notify signal is emitted when the Client starts or ends move/resize mode.
     **/
    Q_PROPERTY(bool move READ isMove NOTIFY moveResizedChanged)
    /**
     * Whether the Client is currently being resized by the user.
     * Notify signal is emitted when the Client starts or ends move/resize mode.
     **/
    Q_PROPERTY(bool resize READ isResize NOTIFY moveResizedChanged)
    /**
     * The optional geometry representing the minimized Client in e.g a taskbar.
     * See _NET_WM_ICON_GEOMETRY at http://standards.freedesktop.org/wm-spec/wm-spec-latest.html .
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     **/
    Q_PROPERTY(QRect iconGeometry READ iconGeometry)
    /**
     * Returns whether the window is any of special windows types (desktop, dock, splash, ...),
     * i.e. window types that usually don't have a window frame and the user does not use window
     * management (moving, raising,...) on them.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     **/
    Q_PROPERTY(bool specialWindow READ isSpecialWindow)
    /**
     * Whether the Client can accept keyboard focus.
     * The value is evaluated each time the getter is called.
     * Because of that no changed signal is provided.
     **/
    Q_PROPERTY(bool wantsInput READ wantsInput)
    // TODO: a QIcon with all icon sizes?
    Q_PROPERTY(QPixmap icon READ icon NOTIFY iconChanged)
    /**
     * Whether the Client should be excluded from window switching effects.
     **/
    Q_PROPERTY(bool skipSwitcher READ skipSwitcher WRITE setSkipSwitcher NOTIFY skipSwitcherChanged)
    /**
     * Indicates that the window should not be included on a taskbar.
     **/
    Q_PROPERTY(bool skipTaskbar READ skipTaskbar WRITE setSkipTaskbar NOTIFY skipTaskbarChanged)
    /**
     * Indicates that the window should not be included on a Pager.
     **/
    Q_PROPERTY(bool skipPager READ skipPager WRITE setSkipPager NOTIFY skipPagerChanged)
    /**
     * The "Window Tabs" Group this Client belongs to.
     **/
    Q_PROPERTY(KWin::TabGroup* tabGroup READ tabGroup NOTIFY tabGroupChanged SCRIPTABLE false)
    /**
     * Whether this Client is the currently visible Client in its Client Group (Window Tabs).
     * For change connect to the visibleChanged signal on the Client's Group.
     **/
    Q_PROPERTY(bool isCurrentTab READ isCurrentTab)
    /**
     * Minimum size as specified in WM_NORMAL_HINTS
     **/
    Q_PROPERTY(QSize minSize READ minSize)
    /**
     * Maximum size as specified in WM_NORMAL_HINTS
     **/
    Q_PROPERTY(QSize maxSize READ maxSize)
    /**
     * Whether the window has a decoration or not.
     * This property is not allowed to be set by applications themselves.
     * The decision whether a window has a border or not belongs to the window manager.
     * If this property gets abused by application developers, it will be removed again.
     **/
    Q_PROPERTY(bool noBorder READ noBorder WRITE setNoBorder)
    /**
     * Whether window state _NET_WM_STATE_DEMANDS_ATTENTION is set. This state indicates that some
     * action in or with the window happened. For example, it may be set by the Window Manager if
     * the window requested activation but the Window Manager refused it, or the application may set
     * it if it finished some work. This state may be set by both the Client and the Window Manager.
     * It should be unset by the Window Manager when it decides the window got the required attention
     * (usually, that it got activated).
     **/
    Q_PROPERTY(bool demandsAttention READ isDemandingAttention WRITE demandAttention NOTIFY demandsAttentionChanged)
    /**
     * A client can block compositing. That is while the Client is alive and the state is set,
     * Compositing is suspended and is resumed when there are no Clients blocking compositing any
     * more.
     *
     * This is actually set by a window property, unfortunately not used by the target application
     * group. For convenience it's exported as a property to the scripts.
     *
     * Use with care!
     **/
    Q_PROPERTY(bool blocksCompositing READ isBlockingCompositing WRITE setBlockingCompositing NOTIFY blockingCompositingChanged)
public:
    Client(Workspace* ws);
    Window wrapperId() const;
    Window decorationId() const;
    Window inputId() const { return input_window; }

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
    void updateWindowRules(Rules::Types selection);
    void updateFullscreenMonitors(NETFullscreenMonitors topology);

    /**
     * Returns true for "special" windows and false for windows which are "normal"
     * (normal=window which has a border, can be moved by the user, can be closed, etc.)
     * true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
     * false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
     */
    bool isSpecialWindow() const;
    bool hasNETSupport() const;

    QSize minSize() const;
    QSize maxSize() const;
    QSize basicUnit() const;
    virtual QPoint clientPos() const; // Inside of geometry()
    virtual QSize clientSize() const;
    virtual QRect visibleRect() const;
    QPoint inputPos() const { return input_offset; } // Inside of geometry()

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
    void setShade(bool set);
    void setShade(ShadeMode mode);
    bool isShadeable() const;

    bool isMinimized() const;
    bool isMaximizable() const;
    QRect geometryRestore() const;
    MaximizeMode maximizeMode() const;
    QuickTileMode quickTileMode() const;
    bool isMinimizable() const;
    void setMaximize(bool vertically, bool horizontally);
    QRect iconGeometry() const;

    void setFullScreen(bool set, bool user = true);
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
    void setSkipTaskbar(bool set, bool from_outside = false);

    bool skipPager() const;
    void setSkipPager(bool);

    bool skipSwitcher() const;
    void setSkipSwitcher(bool set);

    bool keepAbove() const;
    void setKeepAbove(bool);
    bool keepBelow() const;
    void setKeepBelow(bool);
    virtual Layer layer() const;
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
    bool isDemandingAttention() const {
        return demands_attention;
    }
    void demandAttention(bool set = true);

    void setMask(const QRegion& r, int mode = X::Unsorted);
    QRegion mask() const;

    void updateDecoration(bool check_workspace_pos, bool force = false);
    bool checkBorderSizes(bool also_resize);
    void triggerDecorationRepaint();

    void updateShape();

    void setGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet);
    void setGeometry(const QRect& r, ForceGeometry_t force = NormalGeometrySet);
    void move(int x, int y, ForceGeometry_t force = NormalGeometrySet);
    void move(const QPoint& p, ForceGeometry_t force = NormalGeometrySet);
    /// plainResize() simply resizes
    void plainResize(int w, int h, ForceGeometry_t force = NormalGeometrySet);
    void plainResize(const QSize& s, ForceGeometry_t force = NormalGeometrySet);
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

    virtual bool setupCompositing();
    virtual void finishCompositing();
    void setBlockingCompositing(bool block);
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

    void checkWorkspacePosition(QRect oldGeometry = QRect(), int oldDesktop = -2);
    void updateUserTime(Time time = CurrentTime);
    Time userTime() const;
    bool hasUserTimeSupport() const;

    /// Does 'delete c;'
    static void deleteClient(Client* c, allowed_t);

    static bool belongToSameApplication(const Client* c1, const Client* c2, bool active_hack = false);
    static bool sameAppWindowRoleMatch(const Client* c1, const Client* c2, bool active_hack);
    static void readIcons(Window win, QPixmap* icon, QPixmap* miniicon, QPixmap* bigicon, QPixmap* hugeicon);

    void setMinimized(bool set);
    void minimize(bool avoid_animation = false);
    void unminimize(bool avoid_animation = false);
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
    TabGroup* tabGroup() const; // Returns a pointer to client_group
    Q_INVOKABLE inline bool tabBefore(Client *other, bool activate) { return tabTo(other, false, activate); }
    Q_INVOKABLE inline bool tabBehind(Client *other, bool activate) { return tabTo(other, true, activate); }
    /**
     * Syncs the *dynamic* @param property @param fromThisClient or the @link currentTab() to
     * all members of the @link tabGroup() (if there is one)
     *
     * eg. if you call:
     * client->setProperty("kwin_tiling_floats", true);
     * client->syncTabGroupFor("kwin_tiling_floats", true)
     * all clients in this tabGroup will have ::property("kwin_tiling_floats").toBool() == true
     *
     * WARNING: non dynamic properties are ignored - you're not supposed to alter/update such explicitly
     */
    Q_INVOKABLE void syncTabGroupFor(QString property, bool fromThisClient = false);
    Q_INVOKABLE bool untab(const QRect &toGeometry = QRect());
    /**
     * Set tab group - this is to be invoked by TabGroup::add/remove(client) and NO ONE ELSE
     */
    void setTabGroup(TabGroup* group);
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
    bool isCurrentTab() const;

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

    TabBox::TabBoxClientImpl* tabBoxClient() const {
        return m_tabBoxClient;
    }
    bool isFirstInTabBox() const {
        return m_firstInTabBox;
    }
    void setFirstInTabBox(bool enable) {
        m_firstInTabBox = enable;
    }
    void updateFirstInTabBox();

    //sets whether the client should be treated as a SessionInteract window
    void setSessionInteract(bool needed);
    virtual bool isClient() const;

public slots:
    void closeWindow();

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
    virtual void damageNotifyEvent(XDamageNotifyEvent* e);

    bool buttonPressEvent(Window w, int button, int state, int x, int y, int x_root, int y_root);
    bool buttonReleaseEvent(Window w, int button, int state, int x, int y, int x_root, int y_root);
    bool motionNotifyEvent(Window w, int state, int x, int y, int x_root, int y_root);
    void checkQuickTilingMaximizationZones(int xroot, int yroot);

    bool processDecorationButtonPress(int button, int state, int x, int y, int x_root, int y_root,
                                      bool ignoreMenu = false);
    Client* findAutogroupCandidate() const;

protected:
    virtual void debug(QDebug& stream) const;
    virtual bool shouldUnredirect() const;

private slots:
    void delayedSetShortcut();
    void performMoveResize();
    void removeSyncSupport();
    void pingTimeout();
    void demandAttentionKNotify();
    void repaintDecorationPending();

    //Signals for the scripting interface
    //Signals make an excellent way for communication
    //in between objects as compared to simple function
    //calls
signals:
    void clientManaging(KWin::Client*);
    void clientFullScreenSet(KWin::Client*, bool, bool);
    void clientMaximizedStateChanged(KWin::Client*, KDecorationDefines::MaximizeMode);
    void clientMaximizedStateChanged(KWin::Client* c, bool h, bool v);
    void clientMinimized(KWin::Client* client, bool animate);
    void clientUnminimized(KWin::Client* client, bool animate);
    void clientStartUserMovedResized(KWin::Client*);
    void clientStepUserMovedResized(KWin::Client *, const QRect&);
    void clientFinishUserMovedResized(KWin::Client*);
    void activeChanged();
    void captionChanged();
    void desktopChanged();
    void fullScreenChanged();
    void transientChanged();
    void modalChanged();
    void shadeChanged();
    void keepAboveChanged();
    void keepBelowChanged();
    void minimizedChanged();
    void moveResizedChanged();
    void iconChanged();
    void skipSwitcherChanged();
    void skipTaskbarChanged();
    void skipPagerChanged();
    /**
     * Emitted whenever the Client's TabGroup changed. That is whenever the Client is moved to
     * another group, but not when a Client gets added or removed to the Client's ClientGroup.
     **/
    void tabGroupChanged();
    /**
     * Emitted whenever the demands attention state changes.
     **/
    void demandsAttentionChanged();
    /**
     * Emitted whenever the Client's block compositing state changes.
     **/
    void blockingCompositingChanged();

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
    void resizeDecoration(const QSize& s);

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

    void checkOffscreenPosition (QRect* geom, const QRect& screenArea);

    void updateInputWindow();

    bool tabTo(Client *other, bool behind, bool activate);

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
    int moveResizeStartScreen;
    static bool s_haveResizeEffect;

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
    Client *shade_below;
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
    QTimer* autoRaiseTimer;
    QTimer* shadeHoverTimer;
    QTimer* delayedMoveResizeTimer;
    Colormap cmap;
    QString cap_normal, cap_iconic, cap_suffix;
    Group* in_group;
    Window window_group;
    TabGroup* tab_group;
    Layer in_layer;
    QTimer* ping_timer;
    qint64 m_killHelperPID;
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
    struct {
        XSyncCounter counter;
        XSyncValue value;
        XSyncAlarm alarm;
        QTimer *timeout, *failsafeTimeout;
        bool isPending;
    } syncRequest;
#endif
    int border_left, border_right, border_top, border_bottom;
    int padding_left, padding_right, padding_top, padding_bottom;
    QRegion _mask;
    static bool check_active_modal; ///< \see Client::checkActiveModal()
    KShortcut _shortcut;
    int sm_stacking_order;
    friend struct FetchNameInternalPredicate;
    friend struct ResetupRulesProcedure;
    friend class GeometryUpdatesBlocker;
    QTimer* demandAttentionKNotifyTimer;
    QPixmap decorationPixmapLeft, decorationPixmapRight, decorationPixmapTop, decorationPixmapBottom;
    // we (instead of Qt) initialize the Pixmaps, and have to free them
    bool m_responsibleForDecoPixmap;
    PaintRedirector* paintRedirector;
    TabBox::TabBoxClientImpl* m_tabBoxClient;
    bool m_firstInTabBox;

    bool electricMaximizing;
    QuickTileMode electricMode;

    friend bool performTransiencyCheck();

    void checkActivities();
    bool activitiesDefined; //whether the x property was actually set

    bool needsSessionInteract;

    Window input_window;
    QPoint input_offset;
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

inline TabGroup* Client::tabGroup() const
{
    return tab_group;
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
           (!tabGroup() || tabGroup()->current() == this);
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

inline Client::MaximizeMode Client::maximizeMode() const
{
    return max_mode;
}

inline KWin::QuickTileMode Client::quickTileMode() const
{
    return (KWin::QuickTileMode)quick_tile_mode;
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

inline void Client::setGeometry(const QRect& r, ForceGeometry_t force)
{
    setGeometry(r.x(), r.y(), r.width(), r.height(), force);
}

inline void Client::move(const QPoint& p, ForceGeometry_t force)
{
    move(p.x(), p.y(), force);
}

inline void Client::plainResize(const QSize& s, ForceGeometry_t force)
{
    plainResize(s.width(), s.height(), force);
}

inline void Client::resizeWithChecks(const QSize& s, ForceGeometry_t force)
{
    resizeWithChecks(s.width(), s.height(), force);
}

inline bool Client::hasUserTimeSupport() const
{
    return info->userTime() != -1U;
}

inline const WindowRules* Client::rules() const
{
    return &client_rules;
}

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
KWIN_COMPARE_PREDICATE(InputIdMatchPredicate, Client, Window, cl->inputId() == value);

} // namespace
Q_DECLARE_METATYPE(KWin::Client*)
Q_DECLARE_METATYPE(QList<KWin::Client*>)

#endif
