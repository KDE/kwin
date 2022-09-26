/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// kwin
#include "options.h"
#include "sm.h"
#include "utils/common.h"
// Qt
#include <QStringList>
#include <QTimer>
#include <QVector>
// std
#include <functional>
#include <memory>

class KConfig;
class KConfigGroup;
class KStartupInfo;
class KStartupInfoData;
class KStartupInfoId;

namespace KWin
{

namespace Decoration
{
class DecorationBridge;
}

namespace Xcb
{
class Tree;
class Window;
}

namespace TabBox
{
class TabBox;
}

class Window;
class Output;
class ColorMapper;
class Compositor;
class Deleted;
class Group;
class InternalWindow;
class KillWindow;
class ShortcutDialog;
class Unmanaged;
class UserActionsMenu;
class VirtualDesktop;
class X11Window;
class X11EventFilter;
class FocusChain;
class ApplicationMenu;
class PlacementTracker;
enum class Predicate;
class Outline;
class RuleBook;
class ScreenEdges;
#if KWIN_BUILD_ACTIVITIES
class Activities;
#endif
class PlaceholderInputEventFilter;
class PlaceholderOutput;
class Placement;
class OutputConfiguration;
class TileManager;

class KWIN_EXPORT Workspace : public QObject
{
    Q_OBJECT
public:
    explicit Workspace();
    ~Workspace() override;

    static Workspace *self()
    {
        return _self;
    }

    bool workspaceEvent(xcb_generic_event_t *);

    bool hasWindow(const Window *);

    /**
     * @brief Finds the first Client matching the condition expressed by passed in @p func.
     *
     * Internally findClient uses the std::find_if algorithm and that determines how the function
     * needs to be implemented. An example usage for finding a Client with a matching windowId
     * @code
     * xcb_window_t w; // our test window
     * X11Window *client = findClient([w](const X11Window *c) -> bool {
     *     return c->window() == w;
     * });
     * @endcode
     *
     * For the standard cases of matching the window id with one of the Client's windows use
     * the simplified overload method findClient(Predicate, xcb_window_t). Above example
     * can be simplified to:
     * @code
     * xcb_window_t w; // our test window
     * X11Window *client = findClient(Predicate::WindowMatch, w);
     * @endcode
     *
     * @param func Unary function that accepts a X11Window *as argument and
     * returns a value convertible to bool. The value returned indicates whether the
     * X11Window *is considered a match in the context of this function.
     * The function shall not modify its argument.
     * This can either be a function pointer or a function object.
     * @return KWin::X11Window *The found Client or @c null
     * @see findClient(Predicate, xcb_window_t)
     */
    X11Window *findClient(std::function<bool(const X11Window *)> func) const;
    Window *findAbstractClient(std::function<bool(const Window *)> func) const;
    /**
     * @brief Finds the Client matching the given match @p predicate for the given window.
     *
     * @param predicate Which window should be compared
     * @param w The window id to test against
     * @return KWin::X11Window *The found Client or @c null
     * @see findClient(std::function<bool (const X11Window *)>)
     */
    X11Window *findClient(Predicate predicate, xcb_window_t w) const;
    void forEachClient(std::function<void(X11Window *)> func);
    void forEachAbstractClient(std::function<void(Window *)> func);
    Unmanaged *findUnmanaged(std::function<bool(const Unmanaged *)> func) const;
    /**
     * @brief Finds the Unmanaged with the given window id.
     *
     * @param w The window id to search for
     * @return KWin::Unmanaged* Found Unmanaged or @c null if there is no Unmanaged with given Id.
     */
    Unmanaged *findUnmanaged(xcb_window_t w) const;
    void forEachUnmanaged(std::function<void(Unmanaged *)> func);
    Window *findToplevel(std::function<bool(const Window *)> func) const;
    void forEachToplevel(std::function<void(Window *)> func);

    Window *findToplevel(const QUuid &internalId) const;

