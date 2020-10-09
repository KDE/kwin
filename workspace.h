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
#include "utils.h"
// Qt
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
class QStringList;

namespace KWin
{

namespace Xcb
{
class Tree;
class Window;
}

class AbstractClient;
class ColorMapper;
class Compositor;
class Deleted;
class Group;
class InternalClient;
class KillWindow;
class ShortcutDialog;
class Toplevel;
class Unmanaged;
class UserActionsMenu;
class X11Client;
class X11EventFilter;
enum class Predicate;

class X11EventFilterContainer : public QObject
{
    Q_OBJECT

public:
    explicit X11EventFilterContainer(X11EventFilter *filter);

    X11EventFilter *filter() const;

private:
    X11EventFilter *m_filter;
};

class KWIN_EXPORT Workspace : public QObject
{
    Q_OBJECT
public:
    explicit Workspace();
    ~Workspace() override;

    static Workspace* self() {
        return _self;
    }

    bool workspaceEvent(xcb_generic_event_t*);
    bool workspaceEvent(QEvent*);

    bool hasClient(const X11Client *);
    bool hasClient(const AbstractClient*);

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
    X11Client *findClient(std::function<bool (const X11Client *)> func) const;
    AbstractClient *findAbstractClient(std::function<bool (const AbstractClient*)> func) const;
    /**
     * @brief Finds the Client matching the given match @p predicate for the given window.
     *
     * @param predicate Which window should be compared
     * @param w The window id to test against
     * @return KWin::X11Client *The found Client or @c null
     * @see findClient(std::function<bool (const X11Client *)>)
     */
    X11Client *findClient(Predicate predicate, xcb_window_t w) const;
    void forEachClient(std::function<void (X11Client *)> func);
    void forEachAbstractClient(std::function<void (AbstractClient*)> func);
    Unmanaged *findUnmanaged(std::function<bool (const Unmanaged*)> func) const;
    /**
     * @brief Finds the Unmanaged with the given window id.
     *
     * @param w The window id to search for
     * @return KWin::Unmanaged* Found Unmanaged or @c null if there is no Unmanaged with given Id.
     */
    Unmanaged *findUnmanaged(xcb_window_t w) const;
    void forEachUnmanaged(std::function<void (Unmanaged*)> func);
    Toplevel *findToplevel(std::function<bool (const Toplevel*)> func) const;
    void forEachToplevel(std::function<void (Toplevel *)> func);

    Toplevel *findToplevel(const QUuid &internalId) const;

    /**
     * @brief Finds a Toplevel for the internal window @p w.
     *
     * Internal window means a window created by KWin itself. On X11 this is an Unmanaged
     * and mapped by the window id, on Wayland a XdgShellClient mapped on the internal window id.
     *
     * @returns Toplevel
     */
    Toplevel *findInternal(QWindow *w) const;

    QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    QRect clientArea(clientAreaOption, const AbstractClient* c) const;
    QRect clientArea(clientAreaOption, int screen, int desktop) const;

    QRegion restrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;

    bool initializing() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    AbstractClient* activeClient() const;
    /**
     * Client that was activated, but it's not yet really activeClient(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    AbstractClient* mostRecentlyActivatedClient() const;

    AbstractClient* clientUnderMouse(int screen) const;

    void activateClient(AbstractClient*, bool force = false);
    bool requestFocus(AbstractClient* c, bool force = false);
    enum ActivityFlag {
        ActivityFocus = 1 << 0, // focus the window
        ActivityFocusForce = 1 << 1 | ActivityFocus, // focus even if Dock etc.
        ActivityRaise = 1 << 2 // raise the window
    };
    Q_DECLARE_FLAGS(ActivityFlags, ActivityFlag)
    bool takeActivity(AbstractClient* c, ActivityFlags flags);
    bool allowClientActivation(const AbstractClient* c, xcb_timestamp_t time = -1U, bool focus_in = false,
                               bool ignore_desktop = false);
    bool restoreFocus();
    void gotFocusIn(const AbstractClient*);
    void setShouldGetFocus(AbstractClient*);
    bool activateNextClient(AbstractClient* c);
    bool focusChangeEnabled() {
        return block_focus == 0;
    }

    /**
     * Indicates that the client c is being moved or resized by the user.
     */
    void setMoveResizeClient(AbstractClient* c);

