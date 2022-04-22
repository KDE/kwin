/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

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

namespace Xcb
{
class Tree;
class Window;
}

class Window;
class Output;
class ColorMapper;
class Compositor;
class Deleted;
class Group;
class InternalClient;
class KillWindow;
class ShortcutDialog;
class Unmanaged;
class UserActionsMenu;
class VirtualDesktop;
class X11Client;
class X11EventFilter;
enum class Predicate;

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
    bool workspaceEvent(QEvent *);

    bool hasClient(const Window *);

    /**
     * @brief Finds the first Client matching the condition expressed by passed in @p func.
     *
     * Internally findClient uses the std::find_if algorithm and that determines how the function
     * needs to be implemented. An example usage for finding a Client with a matching windowId
     * @code
     * xcb_window_t w; // our test window
     * X11Client *client = findClient([w](const X11Client *c) -> bool {
     *     return c->window() == w;
     * });
     * @endcode
     *
     * For the standard cases of matching the window id with one of the Client's windows use
     * the simplified overload method findClient(Predicate, xcb_window_t). Above example
     * can be simplified to:
     * @code
     * xcb_window_t w; // our test window
     * X11Client *client = findClient(Predicate::WindowMatch, w);
     * @endcode
     *
     * @param func Unary function that accepts a X11Client *as argument and
     * returns a value convertible to bool. The value returned indicates whether the
     * X11Client *is considered a match in the context of this function.
     * The function shall not modify its argument.
     * This can either be a function pointer or a function object.
     * @return KWin::X11Client *The found Client or @c null
     * @see findClient(Predicate, xcb_window_t)
     */
    X11Client *findClient(std::function<bool(const X11Client *)> func) const;
    Window *findAbstractClient(std::function<bool(const Window *)> func) const;
    Window *findAbstractClient(const QUuid &internalId) const;
    /**
     * @brief Finds the Client matching the given match @p predicate for the given window.
     *
     * @param predicate Which window should be compared
     * @param w The window id to test against
     * @return KWin::X11Client *The found Client or @c null
     * @see findClient(std::function<bool (const X11Client *)>)
     */
    X11Client *findClient(Predicate predicate, xcb_window_t w) const;
    void forEachClient(std::function<void(X11Client *)> func);
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

    QRect clientArea(clientAreaOption, const Output *output, const VirtualDesktop *desktop) const;
    QRect clientArea(clientAreaOption, const Window *window) const;
    QRect clientArea(clientAreaOption, const Window *window, const Output *output) const;
    QRect clientArea(clientAreaOption, const Window *window, const QPoint &pos) const;

    /**
     * Returns the geometry of this Workspace, i.e. the bounding rectangle of all outputs.
     */
    QRect geometry() const;
    QRegion restrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas = StrutAreaAll) const;

    bool initializing() const;

    Output *activeOutput() const;
    void setActiveOutput(Output *output);
    void setActiveOutput(const QPoint &pos);

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    Window *activeClient() const;
    /**
     * Client that was activated, but it's not yet really activeClient(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    Window *mostRecentlyActivatedClient() const;

    Window *clientUnderMouse(Output *output) const;

    void activateClient(Window *, bool force = false);
    bool requestFocus(Window *c, bool force = false);
    enum ActivityFlag {
        ActivityFocus = 1 << 0, // focus the window
        ActivityFocusForce = 1 << 1 | ActivityFocus, // focus even if Dock etc.
        ActivityRaise = 1 << 2 // raise the window
    };
    Q_DECLARE_FLAGS(ActivityFlags, ActivityFlag)
    bool takeActivity(Window *c, ActivityFlags flags);
    bool allowClientActivation(const Window *c, xcb_timestamp_t time = -1U, bool focus_in = false,
                               bool ignore_desktop = false);
    bool restoreFocus();
    void gotFocusIn(const Window *);
    void setShouldGetFocus(Window *);
    bool activateNextClient(Window *c);
    bool focusChangeEnabled()
    {
        return block_focus == 0;
    }

    /**
     * Indicates that the client c is being moved or resized by the user.
     */
    void setMoveResizeClient(Window *c);

    QRect adjustClientArea(Window *client, const QRect &area) const;
    QPoint adjustClientPosition(Window *c, QPoint pos, bool unrestricted, double snapAdjust = 1.0);
    QRect adjustClientSize(Window *c, QRect moveResizeGeom, Gravity gravity);
    void raiseClient(Window *c, bool nogroup = false);
    void lowerClient(Window *c, bool nogroup = false);
    void raiseClientRequest(Window *c, NET::RequestSource src = NET::FromApplication, xcb_timestamp_t timestamp = 0);
    void lowerClientRequest(X11Client *c, NET::RequestSource src, xcb_timestamp_t timestamp);
    void lowerClientRequest(Window *c);
    void restackClientUnderActive(Window *);
    void restack(Window *c, Window *under, bool force = false);
    void raiseOrLowerClient(Window *);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(X11Client *c);
    void updateStackingOrder(bool propagate_new_clients = false);
    void forceRestacking();

    void constrain(Window *below, Window *above);
    void unconstrain(Window *below, Window *above);

    void clientHidden(Window *);
    void clientAttentionChanged(Window *c, bool set);

    /**
     * @return List of clients currently managed by Workspace
     */
    const QList<X11Client *> &clientList() const
    {
        return m_x11Clients;
    }
    /**
     * @return List of unmanaged "clients" currently registered in Workspace
     */
    const QList<Unmanaged *> &unmanagedList() const
    {
        return m_unmanaged;
    }
    /**
     * @return List of deleted "clients" currently managed by Workspace
     */
    const QList<Deleted *> &deletedList() const
    {
        return deleted;
    }
    /**
     * @returns List of all clients (either X11 or Wayland) currently managed by Workspace
     */
    const QList<Window *> allClientList() const
    {
        return m_allClients;
    }

    /**
     * @returns List of all internal clients currently managed by Workspace
     */
    const QList<InternalClient *> &internalClients() const
    {
        return m_internalClients;
    }

    void stackScreenEdgesUnderOverrideRedirect();

    SessionManager *sessionManager() const;