    /**
     * @brief Finds a Window for the internal window @p w.
     *
     * Internal window means a window created by KWin itself. On X11 this is an Unmanaged
     * and mapped by the window id, on Wayland a XdgShellClient mapped on the internal window id.
     *
     * @returns Window
     */
    Window *findInternal(QWindow *w) const;

    QRectF clientArea(clientAreaOption, const Output *output, const VirtualDesktop *desktop) const;
    QRectF clientArea(clientAreaOption, const Window *window) const;
    QRectF clientArea(clientAreaOption, const Window *window, const Output *output) const;
    QRectF clientArea(clientAreaOption, const Window *window, const QPointF &pos) const;

    /**
     * Returns the geometry of this Workspace, i.e. the bounding rectangle of all outputs.
     */
    QRect geometry() const;
    StrutRects restrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas = StrutAreaAll) const;

    bool initializing() const;

    Output *xineramaIndexToOutput(int index) const;

    void setOutputOrder(const QVector<Output *> &order);
    QVector<Output *> outputOrder() const;

    Output *activeOutput() const;
    void setActiveOutput(Output *output);
    void setActiveOutput(const QPointF &pos);
    void setActiveCursorOutput(Output *output);
    void setActiveCursorOutput(const QPointF &pos);

    /**
     * Returns the active window, i.e. the window that has the focus (or None
     * if no window has the focus)
     */
    Window *activeWindow() const;
    /**
     * Window that was activated, but it's not yet really activeWindow(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    Window *mostRecentlyActivatedWindow() const;

    Window *windowUnderMouse(Output *output) const;

    void activateWindow(Window *window, bool force = false);
    bool requestFocus(Window *window, bool force = false);
    enum ActivityFlag {
        ActivityFocus = 1 << 0, // focus the window
        ActivityFocusForce = 1 << 1 | ActivityFocus, // focus even if Dock etc.
        ActivityRaise = 1 << 2 // raise the window
    };
    Q_DECLARE_FLAGS(ActivityFlags, ActivityFlag)
    bool takeActivity(Window *window, ActivityFlags flags);
    bool restoreFocus();
    void gotFocusIn(const Window *window);
    void setShouldGetFocus(Window *window);
    bool activateNextWindow(Window *window);
    bool focusChangeEnabled()
    {
        return block_focus == 0;
    }

    /**
     * Indicates that the given @a window is being moved or resized by the user.
     */
    void setMoveResizeWindow(Window *window);

    QRectF adjustClientArea(Window *window, const QRectF &area) const;
    QPointF adjustWindowPosition(Window *window, QPointF pos, bool unrestricted, double snapAdjust = 1.0);
    QRectF adjustWindowSize(Window *window, QRectF moveResizeGeom, Gravity gravity);
    void raiseWindow(Window *window, bool nogroup = false);
    void lowerWindow(Window *window, bool nogroup = false);
    void raiseWindowRequest(Window *window, NET::RequestSource src = NET::FromApplication, xcb_timestamp_t timestamp = 0);
    void lowerWindowRequest(X11Window *window, NET::RequestSource src, xcb_timestamp_t timestamp);
    void lowerWindowRequest(Window *window);
    void restackWindowUnderActive(Window *window);
    void restack(Window *window, Window *under, bool force = false);
    void raiseOrLowerWindow(Window *window);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(X11Window *window);
    void updateStackingOrder(bool propagate_new_windows = false);
    void forceRestacking();

    void constrain(Window *below, Window *above);
    void unconstrain(Window *below, Window *above);

    void windowHidden(Window *);
    void windowAttentionChanged(Window *, bool set);