    QRect adjustClientArea(AbstractClient *client, const QRect &area) const;
    QPoint adjustClientPosition(AbstractClient* c, QPoint pos, bool unrestricted, double snapAdjust = 1.0);
    QRect adjustClientSize(AbstractClient* c, QRect moveResizeGeom, int mode);
    void raiseClient(AbstractClient* c, bool nogroup = false);
    void lowerClient(AbstractClient* c, bool nogroup = false);
    void raiseClientRequest(AbstractClient* c, NET::RequestSource src = NET::FromApplication, xcb_timestamp_t timestamp = 0);
    void lowerClientRequest(X11Client *c, NET::RequestSource src, xcb_timestamp_t timestamp);
    void lowerClientRequest(AbstractClient* c);
    void restackClientUnderActive(AbstractClient*);
    void restack(AbstractClient *c, AbstractClient *under, bool force = false);
    void updateClientLayer(AbstractClient* c);
    void raiseOrLowerClient(AbstractClient*);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(X11Client *c);
    void updateStackingOrder(bool propagate_new_clients = false);
    void forceRestacking();

    void clientHidden(AbstractClient*);
    void clientAttentionChanged(AbstractClient* c, bool set);

    /**
     * @return List of clients currently managed by Workspace
     */
    const QList<X11Client *> &clientList() const {
        return clients;
    }
    /**
     * @return List of unmanaged "clients" currently registered in Workspace
     */
    const QList<Unmanaged *> &unmanagedList() const {
        return m_unmanaged;
    }
    /**
     * @return List of deleted "clients" currently managed by Workspace
     */
    const QList<Deleted *> &deletedList() const {
        return deleted;
    }
    /**
     * @returns List of all clients (either X11 or Wayland) currently managed by Workspace
     */
    const QList<AbstractClient*> allClientList() const {
        return m_allClients;
    }

    /**
     * @returns List of all internal clients currently managed by Workspace
     */
    const QList<InternalClient *> &internalClients() const {
        return m_internalClients;
    }

    void stackScreenEdgesUnderOverrideRedirect();

    SessionManager *sessionManager() const;

public:
    QPoint cascadeOffset(const AbstractClient *c) const;

private:
    Compositor *m_compositor;
    QTimer *m_quickTileCombineTimer;
    QuickTileMode m_lastTilingMode;

    //-------------------------------------------------
    // Unsorted

public:
    bool isOnCurrentHead();
    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    QRegion previousRestrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;
    QVector< QRect > previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const QList<Toplevel *> &stackingOrder() const;
    QList<Toplevel *> xStackingOrder() const;
    QList<X11Client *> ensureStackingOrder(const QList<X11Client *> &clients) const;
    QList<AbstractClient*> ensureStackingOrder(const QList<AbstractClient*> &clients) const;

    AbstractClient* topClientOnDesktop(int desktop, int screen, bool unconstrained = false,
                               bool only_normal = true) const;
    AbstractClient* findDesktop(bool topmost, int desktop) const;
    void sendClientToDesktop(AbstractClient* c, int desktop, bool dont_activate);
    void windowToPreviousDesktop(AbstractClient* c);
    void windowToNextDesktop(AbstractClient* c);
    void sendClientToScreen(AbstractClient* c, int screen);

    void addManualOverlay(xcb_window_t id) {
        manual_overlays << id;
    }
    void removeManualOverlay(xcb_window_t id) {
        manual_overlays.removeOne(id);
    }

    /**
     * Shows the menu operations menu for the client and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect& pos, AbstractClient* cl);
    const UserActionsMenu *userActionsMenu() const {
        return m_userActionsMenu;
    }

    void showApplicationMenu(const QRect &pos, AbstractClient *c, int actionId);

    void updateMinimizedOfTransients(AbstractClient*);
    void updateOnAllDesktopsOfTransients(AbstractClient*);
    void checkTransients(xcb_window_t w);

    void storeSession(const QString &sessionName, SMSavePhase phase);
    void storeClient(KConfigGroup &cg, int num, X11Client *c);
    void storeSubSession(const QString &name, QSet<QByteArray> sessionIds);
    void loadSubSessionInfo(const QString &name);

    SessionInfo* takeSessionInfo(X11Client *);

    // D-Bus interface
    QString supportInformation() const;

    void setCurrentScreen(int new_screen);

    void setShowingDesktop(bool showing);
    bool showingDesktop() const;

    void removeClient(X11Client *);   // Only called from X11Client::destroyClient() or X11Client::releaseWindow()
    void setActiveClient(AbstractClient*);
    Group* findGroup(xcb_window_t leader) const;
    void addGroup(Group* group);
    void removeGroup(Group* group);
    Group* findClientLeaderGroup(const X11Client *c) const;

    void removeUnmanaged(Unmanaged*);   // Only called from Unmanaged::release()
    void removeDeleted(Deleted*);
    void addDeleted(Deleted*, Toplevel*);

    bool checkStartupNotification(xcb_window_t w, KStartupInfoId& id, KStartupInfoData& data);

    void focusToNull(); // SELI TODO: Public?

    void clientShortcutUpdated(AbstractClient* c);
    bool shortcutAvailable(const QKeySequence &cut, AbstractClient* ignore = nullptr) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcutsForClient(bool disable);

    void setWasUserInteraction();
    bool wasUserInteraction() const;

    int packPositionLeft(const AbstractClient *client, int oldX, bool leftEdge) const;
    int packPositionRight(const AbstractClient *client, int oldX, bool rightEdge) const;
    int packPositionUp(const AbstractClient *client, int oldY, bool topEdge) const;
    int packPositionDown(const AbstractClient *client, int oldY, bool bottomEdge) const;

    void cancelDelayFocus();
    void requestDelayFocus(AbstractClient*);

    /**
     * updates the mouse position to track whether a focus follow mouse focus change was caused by
     * an actual mouse move
     * is esp. called on enter/motion events of inactive windows
     * since an active window doesn't receive mouse events, it must also be invoked if a (potentially)
     * active window might be moved/resize away from the cursor (causing a leave event)
     */
    void updateFocusMousePosition(const QPoint& pos);
    QPoint focusMousePosition() const;