public:
    QPoint cascadeOffset(const Window *c) const;

private:
    QTimer *m_quickTileCombineTimer;
    QuickTileMode m_lastTilingMode;

    //-------------------------------------------------
    // Unsorted

public:
    bool isOnCurrentHead();
    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    QRegion previousRestrictedMoveArea(const VirtualDesktop *desktop, StrutAreas areas = StrutAreaAll) const;
    QHash<const Output *, QRect> previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const QList<Window *> &stackingOrder() const;
    QList<Window *> xStackingOrder() const;
    QList<Window *> unconstrainedStackingOrder() const;
    QList<X11Client *> ensureStackingOrder(const QList<X11Client *> &clients) const;
    QList<Window *> ensureStackingOrder(const QList<Window *> &clients) const;

    Window *topClientOnDesktop(VirtualDesktop *desktop, Output *output = nullptr, bool unconstrained = false,
                                       bool only_normal = true) const;
    Window *findDesktop(bool topmost, VirtualDesktop *desktop) const;
    void sendClientToDesktop(Window *c, int desktop, bool dont_activate);
    void windowToPreviousDesktop(Window *c);
    void windowToNextDesktop(Window *c);
    void sendClientToOutput(Window *client, Output *output);

    void addManualOverlay(xcb_window_t id)
    {
        manual_overlays << id;
    }
    void removeManualOverlay(xcb_window_t id)
    {
        manual_overlays.removeOne(id);
    }

    /**
     * Shows the menu operations menu for the client and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect &pos, Window *cl);
    const UserActionsMenu *userActionsMenu() const
    {
        return m_userActionsMenu;
    }

    void showApplicationMenu(const QRect &pos, Window *c, int actionId);

    void updateMinimizedOfTransients(Window *);
    void updateOnAllDesktopsOfTransients(Window *);
    void checkTransients(xcb_window_t w);

    SessionInfo *takeSessionInfo(X11Client *);

    // D-Bus interface
    QString supportInformation() const;

    Output *nextOutput(Output *reference) const;
    Output *previousOutput(Output *reference) const;
    void switchToOutput(Output *output);

    /**
     * Set "Show Desktop" status
     *
     * @param showing @c true to show the desktop, @c false to restore the window positions
     * @param animated @c true if the "Show Desktop Animation" should be played, otherwise @c false
     */
    void setShowingDesktop(bool showing, bool animated = true);
    bool showingDesktop() const;

    void removeX11Client(X11Client *); // Only called from X11Client::destroyClient() or X11Client::releaseWindow()
    void setActiveClient(Window *);
    Group *findGroup(xcb_window_t leader) const;
    void addGroup(Group *group);
    void removeGroup(Group *group);
    Group *findClientLeaderGroup(const X11Client *c) const;
    int unconstainedStackingOrderIndex(const X11Client *c) const;

    void removeUnmanaged(Unmanaged *); // Only called from Unmanaged::release()
    void removeDeleted(Deleted *);
    void addDeleted(Deleted *, Window *);

    bool checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data);

    void focusToNull(); // SELI TODO: Public?

    void clientShortcutUpdated(Window *c);
    bool shortcutAvailable(const QKeySequence &cut, Window *ignore = nullptr) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcutsForClient(bool disable);

    void setWasUserInteraction();
    bool wasUserInteraction() const;

    int packPositionLeft(const Window *client, int oldX, bool leftEdge) const;
    int packPositionRight(const Window *client, int oldX, bool rightEdge) const;
    int packPositionUp(const Window *client, int oldY, bool topEdge) const;
    int packPositionDown(const Window *client, int oldY, bool bottomEdge) const;

    void cancelDelayFocus();
    void requestDelayFocus(Window *);

    /**
     * updates the mouse position to track whether a focus follow mouse focus change was caused by
     * an actual mouse move
     * is esp. called on enter/motion events of inactive windows
     * since an active window doesn't receive mouse events, it must also be invoked if a (potentially)
     * active window might be moved/resize away from the cursor (causing a leave event)
     */
    void updateFocusMousePosition(const QPoint &pos);
    QPoint focusMousePosition() const;

    /**
     * Returns a client that is currently being moved or resized by the user.
     *
     * If none of clients is being moved or resized, @c null will be returned.
     */
    Window *moveResizeClient()
    {
        return movingClient;
    }

    void markXStackingOrderAsDirty();

    void quickTileWindow(QuickTileMode mode);

    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest
    };
    void switchWindow(Direction direction);

    ShortcutDialog *shortcutDialog() const
    {
        return client_keys_dialog;
    }

    /**
     * Adds the internal client to Workspace.
     *
     * This method will be called by InternalClient when it's mapped.
     *
     * @see internalClientAdded
     * @internal
     */
    void addInternalClient(InternalClient *client);

    /**
     * Removes the internal client from Workspace.
     *
     * This method is meant to be called only by InternalClient.
     *
     * @see internalClientRemoved
     * @internal
     */
    void removeInternalClient(InternalClient *client);

    /**
     * @internal
     * Used by session management
     */
    void setInitialDesktop(int desktop);