    /**
     * @return List of windows currently managed by Workspace
     */
    const QList<X11Window *> &clientList() const
    {
        return m_x11Clients;
    }
    /**
     * @return List of unmanaged "windows" currently registered in Workspace
     */
    const QList<Unmanaged *> &unmanagedList() const
    {
        return m_unmanaged;
    }
    /**
     * @return List of deleted "windows" currently managed by Workspace
     */
    const QList<Deleted *> &deletedList() const
    {
        return deleted;
    }
    /**
     * @returns List of all windows (either X11 or Wayland) currently managed by Workspace
     */
    const QList<Window *> allClientList() const
    {
        return m_allClients;
    }

    /**
     * @returns List of all internal windows currently managed by Workspace
     */
    const QList<InternalWindow *> &internalWindows() const
    {
        return m_internalWindows;
    }

    void stackScreenEdgesUnderOverrideRedirect();

    SessionManager *sessionManager() const;

    /**
     * @returns the TileManager associated to a given output
     */
    TileManager *tileManager(Output *output);

public:
    QPoint cascadeOffset(const Window *c) const;

private:
    QTimer *m_quickTileCombineTimer;
    QuickTileMode m_lastTilingMode;

    //-------------------------------------------------
    // Unsorted

public:
    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    StrutRects previousRestrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas = StrutAreaAll) const;
    QHash<const Output *, QRect> previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    /**
     * Returns the list of windows sorted in stacking order, with topmost window
     * at the last position
     */
    const QList<Window *> &stackingOrder() const;
    QList<Window *> unconstrainedStackingOrder() const;
    QList<X11Window *> ensureStackingOrder(const QList<X11Window *> &windows) const;
    QList<Window *> ensureStackingOrder(const QList<Window *> &windows) const;

    Window *topWindowOnDesktop(VirtualDesktop *desktop, Output *output = nullptr, bool unconstrained = false,
                               bool only_normal = true) const;
    Window *findDesktop(bool topmost, VirtualDesktop *desktop) const;
    void sendWindowToDesktop(Window *window, int desktop, bool dont_activate);
    void windowToPreviousDesktop(Window *window);
    void windowToNextDesktop(Window *window);
    void sendWindowToOutput(Window *window, Output *output);

    void addManualOverlay(xcb_window_t id)
    {
        manual_overlays << id;
    }
    void removeManualOverlay(xcb_window_t id)
    {
        manual_overlays.removeOne(id);
    }

    /**
     * Shows the menu operations menu for the window and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect &pos, Window *cl);
    const UserActionsMenu *userActionsMenu() const
    {
        return m_userActionsMenu;
    }

    void showApplicationMenu(const QRect &pos, Window *window, int actionId);

    void updateMinimizedOfTransients(Window *);
    void updateOnAllDesktopsOfTransients(Window *);
    void checkTransients(xcb_window_t w);

    SessionInfo *takeSessionInfo(X11Window *);

    // D-Bus interface
    QString supportInformation() const;

    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest,
        DirectionPrev,
        DirectionNext
    };
    Output *findOutput(Output *reference, Direction direction, bool wrapAround = false) const;
    void switchToOutput(Output *output);

    QList<Output *> outputs() const;
    Output *outputAt(const QPointF &pos) const;

    /**
     * Set "Show Desktop" status
     *
     * @param showing @c true to show the desktop, @c false to restore the window positions
     * @param animated @c true if the "Show Desktop Animation" should be played, otherwise @c false
     */
    void setShowingDesktop(bool showing, bool animated = true);
    bool showingDesktop() const;

    void removeX11Window(X11Window *); // Only called from X11Window::destroyWindow() or X11Window::releaseWindow()
    void setActiveWindow(Window *window);
    Group *findGroup(xcb_window_t leader) const;
    void addGroup(Group *group);
    void removeGroup(Group *group);
    Group *findClientLeaderGroup(const X11Window *c) const;
    int unconstainedStackingOrderIndex(const X11Window *c) const;

    void removeUnmanaged(Unmanaged *); // Only called from Unmanaged::release()
    void removeDeleted(Deleted *);
    void addDeleted(Deleted *, Window *);

    bool checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data);

    void focusToNull(); // SELI TODO: Public?

    void windowShortcutUpdated(Window *window);
    bool shortcutAvailable(const QKeySequence &cut, Window *ignore = nullptr) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcutsForClient(bool disable);

    void setWasUserInteraction();
    bool wasUserInteraction() const;

    qreal packPositionLeft(const Window *window, qreal oldX, bool leftEdge) const;
    qreal packPositionRight(const Window *window, qreal oldX, bool rightEdge) const;
    qreal packPositionUp(const Window *window, qreal oldY, bool topEdge) const;
    qreal packPositionDown(const Window *window, qreal oldY, bool bottomEdge) const;

    void cancelDelayFocus();
    void requestDelayFocus(Window *);

    /**
     * updates the mouse position to track whether a focus follow mouse focus change was caused by
     * an actual mouse move
     * is esp. called on enter/motion events of inactive windows
     * since an active window doesn't receive mouse events, it must also be invoked if a (potentially)
     * active window might be moved/resize away from the cursor (causing a leave event)
     */
    void updateFocusMousePosition(const QPointF &pos);
    QPointF focusMousePosition() const;

    /**
     * Returns a window that is currently being moved or resized by the user.
     *
     * If none of windows is being moved or resized, @c null will be returned.
     */
    Window *moveResizeWindow()
    {
        return m_moveResizeWindow;
    }

    void quickTileWindow(QuickTileMode mode);
    void switchWindow(Direction direction);

    ShortcutDialog *shortcutDialog() const
    {
        return m_windowKeysDialog;
    }

    void addInternalWindow(InternalWindow *window);
    void removeInternalWindow(InternalWindow *window);

    /**
     * @internal
     * Used by session management
     */
    void setInitialDesktop(int desktop);

    bool inShouldGetFocus(Window *w) const
    {
        return should_get_focus.contains(w);
    }
    Window *lastActiveWindow() const
    {
        return m_lastActiveWindow;
    }
    FocusChain *focusChain() const;
    ApplicationMenu *applicationMenu() const;
    Decoration::DecorationBridge *decorationBridge() const;
    Outline *outline() const;
    Placement *placement() const;
    RuleBook *rulebook() const;
    ScreenEdges *screenEdges() const;
