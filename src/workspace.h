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
#include "utils/filedescriptor.h"
#include "utils/serial.h"
// KF
#include <netwm_def.h>
// Qt
#include <QList>
#include <QStringList>
#include <QTimer>
// std
#include <functional>
#include <memory>

class KConfig;
class KConfigGroup;
class KStartupInfo;
class KStartupInfoData;
class KStartupInfoId;
class QLightSensor;

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

class Gravity;
class Window;
class LogicalOutput;
class Compositor;
class Group;
class InternalWindow;
class KillWindow;
class ShortcutDialog;
class UserActionsMenu;
class VirtualDesktop;
class X11Window;
class X11EventFilter;
class FocusChain;
class ApplicationMenu;
class PlacementTracker;
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
class RootTile;
class TileManager;
class OutputConfigurationStore;
class LidSwitchTracker;
class DpmsInputEventFilter;
class OrientationSensor;
class BrightnessDevice;
class BackendOutput;
class LightSensor;

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

    bool hasWindow(const Window *);

#if KWIN_BUILD_X11
    bool workspaceEvent(xcb_generic_event_t *);

    UInt32Serial x11FocusSerial() const;
    void setX11FocusSerial(UInt32Serial serial);

    X11Window *findClient(std::function<bool(const X11Window *)> func) const;
    X11Window *findClient(xcb_window_t w) const;
    void forEachClient(std::function<void(X11Window *)> func);
    X11Window *findUnmanaged(std::function<bool(const X11Window *)> func) const;
    X11Window *findUnmanaged(xcb_window_t w) const;
#endif

    Window *findWindow(const QUuid &internalId) const;
    Window *findWindow(std::function<bool(const Window *)> func) const;
    void forEachWindow(std::function<void(Window *)> func);

    /**
     * @brief Finds a Window for the internal window @p w.
     *
     * Internal window means a window created by KWin itself. On X11 this is an Unmanaged
     * and mapped by the window id, on Wayland a XdgShellClient mapped on the internal window id.
     *
     * @returns Window
     */
    Window *findInternal(QWindow *w) const;

    RectF clientArea(clientAreaOption, const LogicalOutput *output) const;
    RectF clientArea(clientAreaOption, const Window *window) const;
    RectF clientArea(clientAreaOption, const Window *window, const LogicalOutput *output) const;
    RectF clientArea(clientAreaOption, const Window *window, const QPointF &pos) const;

    /**
     * Returns the geometry of this Workspace, i.e. the bounding rectangle of all outputs.
     */
    Rect geometry() const;
    StrutRects restrictedMoveArea(StrutAreas areas = StrutAreaAll) const;

    bool initializing() const;

    LogicalOutput *xineramaIndexToOutput(int index) const;

    QList<LogicalOutput *> outputOrder() const;

    LogicalOutput *activeOutput() const;
    void setActiveOutput(LogicalOutput *output);
    void setActiveOutput(const QPointF &pos);

    /**
     * Returns the active window, i.e. the window that has the focus (or None
     * if no window has the focus)
     */
    Window *activeWindow() const;

    Window *windowUnderMouse(LogicalOutput *output) const;

    void activateWindow(Window *window, bool force = false);
    bool requestFocus(Window *window, bool force = false);
    void resetFocus();
    bool restoreFocus();
    void activateNextWindow(Window *window);
    bool focusChangeEnabled()
    {
        return block_focus == 0;
    }

    /**
     * Indicates that the given @a window is being moved or resized by the user.
     */
    void setMoveResizeWindow(Window *window);

    RectF adjustClientArea(Window *window, const RectF &area) const;
    QPointF adjustWindowPosition(const Window *window, QPointF pos, bool unrestricted, double snapAdjust = 1.0) const;
    RectF adjustWindowSize(const Window *window, RectF moveResizeGeom, Gravity gravity) const;
    void raiseWindow(Window *window, bool nogroup = false);
    void lowerWindow(Window *window, bool nogroup = false);
#if KWIN_BUILD_X11
    void raiseWindowRequest(Window *window, NET::RequestSource src = NET::FromApplication, uint32_t timestamp = 0);
    void lowerWindowRequest(X11Window *window, NET::RequestSource src, xcb_timestamp_t timestamp);
    void restoreSessionStackingOrder(X11Window *window);