public Q_SLOTS:
    void performWindowOperation(KWin::Window *c, Options::WindowOperation op);
    // Keybindings
    // void slotSwitchToWindow( int );
    void slotWindowToDesktop(VirtualDesktop *desktop);

    // void slotWindowToListPosition( int );
    void slotSwitchToScreen();
    void slotWindowToScreen();
    void slotSwitchToNextScreen();
    void slotWindowToNextScreen();
    void slotSwitchToPrevScreen();
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
    void slotOutputEnabled(Output *output);
    void slotOutputDisabled(Output *output);

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
    void clientAdded(KWin::Window *);
    void clientRemoved(KWin::Window *);
    void clientActivated(KWin::Window *);
    void clientDemandsAttentionChanged(KWin::Window *, bool);
    void clientMinimizedChanged(KWin::Window *);
    void groupAdded(KWin::Group *);
    void unmanagedAdded(KWin::Unmanaged *);
    void unmanagedRemoved(KWin::Unmanaged *);
    void deletedRemoved(KWin::Deleted *);
    void configChanged();
    void showingDesktopChanged(bool showing, bool animated);
    /**
     * This signal is emitted when the stacking order changed, i.e. a window is risen
     * or lowered
     */
    void stackingOrderChanged();

    /**
     * This signal is emitted whenever an internal client is created.
     */
    void internalClientAdded(KWin::InternalClient *client);

    /**
     * This signal is emitted whenever an internal client gets removed.
     */
    void internalClientRemoved(KWin::InternalClient *client);