#if KWIN_BUILD_TABBOX
    TabBox::TabBox *tabbox() const;
#endif
#if KWIN_BUILD_ACTIVITIES
    Activities *activities() const;
#endif

    /**
     * Apply the requested output configuration. Note that you must use this function
     * instead of Platform::applyOutputChanges().
     */
    bool applyOutputConfiguration(const OutputConfiguration &config, const QVector<Output *> &outputOrder = {});

public Q_SLOTS:
    void performWindowOperation(KWin::Window *window, Options::WindowOperation op);
    // Keybindings
    // void slotSwitchToWindow( int );
    void slotWindowToDesktop(VirtualDesktop *desktop);

    // void slotWindowToListPosition( int );
    void slotSwitchToScreen(Output *output);
    void slotWindowToScreen(Output *output);
    void slotSwitchToLeftScreen();
    void slotSwitchToRightScreen();
    void slotSwitchToAboveScreen();
    void slotSwitchToBelowScreen();
    void slotSwitchToPrevScreen();
    void slotSwitchToNextScreen();
    void slotWindowToLeftScreen();
    void slotWindowToRightScreen();
    void slotWindowToAboveScreen();
    void slotWindowToBelowScreen();
    void slotWindowToNextScreen();
    void slotWindowToPrevScreen();

    void slotToggleShowDesktop();

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowMinimize();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();
    void slotWindowRaiseOrLower();
    void slotActivateAttentionWindow();

    void slotWindowCenter();

    void slotWindowMoveLeft();
    void slotWindowMoveRight();
    void slotWindowMoveUp();
    void slotWindowMoveDown();
    void slotWindowExpandHorizontal();
    void slotWindowExpandVertical();
    void slotWindowShrinkHorizontal();
    void slotWindowShrinkVertical();

    void slotIncreaseWindowOpacity();
    void slotLowerWindowOpacity();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();
    void slotWindowAbove();
    void slotWindowBelow();
    void slotWindowOnAllDesktops();
    void slotWindowFullScreen();
    void slotWindowNoBorder();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();
    void slotWindowToDesktopRight();
    void slotWindowToDesktopLeft();
    void slotWindowToDesktopUp();
    void slotWindowToDesktopDown();

    void reconfigure();
    void slotReconfigure();

    void slotKillWindow();

    void slotSetupWindowShortcut();
    void setupWindowShortcutDone(bool);

    void updateClientArea();