    /**
     * Returns a client that is currently being moved or resized by the user.
     *
     * If none of clients is being moved or resized, @c null will be returned.
     */
    AbstractClient* moveResizeClient() {
        return movingClient;
    }

    /**
     * @returns Whether we have a Compositor and it is active (Scene created)
     */
    bool compositing() const;

    void registerEventFilter(X11EventFilter *filter);
    void unregisterEventFilter(X11EventFilter *filter);

    void markXStackingOrderAsDirty();

    void quickTileWindow(QuickTileMode mode);

    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest
    };
    void switchWindow(Direction direction);

    ShortcutDialog *shortcutDialog() const {
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

public Q_SLOTS:
    void performWindowOperation(KWin::AbstractClient* c, Options::WindowOperation op);
    // Keybindings
    //void slotSwitchToWindow( int );
    void slotWindowToDesktop(uint i);

    //void slotWindowToListPosition( int );
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
    void slotWindowPackLeft();
    void slotWindowPackRight();
    void slotWindowPackUp();
    void slotWindowPackDown();
    void slotWindowGrowHorizontal();
    void slotWindowGrowVertical();
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
    void slotDesktopCountChanged(uint previousCount, uint newCount);
    void slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop);

Q_SIGNALS:
    /**
     * Emitted after the Workspace has setup the complete initialization process.
     * This can be used to connect to for performing post-workspace initialization.
     */
    void workspaceInitialized();

    //Signals required for the scripting interface
    void desktopPresenceChanged(KWin::AbstractClient*, int);
    void currentDesktopChanged(int, KWin::AbstractClient*);
    void clientAdded(KWin::AbstractClient *);
    void clientRemoved(KWin::AbstractClient*);
    void clientActivated(KWin::AbstractClient*);
    void clientDemandsAttentionChanged(KWin::AbstractClient*, bool);
    void clientMinimizedChanged(KWin::AbstractClient*);
    void groupAdded(KWin::Group*);
    void unmanagedAdded(KWin::Unmanaged*);
    void unmanagedRemoved(KWin::Unmanaged*);
    void deletedRemoved(KWin::Deleted*);
    void configChanged();
    void showingDesktopChanged(bool showing);
    /**
     * This signels is emitted when ever the stacking order is change, ie. a window is risen
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
    template <typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut,
                      Slot slot, const QVariant &data = QVariant());
    template <typename T, typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, const QVariant &data = QVariant());
    void setupWindowShortcut(AbstractClient* c);
    bool switchWindow(AbstractClient *c, Direction direction, QPoint curPos, int desktop);

    void propagateClients(bool propagate_new_clients);   // Called only from updateStackingOrder
    QList<Toplevel *> constrainedStackingOrder();
    void raiseClientWithinApplication(AbstractClient* c);
    void lowerClientWithinApplication(AbstractClient* c);
    bool allowFullClientRaising(const AbstractClient* c, xcb_timestamp_t timestamp);
    bool keepTransientAbove(const AbstractClient* mainwindow, const AbstractClient* transient);
    bool keepDeletedTransientAbove(const Toplevel *mainWindow, const Deleted *transient) const;
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    void saveOldScreenSizes();

    /// This is the right way to create a new client
    X11Client *createClient(xcb_window_t w, bool is_mapped);
    void setupClientConnections(AbstractClient *client);
    void addClient(X11Client *c);
    Unmanaged* createUnmanaged(xcb_window_t w);
    void addUnmanaged(Unmanaged* c);

    void addShellClient(AbstractClient *client);
    void removeShellClient(AbstractClient *client);

    //---------------------------------------------------------------------

    void closeActivePopup();
    void updateClientArea(bool force);
    void resetClientAreas(uint desktopCount);
    void updateClientVisibilityOnDesktopChange(uint newDesktop);
    void activateClientOnNewDesktop(uint desktop);
    AbstractClient *findClientToActivateOnDesktop(uint desktop);

    QWidget* active_popup;
    AbstractClient* active_popup_client;

    int m_initialDesktop;
    void loadSessionInfo(const QString &sessionName);
    void addSessionInfo(KConfigGroup &cg);

    QList<SessionInfo*> session;

    void updateXStackingOrder();
    void updateTabbox();

    AbstractClient* active_client;
    AbstractClient* last_active_client;
    AbstractClient* most_recently_raised; // Used ONLY by raiseOrLowerClient()
    AbstractClient* movingClient;

    // Delay(ed) window focus timer and client
    QTimer* delayFocusTimer;
    AbstractClient* delayfocus_client;
    QPoint focusMousePos;

    QList<X11Client *> clients;
    QList<AbstractClient*> m_allClients;
    QList<Unmanaged *> m_unmanaged;
    QList<Deleted *> deleted;
    QList<InternalClient *> m_internalClients;

    QList<Toplevel *> unconstrained_stacking_order; // Topmost last
    QList<Toplevel *> stacking_order; // Topmost last
    QVector<xcb_window_t> manual_overlays; //Topmost last
    bool force_restacking;
    QList<Toplevel *> x_stacking; // From XQueryTree()
    std::unique_ptr<Xcb::Tree> m_xStackingQueryTree;
    bool m_xStackingDirty = false;
    QList<AbstractClient*> should_get_focus; // Last is most recent
    QList<AbstractClient*> attention_chain;

    bool showing_desktop;

    QList<Group *> groups;

    bool was_user_interaction;
    QScopedPointer<X11EventFilter> m_wasUserInteractionFilter;

    int session_active_client;
    int session_desktop;

    int block_focus;

    /**
     * Holds the menu containing the user actions which is shown
     * on e.g. right click the window decoration.
     */
    UserActionsMenu *m_userActionsMenu;