#endif
    void restackWindowUnderActive(Window *window);
    void stackBelow(Window *window, Window *reference);
    void stackAbove(Window *window, Window *reference);
    void raiseOrLowerWindow(Window *window);
    void updateStackingOrder(bool propagate_new_windows = false);
    void forceRestacking();

    void constrain(Window *below, Window *above);
    void unconstrain(Window *below, Window *above);

    void windowAttentionChanged(Window *, bool set);

    /**
     * @returns List of all windows (either X11 or Wayland) currently managed by Workspace
     */
    QList<Window *> windows() const
    {
        return m_windows;
    }

    SessionManager *sessionManager() const;

    /**
     * @returns the TileManager associated to a given output
     */
    TileManager *tileManager(LogicalOutput *output) const;

    /**
     * Returns the root tile for the given @a output on the current virtual desktop.
     */
    RootTile *rootTile(LogicalOutput *output) const;

    /**
     * Returns the root tile for the given @a output and @a desktop.
     */
    RootTile *rootTile(LogicalOutput *output, VirtualDesktop *desktop) const;

public:
    QPointF cascadeOffset(const RectF &area) const;

    //-------------------------------------------------
    // Unsorted

public:
    StrutRects previousRestrictedMoveArea(StrutAreas areas = StrutAreaAll) const;
    QHash<const LogicalOutput *, Rect> previousScreenSizes() const;

    /**
     * Returns @c true if the workspace is currently being rearranged; otherwise returns @c false.
     */
    bool inRearrange() const;

    /**
     * Re-arranges the workspace, it includes computing restricted areas, moving windows out of the
     * restricted areas, and so on.
     *
     * The client area is the area that is available for windows (that which is not taken by windows
     * like panels, the top-of-screen menu etc).
     *
     * @see clientArea()
     */
    void rearrange(const QHash<Window *, LogicalOutput *> &oldOutputs = {});

    /**
     * Schedules the workspace to be re-arranged at the next available opportunity.
     */
    void scheduleRearrange();

    /**
     * Returns the list of windows sorted in stacking order, with topmost window
     * at the last position
     */
    const QList<Window *> &stackingOrder() const;
    QList<Window *> unconstrainedStackingOrder() const;
    QList<Window *> ensureStackingOrder(const QList<Window *> &windows) const;

    Window *topWindowOnDesktop(VirtualDesktop *desktop, LogicalOutput *output = nullptr, bool unconstrained = false,
                               bool only_normal = true) const;
    Window *findDesktop(VirtualDesktop *desktop, LogicalOutput *output) const;
    void addWindowToDesktop(Window *window, VirtualDesktop *desktop);
    void removeWindowFromDesktop(Window *window, VirtualDesktop *desktop);
    void sendWindowToDesktops(Window *window, const QList<VirtualDesktop *> &desktops, bool dont_activate);
    void windowToPreviousDesktop(Window *window);
    void windowToNextDesktop(Window *window);

#if KWIN_BUILD_X11
    QList<X11Window *> ensureStackingOrder(const QList<X11Window *> &windows) const;

    void addManualOverlay(xcb_window_t id)
    {
        manual_overlays << id;
    }
    void removeManualOverlay(xcb_window_t id)
    {
        manual_overlays.removeOne(id);
    }
#endif

    /**
     * Shows the window menu and makes it active if it's not already.
     */
    void showWindowMenu(const Rect &pos, Window *cl);
    UserActionsMenu *userActionsMenu() const
    {
        return m_userActionsMenu;
    }

    void showApplicationMenu(const Rect &pos, Window *window, int actionId);

    void updateMinimizedOfTransients(Window *);
    void updateOnAllDesktopsOfTransients(Window *);

#if KWIN_BUILD_X11
    void checkTransients(xcb_window_t w);
    SessionInfo *takeSessionInfo(X11Window *);
#endif

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
    LogicalOutput *findOutput(LogicalOutput *reference, Direction direction, bool wrapAround = false) const;
    LogicalOutput *findOutput(const QString &name) const;
    LogicalOutput *findOutput(BackendOutput *backendOutput) const;
    void switchToOutput(LogicalOutput *output);

    QString outputLayoutId() const;
    QList<LogicalOutput *> outputs() const;
    LogicalOutput *outputAt(const QPointF &pos) const;

    /**
     * Set "Show Desktop" status
     *
     * @param showing @c true to show the desktop, @c false to restore the window positions
     * @param animated @c true if the "Show Desktop Animation" should be played, otherwise @c false
     */
    void setShowingDesktop(bool showing, bool animated = true);
    bool showingDesktop() const;

    void setActiveWindow(Window *window);