private Q_SLOTS:
    void desktopResized();
    void selectWmInputEventMask();
    void slotUpdateToolWindows();
    void delayFocus();
    void slotReloadConfig();
    void updateCurrentActivity(const QString &new_activity);
    // virtual desktop handling
    void slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop);
    void slotCurrentDesktopChanging(uint currentDesktop, QPointF delta);
    void slotCurrentDesktopChangingCancelled();
    void slotDesktopAdded(VirtualDesktop *desktop);
    void slotDesktopRemoved(VirtualDesktop *desktop);
    void slotOutputBackendOutputsQueried();

Q_SIGNALS:
    /**
     * Emitted after the Workspace has setup the complete initialization process.
     * This can be used to connect to for performing post-workspace initialization.
     */
    void workspaceInitialized();
    void geometryChanged();

    // Signals required for the scripting interface
    void desktopPresenceChanged(KWin::Window *, int);
    void currentActivityChanged();
    void currentDesktopChanged(int, KWin::Window *);
    void currentDesktopChanging(uint currentDesktop, QPointF delta, KWin::Window *); // for realtime animations
    void currentDesktopChangingCancelled();
    void windowAdded(KWin::Window *);
    void windowRemoved(KWin::Window *);
    void windowActivated(KWin::Window *);
    void windowDemandsAttentionChanged(KWin::Window *, bool);
    void windowMinimizedChanged(KWin::Window *);
    void groupAdded(KWin::Group *);
    void unmanagedAdded(KWin::Unmanaged *);
    void unmanagedRemoved(KWin::Unmanaged *);
    void deletedRemoved(KWin::Deleted *);
    void configChanged();
    void showingDesktopChanged(bool showing, bool animated);
    void outputOrderChanged();
    void outputAdded(KWin::Output *);
    void outputRemoved(KWin::Output *);
    void outputsChanged();
    /**
     * This signal is emitted when the stacking order changed, i.e. a window is risen
     * or lowered
     */
    void stackingOrderChanged();

    /**
     * This signal is emitted whenever an internal window is created.
     */
    void internalWindowAdded(KWin::InternalWindow *window);

    /**
     * This signal is emitted whenever an internal window gets removed.
     */
    void internalWindowRemoved(KWin::InternalWindow *window);