    void modalActionsSwitch(bool enabled);

    ShortcutDialog* client_keys_dialog;
    AbstractClient* client_keys_client;
    bool global_shortcuts_disabled_for_client;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace* _self;

    bool workspaceInit;

    QScopedPointer<KStartupInfo> m_startup;
    QScopedPointer<ColorMapper> m_colorMapper;

    QVector<QRect> workarea; // Array of workareas for virtual desktops
    // Array of restricted areas that window cannot be moved into
    QVector<StrutRects> restrictedmovearea;
    // Array of the previous restricted areas that window cannot be moved into
    QVector<StrutRects> oldrestrictedmovearea;
    QVector< QVector<QRect> > screenarea; // Array of workareas per xinerama screen for all virtual desktops
    QVector< QRect > oldscreensizes; // array of previous sizes of xinerama screens
    QSize olddisplaysize; // previous sizes od displayWidth()/displayHeight()

    int set_active_client_recursion;
    int block_stacking_updates; // When > 0, stacking updates are temporarily disabled
    bool blocked_propagating_new_clients; // Propagate also new clients after enabling stacking updates?
    QScopedPointer<Xcb::Window> m_nullFocus;
    friend class StackingUpdatesBlocker;

    QScopedPointer<KillWindow> m_windowKiller;

    QList<QPointer<X11EventFilterContainer>> m_eventFilters;
    QList<QPointer<X11EventFilterContainer>> m_genericEventFilters;
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
    explicit StackingUpdatesBlocker(Workspace* w)
        : ws(w) {
        ws->blockStackingUpdates(true);
    }
    ~StackingUpdatesBlocker() {
        ws->blockStackingUpdates(false);
    }

private:
    Workspace* ws;
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

inline AbstractClient *Workspace::activeClient() const
{
    return active_client;
}

inline AbstractClient *Workspace::mostRecentlyActivatedClient() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : active_client;
}

inline void Workspace::addGroup(Group* group)
{
    emit groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group* group)
{
    groups.removeAll(group);
}

inline const QList<Toplevel *> &Workspace::stackingOrder() const
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
    StackingUpdatesBlocker blocker(this);   // Do restacking if not blocked
}

inline void Workspace::updateFocusMousePosition(const QPoint& pos)
{
    focusMousePos = pos;
}

inline QPoint Workspace::focusMousePosition() const
{
    return focusMousePos;
}

inline
void Workspace::forEachClient(std::function< void (X11Client *) > func)
{
    std::for_each(clients.constBegin(), clients.constEnd(), func);
}

inline
void Workspace::forEachUnmanaged(std::function< void (Unmanaged*) > func)
{
    std::for_each(m_unmanaged.constBegin(), m_unmanaged.constEnd(), func);
}

inline bool Workspace::hasClient(const X11Client *c)
{
    return findClient([c](const X11Client *test) {
        return test == c;
    });
}

inline Workspace *workspace()
{
    return Workspace::_self;
}

} // namespace
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Workspace::ActivityFlags)

#endif