#if KWIN_BUILD_X11
    void removeX11Window(X11Window *); // Only called from X11Window::destroyWindow() or X11Window::releaseWindow()
    Group *findGroup(xcb_window_t leader) const;
    void addGroup(Group *group);
    void removeGroup(Group *group);
    Group *findClientLeaderGroup(const X11Window *c) const;
    int unconstainedStackingOrderIndex(const X11Window *c) const;

    void removeUnmanaged(X11Window *);
    bool checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data);
#endif
    void removeDeleted(Window *);
    void addDeleted(Window *);

    void focusToNull(); // SELI TODO: Public?
#if KWIN_BUILD_X11
    /**
     * Returns the id of the null window. The null window is a special window that gets focused
     * when no other X11 window can be focused.
     *
     * If the root window gets focus, it serves us as a signal to activate any window of our choice.
     * In case no window can be focused, then the null window will be focused.
     */
    xcb_window_t nullFocusWindow() const;
#endif

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
    void customQuickTileWindow(QuickTileMode mode);
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
    OutputConfigurationError applyOutputConfiguration(OutputConfiguration &config);
    void updateXwaylandScale();

    void setActivationToken(const QString &token, UInt32Serial serial, const QString &appId);
    bool mayActivate(Window *window, const QString &token) const;

    enum class DpmsState {
        Off,
        TurningOff,
        AboutToTurnOff,
        On
    };
    void requestDpmsState(DpmsState state);
    DpmsState dpmsState() const;

public Q_SLOTS:
    void performWindowOperation(KWin::Window *window, Options::WindowOperation op);
    // Keybindings
    // void slotSwitchToWindow( int );
    void slotWindowToDesktop(VirtualDesktop *desktop);

    // void slotWindowToListPosition( int );
    void slotSwitchToScreen(LogicalOutput *output);
    void slotWindowToScreen(LogicalOutput *output);
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
    void slotWindowExcludeFromCapture();

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

    void slotEndInteractiveMoveResize();

private Q_SLOTS:
#if KWIN_BUILD_X11
    void selectWmInputEventMask();
#endif
    void delayFocus();
    void slotReloadConfig();
    void updateCurrentActivity(const QString &new_activity);
    // virtual desktop handling
    void slotCurrentDesktopChanged(VirtualDesktop *previousDesktop, VirtualDesktop *newDesktop);
    void slotCurrentDesktopChanging(VirtualDesktop *currentDesktop, QPointF delta);
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
    void currentActivityChanged();
    void currentDesktopChanged(KWin::VirtualDesktop *previousDesktop, KWin::Window *);
    void currentDesktopChanging(KWin::VirtualDesktop *currentDesktop, QPointF delta, KWin::Window *); // for realtime animations
    void currentDesktopChangingCancelled();
    void windowAdded(KWin::Window *);
    void windowRemoved(KWin::Window *);
    void windowActivated(KWin::Window *);
    void windowMinimizedChanged(KWin::Window *);
#if KWIN_BUILD_X11
    void groupAdded(KWin::Group *);
#endif
    void deletedRemoved(KWin::Window *);
    void configChanged();
    void showingDesktopChanged(bool showing, bool animated);
    void outputOrderChanged();
    void outputAdded(KWin::LogicalOutput *);
    void outputRemoved(KWin::LogicalOutput *);
    void outputsChanged();
    /**
     * This signal is emitted when the stacking order changed, i.e. a window is risen
     * or lowered
     */
    void stackingOrderChanged();
    void aboutToRearrange();
    void dpmsStateChanged(std::chrono::milliseconds animationTime);

private:
    void init();
    void initShortcuts();
    template<typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, Slot slot, bool autoRepeat);
    template<typename T, typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, bool autoRepeat);
    void setupWindowShortcut(Window *window);
    bool switchWindow(Window *window, Direction direction, QPoint curPos, VirtualDesktop *desktop);
    void desktopResized(const QHash<Window *, LogicalOutput *> &oldOutputs);

    QList<Window *> constrainedStackingOrder();
    bool areConstrained(const Window *below, const Window *above) const;
    void raiseWindowWithinApplication(Window *window);
    void lowerWindowWithinApplication(Window *window);
    bool allowFullClientRaising(const Window *window, uint32_t timestamp);
    void blockStackingUpdates(bool block);
    void saveOldScreenSizes();
    void addToStack(Window *window);
    void removeFromStack(Window *window);

#if KWIN_BUILD_X11
    void initializeX11();
    void cleanupX11();

    void propagateWindows(bool propagate_new_windows); // Called only from updateStackingOrder
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    /// This is the right way to create a new X11 window
    X11Window *createX11Window(xcb_window_t windowId, bool is_mapped);
    void addX11Window(X11Window *c);
    X11Window *createUnmanaged(xcb_window_t windowId);
    void addUnmanaged(X11Window *c);
    bool updateXStackingOrder();