private:
    void init();
    void initializeX11();
    void cleanupX11();
    void initShortcuts();
    template<typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, Slot slot);
    template<typename T, typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot);
    void setupWindowShortcut(Window *window);
    bool switchWindow(Window *window, Direction direction, QPoint curPos, VirtualDesktop *desktop);

    void propagateWindows(bool propagate_new_windows); // Called only from updateStackingOrder
    QList<Window *> constrainedStackingOrder();
    void raiseWindowWithinApplication(Window *window);
    void lowerWindowWithinApplication(Window *window);
    bool allowFullClientRaising(const Window *window, xcb_timestamp_t timestamp);
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    void saveOldScreenSizes();
    void addToStack(Window *window);
    void replaceInStack(Window *original, Deleted *deleted);
    void removeFromStack(Window *window);

    /// This is the right way to create a new X11 window
    X11Window *createX11Window(xcb_window_t windowId, bool is_mapped);
    void addX11Window(X11Window *c);
    void setupWindowConnections(Window *window);
    Unmanaged *createUnmanaged(xcb_window_t windowId);
    void addUnmanaged(Unmanaged *c);

    void addWaylandWindow(Window *window);
    void removeWaylandWindow(Window *window);

    //---------------------------------------------------------------------

    void closeActivePopup();
    void updateWindowVisibilityOnDesktopChange(VirtualDesktop *newDesktop);
    void activateWindowOnNewDesktop(VirtualDesktop *desktop);
    Window *findWindowToActivateOnDesktop(VirtualDesktop *desktop);
    void removeWindow(Window *window);
    QString getPlacementTrackerHash();

    void updateOutputConfiguration();
    void updateOutputs(const QVector<Output *> &outputOrder = {});

    struct Constraint
    {
        Window *below;
        Window *above;
        // All constraints above our "below" window
        QList<Constraint *> parents;
        // All constraints below our "above" window
        QList<Constraint *> children;
        // Used to prevent cycles.
        bool enqueued = false;
    };

    QList<Constraint *> m_constraints;
    QWidget *active_popup;
    Window *m_activePopupWindow;

    int m_initialDesktop;
    void updateXStackingOrder();
    void updateTabbox();

    QList<Output *> m_outputs;
    Output *m_activeOutput = nullptr;
    Output *m_activeCursorOutput = nullptr;
    QString m_outputsHash;
    QVector<Output *> m_outputOrder;

    Window *m_activeWindow;
    Window *m_lastActiveWindow;
    Window *m_moveResizeWindow;

    // Delay(ed) window focus timer and window
    QTimer *delayFocusTimer;
    Window *m_delayFocusWindow;
    QPointF focusMousePos;

    QList<X11Window *> m_x11Clients;
    QList<Window *> m_allClients;
    QList<Unmanaged *> m_unmanaged;
    QList<Deleted *> deleted;
    QList<InternalWindow *> m_internalWindows;

    QList<Window *> unconstrained_stacking_order; // Topmost last
    QList<Window *> stacking_order; // Topmost last
    QVector<xcb_window_t> manual_overlays; // Topmost last
    bool force_restacking;
    QList<Window *> should_get_focus; // Last is most recent
    QList<Window *> attention_chain;

    bool showing_desktop;

    QList<Group *> groups;

    bool was_user_interaction;
    std::unique_ptr<X11EventFilter> m_wasUserInteractionFilter;

    int block_focus;

    /**
     * Holds the menu containing the user actions which is shown
     * on e.g. right click the window decoration.
     */
    UserActionsMenu *m_userActionsMenu;

    void modalActionsSwitch(bool enabled);

    ShortcutDialog *m_windowKeysDialog = nullptr;
    Window *m_windowKeysWindow = nullptr;
    bool m_globalShortcutsDisabledForWindow = false;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace *_self;

    std::unique_ptr<KStartupInfo> m_startup;
    std::unique_ptr<ColorMapper> m_colorMapper;

    QHash<const VirtualDesktop *, QRectF> m_workAreas;
    QHash<const VirtualDesktop *, StrutRects> m_restrictedAreas;
    QHash<const VirtualDesktop *, QHash<const Output *, QRectF>> m_screenAreas;
    QRect m_geometry;

    QHash<const Output *, QRect> m_oldScreenGeometries;
    QSize olddisplaysize; // previous sizes od displayWidth()/displayHeight()
    QHash<const VirtualDesktop *, StrutRects> m_oldRestrictedAreas;
    bool m_inUpdateClientArea = false;

    int m_setActiveWindowRecursion = 0;
    int m_blockStackingUpdates = 0; // When > 0, stacking updates are temporarily disabled
    bool m_blockedPropagatingNewWindows; // Propagate also new windows after enabling stacking updates?
    std::unique_ptr<Xcb::Window> m_nullFocus;
    friend class StackingUpdatesBlocker;

    std::unique_ptr<KillWindow> m_windowKiller;
    std::unique_ptr<X11EventFilter> m_movingClientFilter;
    std::unique_ptr<X11EventFilter> m_syncAlarmFilter;

    SessionManager *m_sessionManager;
    std::unique_ptr<FocusChain> m_focusChain;
    std::unique_ptr<ApplicationMenu> m_applicationMenu;
    std::unique_ptr<Decoration::DecorationBridge> m_decorationBridge;
    std::unique_ptr<Outline> m_outline;
    std::unique_ptr<Placement> m_placement;
    std::unique_ptr<RuleBook> m_rulebook;
    std::unique_ptr<ScreenEdges> m_screenEdges;