private:
    void init();
    void initializeX11();
    void cleanupX11();
    void initShortcuts();
    template<typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut,
                      Slot slot, const QVariant &data = QVariant());
    template<typename T, typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, const QVariant &data = QVariant());
    void setupWindowShortcut(Window *c);
    bool switchWindow(Window *c, Direction direction, QPoint curPos, VirtualDesktop *desktop);

    void propagateClients(bool propagate_new_clients); // Called only from updateStackingOrder
    QList<Window *> constrainedStackingOrder();
    void raiseClientWithinApplication(Window *c);
    void lowerClientWithinApplication(Window *c);
    bool allowFullClientRaising(const Window *c, xcb_timestamp_t timestamp);
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    void saveOldScreenSizes();
    void addToStack(Window *toplevel);
    void replaceInStack(Window *original, Deleted *deleted);
    void removeFromStack(Window *toplevel);

    /// This is the right way to create a new client
    X11Client *createClient(xcb_window_t w, bool is_mapped);
    void setupClientConnections(Window *client);
    void addClient(X11Client *c);
    Unmanaged *createUnmanaged(xcb_window_t w);
    void addUnmanaged(Unmanaged *c);

    void addShellClient(Window *client);
    void removeShellClient(Window *client);

    //---------------------------------------------------------------------

    void closeActivePopup();
    void updateClientVisibilityOnDesktopChange(VirtualDesktop *newDesktop);
    void activateClientOnNewDesktop(VirtualDesktop *desktop);
    Window *findClientToActivateOnDesktop(VirtualDesktop *desktop);
    void removeAbstractClient(Window *client);

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
    Window *active_popup_client;

    int m_initialDesktop;
    void updateXStackingOrder();
    void updateTabbox();

    Output *m_activeOutput = nullptr;
    Window *active_client;
    Window *last_active_client;
    Window *movingClient;

    // Delay(ed) window focus timer and client
    QTimer *delayFocusTimer;
    Window *delayfocus_client;
    QPoint focusMousePos;

    QList<X11Client *> m_x11Clients;
    QList<Window *> m_allClients;
    QList<Unmanaged *> m_unmanaged;
    QList<Deleted *> deleted;
    QList<InternalClient *> m_internalClients;

    QList<Window *> unconstrained_stacking_order; // Topmost last
    QList<Window *> stacking_order; // Topmost last
    QVector<xcb_window_t> manual_overlays; // Topmost last
    bool force_restacking;
    QList<Window *> x_stacking; // From XQueryTree()
    std::unique_ptr<Xcb::Tree> m_xStackingQueryTree;
    bool m_xStackingDirty = false;
    QList<Window *> should_get_focus; // Last is most recent
    QList<Window *> attention_chain;

    bool showing_desktop;

    QList<Group *> groups;

    bool was_user_interaction;
    QScopedPointer<X11EventFilter> m_wasUserInteractionFilter;

    int block_focus;

    /**
     * Holds the menu containing the user actions which is shown
     * on e.g. right click the window decoration.
     */
    UserActionsMenu *m_userActionsMenu;

    void modalActionsSwitch(bool enabled);

    ShortcutDialog *client_keys_dialog;
    Window *client_keys_client;
    bool global_shortcuts_disabled_for_client;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace *_self;

    bool workspaceInit;

    QScopedPointer<KStartupInfo> m_startup;
    QScopedPointer<ColorMapper> m_colorMapper;

    QHash<const VirtualDesktop *, QRect> m_workAreas;
    QHash<const VirtualDesktop *, StrutRects> m_restrictedAreas;
    QHash<const VirtualDesktop *, QHash<const Output *, QRect>> m_screenAreas;
    QRect m_geometry;

    QHash<const Output *, QRect> m_oldScreenGeometries;
    QSize olddisplaysize; // previous sizes od displayWidth()/displayHeight()
    QHash<const VirtualDesktop *, StrutRects> m_oldRestrictedAreas;
    bool m_inUpdateClientArea = false;

    int set_active_client_recursion;
    int block_stacking_updates; // When > 0, stacking updates are temporarily disabled
    bool blocked_propagating_new_clients; // Propagate also new clients after enabling stacking updates?
    QScopedPointer<Xcb::Window> m_nullFocus;
    friend class StackingUpdatesBlocker;

    QScopedPointer<KillWindow> m_windowKiller;
    QScopedPointer<X11EventFilter> m_movingClientFilter;
    QScopedPointer<X11EventFilter> m_syncAlarmFilter;

    SessionManager *m_sessionManager;

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

inline bool Workspace::initializing() const
{
    return workspaceInit;
}

inline Window *Workspace::activeClient() const
{
    return active_client;
}

inline Window *Workspace::mostRecentlyActivatedClient() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : active_client;
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
    return global_shortcuts_disabled_for_client;
}

inline void Workspace::forceRestacking()
{
    force_restacking = true;
    StackingUpdatesBlocker blocker(this); // Do restacking if not blocked
}

inline void Workspace::updateFocusMousePosition(const QPoint &pos)
{
    focusMousePos = pos;
}

inline QPoint Workspace::focusMousePosition() const
{
    return focusMousePos;
}

inline void Workspace::forEachClient(std::function<void(X11Client *)> func)
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

#endif