#endif
    void setupWindowConnections(Window *window);

    void addWaylandWindow(Window *window);
    void removeWaylandWindow(Window *window);

    //---------------------------------------------------------------------

    void closeActivePopup();
    void updateWindowVisibilityOnDesktopChange(VirtualDesktop *newDesktop);
    void updateWindowVisibilityAndActivateOnDesktopChange(VirtualDesktop *newDesktop);
    void activateWindowOnDesktop(VirtualDesktop *desktop);
    Window *findWindowToActivateOnDesktop(VirtualDesktop *desktop);
    void removeWindow(Window *window);

    void updateOutputConfiguration();
    void updateOutputs();
    void assignBrightnessDevices(OutputConfiguration &outputConfig);
    void updateOutputOrder();
    void maybeUpdateDpmsState();

    bool breaksShowingDesktop(Window *window) const;

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

    void updateTabbox();

    QList<LogicalOutput *> m_outputs;
    LogicalOutput *m_activeOutput = nullptr;
    QList<LogicalOutput *> m_outputOrder;

    Window *m_activeWindow;
    Window *m_lastActiveWindow;
    Window *m_moveResizeWindow;

    // Delay(ed) window focus timer and window
    QTimer *delayFocusTimer;
    Window *m_delayFocusWindow;
    QPointF focusMousePos;

    QList<Window *> m_windows;
    QList<Window *> deleted;

    QList<Window *> unconstrained_stacking_order; // Topmost last
    QList<Window *> stacking_order; // Topmost last
    QList<Window *> attention_chain;
    bool force_restacking;

    bool showing_desktop;

    QList<Group *> groups;

    bool was_user_interaction;
#if KWIN_BUILD_X11
    QList<xcb_window_t> manual_overlays; // Topmost last
    std::unique_ptr<Xcb::Window> m_nullFocus;
    std::unique_ptr<Xcb::Window> m_guardWindow;
    std::unique_ptr<X11EventFilter> m_syncAlarmFilter;
    UInt32Serial m_x11FocusSerial = 0;
#endif

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

    static Workspace *_self;
#if KWIN_BUILD_X11
    std::unique_ptr<KStartupInfo> m_startup;
#endif
    RectF m_workArea;
    StrutRects m_restrictedArea;
    QHash<const LogicalOutput *, RectF> m_screenAreas;
    Rect m_geometry;

    QHash<const LogicalOutput *, Rect> m_oldScreenGeometries;
    StrutRects m_oldRestrictedArea;
    QTimer m_rearrangeTimer;
    bool m_inRearrange = false;

    int m_setActiveWindowRecursion = 0;
    int m_blockStackingUpdates = 0; // When > 0, stacking updates are temporarily disabled
    bool m_blockedPropagatingNewWindows; // Propagate also new windows after enabling stacking updates?
    friend class StackingUpdatesBlocker;

    std::unique_ptr<KillWindow> m_windowKiller;

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
    std::map<LogicalOutput *, std::unique_ptr<TileManager>> m_tileManagers;
    std::unique_ptr<OutputConfigurationStore> m_outputConfigStore;
    std::unique_ptr<LidSwitchTracker> m_lidSwitchTracker;
    std::unique_ptr<OrientationSensor> m_orientationSensor;
    std::unique_ptr<LightSensor> m_lightSensor;
    std::unique_ptr<QTimer> m_delayedLightTimer;
    std::optional<double> m_luxAtLastBrightnessAdjust;
    std::unique_ptr<DpmsInputEventFilter> m_dpmsFilter;
    KConfigWatcher::Ptr m_kdeglobalsWatcher;

    QString m_activationToken;
    QString m_activationTokenAppId;
    UInt32Serial m_activationTokenSerial = 0;

    DpmsState m_dpms = DpmsState::On;
    QList<QString> m_recentlyRemovedDpmsOffOutputs;
    QTimer m_dpmsTimer;
    FileDescriptor m_sleepInhibitor;

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

//---------------------------------------------------------
// Unsorted

inline QList<LogicalOutput *> Workspace::outputs() const
{
    return m_outputs;
}

inline Window *Workspace::activeWindow() const
{
    return m_activeWindow;
}

#if KWIN_BUILD_X11
inline void Workspace::addGroup(Group *group)
{
    Q_EMIT groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group *group)
{
    groups.removeAll(group);
}
#endif

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

inline Workspace *workspace()
{
    return Workspace::_self;
}

} // namespace