#if KWIN_BUILD_TABBOX
    std::unique_ptr<TabBox::TabBox> m_tabbox;
#endif
#if KWIN_BUILD_ACTIVITIES
    std::unique_ptr<Activities> m_activities;
#endif
    std::unique_ptr<PlacementTracker> m_placementTracker;

    PlaceholderOutput *m_placeholderOutput = nullptr;
    std::unique_ptr<PlaceholderInputEventFilter> m_placeholderFilter;
    std::map<Output *, std::unique_ptr<TileManager>> m_tileManagers;

private:
    friend bool performTransiencyCheck();
    friend Workspace *workspace();
};

/**
 * Helper for Workspace::blockStackingUpdates() being called in pairs (True/false)
 */
class StackingUpdatesBlocker
{
public:
    explicit StackingUpdatesBlocker(Workspace *w)
        : ws(w)
    {
        ws->blockStackingUpdates(true);
    }
    ~StackingUpdatesBlocker()
    {
        ws->blockStackingUpdates(false);
    }

private:
    Workspace *ws;
};

class ColorMapper : public QObject
{
    Q_OBJECT
public:
    ColorMapper(QObject *parent);
    ~ColorMapper() override;
public Q_SLOTS:
    void update();

private:
    xcb_colormap_t m_default;
    xcb_colormap_t m_installed;
};

//---------------------------------------------------------
// Unsorted

inline QList<Output *> Workspace::outputs() const
{
    return m_outputs;
}

inline Window *Workspace::activeWindow() const
{
    return m_activeWindow;
}

inline Window *Workspace::mostRecentlyActivatedWindow() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : m_activeWindow;
}

inline void Workspace::addGroup(Group *group)
{
    Q_EMIT groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group *group)
{
    groups.removeAll(group);
}

inline const QList<Window *> &Workspace::stackingOrder() const
{
    // TODO: Q_ASSERT( block_stacking_updates == 0 );
    return stacking_order;
}

inline bool Workspace::wasUserInteraction() const
{
    return was_user_interaction;
}

inline SessionManager *Workspace::sessionManager() const
{
    return m_sessionManager;
}

inline bool Workspace::showingDesktop() const
{
    return showing_desktop;
}

inline bool Workspace::globalShortcutsDisabled() const
{
    return m_globalShortcutsDisabledForWindow;
}

inline void Workspace::forceRestacking()
{
    force_restacking = true;
    StackingUpdatesBlocker blocker(this); // Do restacking if not blocked
}

inline void Workspace::updateFocusMousePosition(const QPointF &pos)
{
    focusMousePos = pos;
}

inline QPointF Workspace::focusMousePosition() const
{
    return focusMousePos;
}

inline void Workspace::forEachClient(std::function<void(X11Window *)> func)
{
    std::for_each(m_x11Clients.constBegin(), m_x11Clients.constEnd(), func);
}

inline void Workspace::forEachUnmanaged(std::function<void(Unmanaged *)> func)
{
    std::for_each(m_unmanaged.constBegin(), m_unmanaged.constEnd(), func);
}

inline Workspace *workspace()
{
    return Workspace::_self;
}

} // namespace
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Workspace::ActivityFlags)
