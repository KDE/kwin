/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

#include <QTimer>
#include <QVector>
#include <kshortcut.h>
#include <QCursor>
#include <netwm.h>
#include <kxmessages.h>
#include <QElapsedTimer>
#include <kmanagerselection.h>

#include <KActivities/Controller>

#include "plugins.h"
#include "utils.h"
#include "kdecoration.h"
#include "kdecorationfactory.h"
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif
#include "sm.h"

#include <X11/Xlib.h>

// TODO: Cleanup the order of things in this .h file

class QMenu;
class QActionGroup;
class QStringList;
class KConfig;
class KActionCollection;
class KStartupInfo;
class KStartupInfoId;
class KStartupInfoData;
class QSlider;
class QPushButton;

namespace Kephal
{
    class Screen;
}
namespace KWin
{

#ifdef KWIN_BUILD_TABBOX
namespace TabBox
{
class TabBox;
}
#endif

class Client;
#ifdef KWIN_BUILD_TILING
class Tile;
class Tiling;
class TilingLayout;
#endif
class ClientGroup;
#ifdef KWIN_BUILD_DESKTOPCHANGEOSD
class DesktopChangeOSD;
#endif
class Outline;
class RootInfo;
class PluginMgr;
class Placement;
class Rules;
class WindowRules;

class Workspace : public QObject, public KDecorationDefines
{
    Q_OBJECT
public:
    Workspace(bool restore = false);
    virtual ~Workspace();

    static Workspace* self() {
        return _self;
    }

    bool workspaceEvent(XEvent*);
    bool workspaceEvent(QEvent*);

    KDecoration* createDecoration(KDecorationBridge* bridge);
    bool hasDecorationPlugin() const;

    bool hasClient(const Client*);

    template<typename T> Client* findClient(T predicate) const;
    template<typename T1, typename T2> void forEachClient(T1 procedure, T2 predicate);
    template<typename T> void forEachClient(T procedure);
    template<typename T> Unmanaged* findUnmanaged(T predicate) const;
    template<typename T1, typename T2> void forEachUnmanaged(T1 procedure, T2 predicate);
    template<typename T> void forEachUnmanaged(T procedure);

    QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    QRect clientArea(clientAreaOption, const Client* c) const;
    QRect clientArea(clientAreaOption, int screen, int desktop) const;

    QRegion restrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;

    /**
     * @internal
     */
    void killWindowId(Window window);

    void killWindow() {
        slotKillWindow();
    }

    bool initializing() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    Client* activeClient() const;
    /**
     * Client that was activated, but it's not yet really activeClient(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    Client* mostRecentlyActivatedClient() const;

    void activateClient(Client*, bool force = false);
    void requestFocus(Client* c, bool force = false);
    void takeActivity(Client* c, int flags, bool handled);   // Flags are ActivityFlags
    void handleTakeActivity(Client* c, Time timestamp, int flags);   // Flags are ActivityFlags
    bool allowClientActivation(const Client* c, Time time = -1U, bool focus_in = false,
                               bool ignore_desktop = false);
    void restoreFocus();
    void gotFocusIn(const Client*);
    void setShouldGetFocus(Client*);
    bool activateNextClient(Client* c);
    bool focusChangeEnabled() {
        return block_focus == 0;
    }

    void updateColormap();

    /**
     * Indicates that the client c is being moved around by the user.
     */
    void setClientIsMoving(Client* c);

    void place(Client* c, QRect& area);
    void placeSmart(Client* c, const QRect& area);

    QPoint adjustClientPosition(Client* c, QPoint pos, bool unrestricted, double snapAdjust = 1.0);
    QRect adjustClientSize(Client* c, QRect moveResizeGeom, int mode);
    void raiseClient(Client* c, bool nogroup = false);
    void lowerClient(Client* c, bool nogroup = false);
    void raiseClientRequest(Client* c, NET::RequestSource src, Time timestamp);
    void lowerClientRequest(Client* c, NET::RequestSource src, Time timestamp);
    void restackClientUnderActive(Client*);
    void restack(Client *c, Client *under);
    void updateClientLayer(Client* c);
    void raiseOrLowerClient(Client*);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(Client* c);
    void updateStackingOrder(bool propagate_new_clients = false);
    void forceRestacking();

    void clientHidden(Client*);
    void clientAttentionChanged(Client* c, bool set);

    /**
     * @return List of clients currently managed by Workspace
     **/
    const ClientList &clientList() const {
        return clients;
    }
    /**
     * @return List of unmanaged "clients" currently registered in Workspace
     **/
    const UnmanagedList &unmanagedList() const {
        return unmanaged;
    }

#ifdef KWIN_BUILD_TILING
    Tiling* tiling();
#endif

    Outline* outline();
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdge* screenEdge();
#endif

    //-------------------------------------------------
    // Desktop layout

public:
    /**
     * @returns Total number of desktops currently in existence.
     */
    int numberOfDesktops() const;
    /**
     * Set the number of available desktops to @a count. This function overrides any previous
     * grid layout.
     */
    void setNumberOfDesktops(int count);
    /**
     * Called from within setNumberOfDesktops() to ensure the desktop layout is still valid.
     */
    void updateDesktopLayout();

    /**
     * @returns The size of desktop layout in grid units.
     */
    QSize desktopGridSize() const;
    /**
     * @returns The width of desktop layout in grid units.
     */
    int desktopGridWidth() const;
    /**
     * @returns The height of desktop layout in grid units.
     */
    int desktopGridHeight() const;
    /**
     * @returns The width of desktop layout in pixels. Equivalent to gridWidth() *
     * ::displayWidth().
     */
    int workspaceWidth() const;
    /**
     * @returns The height of desktop layout in pixels. Equivalent to gridHeight() *
     * ::displayHeight().
     */
    int workspaceHeight() const;

    /**
     * @returns The ID of the current desktop.
     */
    int currentDesktop() const;
    /**
     * Set the current desktop to @a current.
     * @returns True on success, false otherwise.
     */
    bool setCurrentDesktop(int current);

    /**
     * Generate a desktop layout from EWMH _NET_DESKTOP_LAYOUT property parameters.
     */
    void setNETDesktopLayout(Qt::Orientation orientation, int width, int height, int startingCorner);

    /**
     * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
     * point. @a coords is to be in grid units.
     */
    int desktopAtCoords(QPoint coords) const;
    /**
     * @returns The coords of desktop @a id in grid units.
     */
    QPoint desktopGridCoords(int id) const;
    /**
     * @returns The coords of the top-left corner of desktop @a id in pixels.
     */
    QPoint desktopCoords(int id) const;

    /**
     * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
     * the layout if @a wrap is set. If @a id is not set use the current one.
     */
    int desktopAbove(int id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
     * left of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    int desktopToRight(int id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
     * layout if @a wrap is set. If @a id is not set use the current one.
     */
    int desktopBelow(int id = 0, bool wrap = true) const;
    /**
     * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
     * right of the layout if @a wrap is set. If @a id is not set use the current one.
     */
    int desktopToLeft(int id = 0, bool wrap = true) const;

private:
    int desktopCount_;
    QSize desktopGridSize_;
    int* desktopGrid_;
    int currentDesktop_;
    QString activity_;
    QStringList allActivities_;

    KActivities::Controller activityController_;

#ifdef KWIN_BUILD_TILING
    Tiling* m_tiling;
#endif
    Outline* m_outline;
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdge m_screenEdge;
#endif

    //-------------------------------------------------
    // Unsorted

public:
    int activeScreen() const;
    int numScreens() const;
    void checkActiveScreen(const Client* c);
    void setActiveScreenMouse(const QPoint& mousepos);
    QRect screenGeometry(int screen) const;
    int screenNumber(const QPoint& pos) const;
    QString currentActivity() const {
        return activity_;
    }
    QStringList activityList() const {
        return allActivities_;
    }
    QStringList openActivityList() const {
        return activityController_.listActivities(KActivities::Info::Running);
    }

    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    QRegion previousRestrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;
    QVector< QRect > previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    // Tab box
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox() const;
#endif
    bool hasTabBox() const;

    const QVector<int> &desktopFocusChain() const {
        return desktop_focus_chain;
    }
    const ClientList &globalFocusChain() const {
        return global_focus_chain;
    }
    KActionCollection* actionCollection() const {
        return keys;
    }
    KActionCollection* disableShortcutsKeys() const {
        return disable_shortcuts_keys;
    }
    KActionCollection* clientKeys() const {
        return client_keys;
    }

    // Tabbing
    void addClientGroup(ClientGroup* group);
    void removeClientGroup(ClientGroup* group);
    /// Returns the index of c in clientGroupList.
    int indexOfClientGroup(ClientGroup* group);
    /// Change the client c_id to the group with index g_id
    void moveItemToClientGroup(ClientGroup* oldGroup, int oldIndex, ClientGroup* group, int index = -1);
    Client* findSimilarClient(Client* c);
    QList<ClientGroup*> clientGroups; // List of existing clients groups with no special order

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const ClientList& stackingOrder() const;
    ToplevelList xStackingOrder() const;
    ClientList ensureStackingOrder(const ClientList& clients) const;

    Client* topClientOnDesktop(int desktop, int screen, bool unconstrained = false,
                               bool only_normal = true) const;
    Client* findDesktop(bool topmost, int desktop) const;
    void sendClientToDesktop(Client* c, int desktop, bool dont_activate);
    void toggleClientOnActivity(Client* c, const QString &activity, bool dont_activate);
    void windowToPreviousDesktop(Client* c);
    void windowToNextDesktop(Client* c);
    void sendClientToScreen(Client* c, int screen);

    // KDE4 remove me - And it's also in the DCOP interface :(
    void showWindowMenuAt(unsigned long id, int x, int y);

    void toggleCompositing();
    void loadEffect(const QString& name);
    void toggleEffect(const QString& name);
    void reconfigureEffect(const QString& name);
    void unloadEffect(const QString& name);
    void updateCompositeBlocking(Client* c = NULL);

    QStringList loadedEffects() const;
    QStringList listOfEffects() const;


    /**
     * Shows the menu operations menu for the client and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect& pos, Client* cl);
    /**
     * Backwards compatibility.
     */
    void showWindowMenu(int x, int y, Client* cl);
    void showWindowMenu(QPoint pos, Client* cl);

    void updateMinimizedOfTransients(Client*);
    void updateOnAllDesktopsOfTransients(Client*);
    void updateOnAllActivitiesOfTransients(Client*);
    void checkTransients(Window w);

    void performWindowOperation(Client* c, WindowOperation op);

    void storeSession(KConfig* config, SMSavePhase phase);
    void storeClient(KConfigGroup &cg, int num, Client *c);
    void storeSubSession(const QString &name, QSet<QByteArray> sessionIds);

    SessionInfo* takeSessionInfo(Client*);
    WindowRules findWindowRules(const Client*, bool);
    void rulesUpdated();
    void discardUsedWindowRules(Client* c, bool withdraw);
    void disableRulesUpdates(bool disable);
    bool rulesUpdatesDisabled() const;

    bool hasDecorationShadows() const;
    Qt::Corner decorationCloseButtonCorner();
    bool decorationHasAlpha() const;
    bool decorationSupportsClientGrouping() const; // Returns true if the decoration supports tabs.
    bool decorationSupportsFrameOverlap() const;
    bool decorationSupportsBlurBehind() const;

    // D-Bus interface
    void cascadeDesktop();
    void unclutterDesktop();
    void doNotManage(const QString&);
    QList<int> decorationSupportedColors() const;
    void nextDesktop();
    void previousDesktop();
    void circulateDesktopApplications();
    bool compositingActive();
    bool waitForCompositingSetup();
    void toggleTiling();
    void nextTileLayout();
    void previousTileLayout();
    bool stopActivity(const QString &id);
    bool startActivity(const QString &id);

    void setCurrentScreen(int new_screen);

    QString desktopName(int desk) const;
    void setShowingDesktop(bool showing);
    void resetShowingDesktop(bool keep_hidden);
    bool showingDesktop() const;

    bool isNotManaged(const QString& title);    // TODO: Setter or getter?

    void sendPingToWindow(Window w, Time timestamp);   // Called from Client::pingWindow()
    void sendTakeActivity(Client* c, Time timestamp, long flags);   // Called from Client::takeActivity()

    void removeClient(Client*, allowed_t);   // Only called from Client::destroyClient() or Client::releaseWindow()
    void setActiveClient(Client*, allowed_t);
    Group* findGroup(Window leader) const;
    void addGroup(Group* group, allowed_t);
    void removeGroup(Group* group, allowed_t);
    Group* findClientLeaderGroup(const Client* c) const;

    void removeUnmanaged(Unmanaged*, allowed_t);   // Only called from Unmanaged::release()
    void removeDeleted(Deleted*, allowed_t);
    void addDeleted(Deleted*, allowed_t);

    bool checkStartupNotification(Window w, KStartupInfoId& id, KStartupInfoData& data);

    void focusToNull(); // SELI TODO: Public?
    enum FocusChainChange {
        FocusChainMakeFirst,
        FocusChainMakeLast,
        FocusChainUpdate
    };
    void updateFocusChains(Client* c, FocusChainChange change);

    bool forcedGlobalMouseGrab() const;
    void clientShortcutUpdated(Client* c);
    bool shortcutAvailable(const KShortcut& cut, Client* ignore = NULL) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcuts(bool disable);
    void disableGlobalShortcutsForClient(bool disable);
    QPoint cursorPos() const;

    void sessionSaveStarted();
    void sessionSaveDone();
    void setWasUserInteraction();
    bool wasUserInteraction() const;
    bool sessionSaving() const;

    int packPositionLeft(const Client* cl, int oldx, bool left_edge) const;
    int packPositionRight(const Client* cl, int oldx, bool right_edge) const;
    int packPositionUp(const Client* cl, int oldy, bool top_edge) const;
    int packPositionDown(const Client* cl, int oldy, bool bottom_edge) const;

    static QStringList configModules(bool controlCenter);

    void cancelDelayFocus();
    void requestDelayFocus(Client*);
    void updateFocusMousePosition(const QPoint& pos);
    QPoint focusMousePosition() const;

    void toggleTopDockShadows(bool on);

    // when adding repaints caused by a window, you probably want to use
    // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
    void addRepaint(const QRect& r);
    void addRepaint(const QRegion& r);
    void addRepaint(int x, int y, int w, int h);
    void checkUnredirect(bool force = false);
    void checkCompositeTimer();

    // Mouse polling
    void startMousePolling();
    void stopMousePolling();

    Client* getMovingClient() {
        return movingClient;
    }

public slots:
    void addRepaintFull();
    void refresh();

    // Keybindings
    void slotSwitchDesktopNext();
    void slotSwitchDesktopPrevious();
    void slotSwitchDesktopRight();
    void slotSwitchDesktopLeft();
    void slotSwitchDesktopUp();
    void slotSwitchDesktopDown();

    void slotSwitchToDesktop();
    //void slotSwitchToWindow( int );
    void slotWindowToDesktop();

    //void slotWindowToListPosition( int );
    void slotSwitchToScreen();
    void slotWindowToScreen();
    void slotSwitchToNextScreen();
    void slotWindowToNextScreen();
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
    void slotWindowQuickTileLeft();
    void slotWindowQuickTileRight();
    void slotWindowQuickTileTopLeft();
    void slotWindowQuickTileTopRight();
    void slotWindowQuickTileBottomLeft();
    void slotWindowQuickTileBottomRight();

    void slotSwitchWindowUp();
    void slotSwitchWindowDown();
    void slotSwitchWindowRight();
    void slotSwitchWindowLeft();

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

    void slotDisableGlobalShortcuts();

    void slotSettingsChanged(int category);

    void reconfigure();
    void slotReconfigure();
    void slotReinitCompositing();
    void resetCompositing();

    void slotKillWindow();

    void slotSetupWindowShortcut();
    void setupWindowShortcutDone(bool);
    void slotToggleCompositing();

    void updateClientArea();
    void suspendCompositing();
    void suspendCompositing(bool suspend);

    // NOTE: debug method
    void dumpTiles() const;

    void slotSwitchToTabLeft(); // Slot to move left the active Client.
    void slotSwitchToTabRight(); // Slot to move right the active Client.
    void slotRemoveFromGroup(); // Slot to remove the active client from its group.

private slots:
    void groupTabPopupAboutToShow(); // Popup to add to another group
    void switchToTabPopupAboutToShow(); // Popup to move in the group
    void slotAddToTabGroup(QAction*);   // Add client to a group
    void slotSwitchToTab(QAction*);   // Change the tab
    void desktopPopupAboutToShow();
    void activityPopupAboutToShow();
    void clientPopupAboutToShow();
    void slotSendToDesktop(QAction*);
    void slotToggleOnActivity(QAction*);
    void clientPopupActivated(QAction*);
    void configureWM();
    void desktopResized();
    void screenChangeTimeout();
    void screenAdded(Kephal::Screen*);
    void screenRemoved(int);
    void screenResized(Kephal::Screen*, QSize, QSize);
    void screenMoved(Kephal::Screen*, QPoint, QPoint);
    void slotUpdateToolWindows();
    void delayFocus();
    void gotTemporaryRulesMessage(const QString&);
    void cleanupTemporaryRules();
    void writeWindowRules();
    void slotBlockShortcuts(int data);
    void slotReloadConfig();
    void setupCompositing();
    void finishCompositing();
    void fallbackToXRenderCompositing();
    void performCompositing();
    void performMousePoll();
    void lostCMSelection();
    void resetCursorPosTime();
    void delayedCheckUnredirect();

    void updateCurrentActivity(const QString &new_activity);
    void activityRemoved(const QString &activity);
    void activityAdded(const QString &activity);
    void reallyStopActivity(const QString &id);   //dbus deadlocks suck

protected:
    void timerEvent(QTimerEvent *te);

Q_SIGNALS:
    Q_SCRIPTABLE void compositingToggled(bool active);

    //Signals required for the scripting interface
signals:
    void desktopPresenceChanged(KWin::Client*, int);
    void currentDesktopChanged(int);
    void numberDesktopsChanged(int oldNumberOfDesktops);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::Client*);
    void clientActivated(KWin::Client*);
    void groupAdded(KWin::Group*);
    void unmanagedAdded(KWin::Unmanaged*);
    void deletedRemoved(KWin::Deleted*);
    void mouseChanged(const QPoint& pos, const QPoint& oldpos,
                      Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                      Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void propertyNotify(long a);
    void configChanged();

private:
    void init();
    void initShortcuts();
    void initDesktopPopup();
    void initActivityPopup();
    void discardPopup();
    void setupWindowShortcut(Client* c);
    void checkCursorPos();

    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest
    };
    void switchWindow(Direction direction);

    void propagateClients(bool propagate_new_clients);   // Called only from updateStackingOrder
    ClientList constrainedStackingOrder();
    void raiseClientWithinApplication(Client* c);
    void lowerClientWithinApplication(Client* c);
    bool allowFullClientRaising(const Client* c, Time timestamp);
    bool keepTransientAbove(const Client* mainwindow, const Client* transient);
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(Window w, const XWindowAttributes& attr);
    void saveOldScreenSizes();

    /// This is the right way to create a new client
    Client* createClient(Window w, bool is_mapped);
    void addClient(Client* c, allowed_t);
    Unmanaged* createUnmanaged(Window w);
    void addUnmanaged(Unmanaged* c, allowed_t);

    Window findSpecialEventWindow(XEvent* e);

    void randomPlacement(Client* c);
    void smartPlacement(Client* c);
    void cascadePlacement(Client* c, bool re_init = false);

    // Desktop names and number of desktops
    void loadDesktopSettings();
    void saveDesktopSettings();

    //---------------------------------------------------------------------

    void helperDialog(const QString& message, const Client* c);

    QMenu* clientPopup();
    void closeActivePopup();
    void updateClientArea(bool force);

    bool windowRepaintsPending() const;
    void setCompositeTimer();

    QVector<int> desktop_focus_chain;

    QWidget* active_popup;
    Client* active_popup_client;

    void loadSessionInfo();
    void addSessionInfo(KConfigGroup &cg);
    void loadSubSessionInfo(const QString &name);

    void loadWindowRules();
    void editWindowRules(Client* c, bool whole_app);

    QList<SessionInfo*> session;
    QList<Rules*> rules;
    KXMessages temporaryRulesMessages;
    QTimer rulesUpdatedTimer;
    QTimer screenChangedTimer;
    bool rules_updates_disabled;
    static const char* windowTypeToTxt(NET::WindowType type);
    static NET::WindowType txtToWindowType(const char* txt);
    static bool sessionInfoWindowTypeMatch(Client* c, SessionInfo* info);

    Client* active_client;
    Client* last_active_client;
    Client* most_recently_raised; // Used ONLY by raiseOrLowerClient()
    Client* movingClient;
    Client* pending_take_activity;
    int active_screen;

    // Delay(ed) window focus timer and client
    QTimer* delayFocusTimer;
    Client* delayfocus_client;
    QPoint focusMousePos;

    ClientList clients;
    ClientList desktops;
    UnmanagedList unmanaged;
    DeletedList deleted;

    ClientList unconstrained_stacking_order; // Topmost last
    ClientList stacking_order; // Topmost last
    bool force_restacking;
    mutable ToplevelList x_stacking; // From XQueryTree()
    mutable bool x_stacking_dirty;
    QVector< ClientList > focus_chain; // Currently ative last
    ClientList global_focus_chain; // This one is only for things like tabbox's MRU
    ClientList should_get_focus; // Last is most recent
    ClientList attention_chain;

    bool showing_desktop;
    ClientList showing_desktop_clients;
    int block_showing_desktop;

    GroupList groups;

    bool was_user_interaction;
    bool session_saving;
    int session_active_client;
    int session_desktop;

    int block_focus;

#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox* tab_box;
#endif
#ifdef KWIN_BUILD_DESKTOPCHANGEOSD
    DesktopChangeOSD* desktop_change_osd;
#endif

    QMenu* popup;
    QMenu* advanced_popup;
    QMenu* desk_popup;
    QMenu* activity_popup;
    QMenu* add_tabs_popup; // Menu to add the group to other group
    QMenu* switch_to_tab_popup; // Menu to change tab

    void modalActionsSwitch(bool enabled);

    KActionCollection* keys;
    KActionCollection* client_keys;
    KActionCollection* disable_shortcuts_keys;
    QAction* mResizeOpAction;
    QAction* mMoveOpAction;
    QAction* mMaximizeOpAction;
    QAction* mShadeOpAction;
    QAction* mTilingStateOpAction;
    QAction* mKeepAboveOpAction;
    QAction* mKeepBelowOpAction;
    QAction* mFullScreenOpAction;
    QAction* mNoBorderOpAction;
    QAction* mMinimizeOpAction;
    QAction* mCloseOpAction;
    QAction* mRemoveTabGroup; // Remove client from group
    QAction* mCloseGroup; // Close all clients in the group
    ShortcutDialog* client_keys_dialog;
    Client* client_keys_client;
    bool global_shortcuts_disabled;
    bool global_shortcuts_disabled_for_client;

    void initAddToTabGroup(); // Load options for menu add_tabs_popup
    void initSwitchToTab(); // Load options for menu switch_to_tab_popup

    PluginMgr* mgr;

    RootInfo* rootInfo;
    QWidget* supportWindow;

    // Swallowing
    QStringList doNotManageList;

    // Colormap handling
    Colormap default_colormap;
    Colormap installed_colormap;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace* _self;

    bool workspaceInit;

    KStartupInfo* startup;

    Placement* initPositioning;

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
    Window null_focus_window;
    bool forced_global_mouse_grab;
    friend class StackingUpdatesBlocker;

    KSelectionOwner* cm_selection;
    bool compositingSuspended, compositingBlocked;
    QBasicTimer compositeTimer;
    QElapsedTimer nextPaintReference;
    QTimer mousePollingTimer;
    uint vBlankInterval, vBlankPadding, fpsInterval, estimatedRenderTime;
    int xrrRefreshRate; // used only for compositing
    QRegion repaints_region;
    QSlider* transSlider;
    QPushButton* transButton;
    QTimer unredirectTimer;
    bool forceUnredirectCheck;
    QTimer compositeResetTimer; // for compressing composite resets
    bool m_finishingCompositing; // finishCompositing() sets this variable while shutting down

private:
    friend bool performTransiencyCheck();
};

/**
 * Helper for Workspace::blockStackingUpdates() being called in pairs (True/false)
 */
class StackingUpdatesBlocker
{
public:
    StackingUpdatesBlocker(Workspace* w)
        : ws(w) {
        ws->blockStackingUpdates(true);
    }
    ~StackingUpdatesBlocker() {
        ws->blockStackingUpdates(false);
    }

private:
    Workspace* ws;
};

/**
 * NET WM Protocol handler class
 */
class RootInfo : public NETRootInfo
{
private:
    typedef KWin::Client Client;  // Because of NET::Client

public:
    RootInfo(Workspace* ws, Display* dpy, Window w, const char* name, unsigned long pr[],
             int pr_num, int scr = -1);

protected:
    virtual void changeNumberOfDesktops(int n);
    virtual void changeCurrentDesktop(int d);
    virtual void changeActiveWindow(Window w, NET::RequestSource src, Time timestamp, Window active_window);
    virtual void closeWindow(Window w);
    virtual void moveResize(Window w, int x_root, int y_root, unsigned long direction);
    virtual void moveResizeWindow(Window w, int flags, int x, int y, int width, int height);
    virtual void gotPing(Window w, Time timestamp);
    virtual void restackWindow(Window w, RequestSource source, Window above, int detail, Time timestamp);
    virtual void gotTakeActivity(Window w, Time timestamp, long flags);
    virtual void changeShowingDesktop(bool showing);

private:
    Workspace* workspace;
};

//---------------------------------------------------------
// Desktop layout

inline int Workspace::numberOfDesktops() const
{
    return desktopCount_;
}

inline QSize Workspace::desktopGridSize() const
{
    return desktopGridSize_;
}

inline int Workspace::desktopGridWidth() const
{
    return desktopGridSize_.width();
}

inline int Workspace::desktopGridHeight() const
{
    return desktopGridSize_.height();
}

inline int Workspace::workspaceWidth() const
{
    return desktopGridSize_.width() * displayWidth();
}

inline int Workspace::workspaceHeight() const
{
    return desktopGridSize_.height() * displayHeight();
}

inline int Workspace::currentDesktop() const
{
    return currentDesktop_;
}

inline int Workspace::desktopAtCoords(QPoint coords) const
{
    return desktopGrid_[coords.y() * desktopGridSize_.width() + coords.x()];
}

//---------------------------------------------------------
// Unsorted

inline bool Workspace::initializing() const
{
    return workspaceInit;
}

inline Client* Workspace::activeClient() const
{
    return active_client;
}

inline Client* Workspace::mostRecentlyActivatedClient() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : active_client;
}

inline void Workspace::addGroup(Group* group, allowed_t)
{
    emit groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group* group, allowed_t)
{
    groups.removeAll(group);
}

inline const ClientList& Workspace::stackingOrder() const
{
    // TODO: Q_ASSERT( block_stacking_updates == 0 );
    return stacking_order;
}

inline void Workspace::showWindowMenu(QPoint pos, Client* cl)
{
    showWindowMenu(QRect(pos, pos), cl);
}

inline void Workspace::showWindowMenu(int x, int y, Client* cl)
{
    showWindowMenu(QRect(QPoint(x, y), QPoint(x, y)), cl);
}

inline void Workspace::setWasUserInteraction()
{
    was_user_interaction = true;
}

inline bool Workspace::wasUserInteraction() const
{
    return was_user_interaction;
}

inline void Workspace::sessionSaveStarted()
{
    session_saving = true;
}

inline bool Workspace::sessionSaving() const
{
    return session_saving;
}

inline bool Workspace::forcedGlobalMouseGrab() const
{
    return forced_global_mouse_grab;
}

inline bool Workspace::showingDesktop() const
{
    return showing_desktop;
}

inline bool Workspace::globalShortcutsDisabled() const
{
    return global_shortcuts_disabled || global_shortcuts_disabled_for_client;
}

inline bool Workspace::rulesUpdatesDisabled() const
{
    return rules_updates_disabled;
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

template< typename T >
inline Client* Workspace::findClient(T predicate) const
{
    if (Client* ret = findClientInList(clients, predicate))
        return ret;
    if (Client* ret = findClientInList(desktops, predicate))
        return ret;
    return NULL;
}

template< typename T1, typename T2 >
inline void Workspace::forEachClient(T1 procedure, T2 predicate)
{
    for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
        if (predicate(const_cast<const Client*>(*it)))
            procedure(*it);
    for (ClientList::ConstIterator it = desktops.constBegin(); it != desktops.constEnd(); ++it)
        if (predicate(const_cast<const Client*>(*it)))
            procedure(*it);
}

template< typename T >
inline void Workspace::forEachClient(T procedure)
{
    return forEachClient(procedure, TruePredicate());
}

template< typename T >
inline Unmanaged* Workspace::findUnmanaged(T predicate) const
{
    return findUnmanagedInList(unmanaged, predicate);
}

template< typename T1, typename T2 >
inline void Workspace::forEachUnmanaged(T1 procedure, T2 predicate)
{
    for (UnmanagedList::ConstIterator it = unmanaged.constBegin(); it != unmanaged.constEnd(); ++it)
        if (predicate(const_cast<const Unmanaged*>(*it)))
            procedure(*it);
}

template< typename T >
inline void Workspace::forEachUnmanaged(T procedure)
{
    return forEachUnmanaged(procedure, TruePredicate());
}

KWIN_COMPARE_PREDICATE(ClientMatchPredicate, Client, const Client*, cl == value);

inline bool Workspace::hasClient(const Client* c)
{
    return findClient(ClientMatchPredicate(c));
}

inline void Workspace::checkCompositeTimer()
{
    if (!compositeTimer.isActive())
        setCompositeTimer();
}

inline bool Workspace::hasDecorationPlugin() const
{
    if (!mgr) {
        return false;
    }
    return !mgr->hasNoDecoration();
}

inline bool Workspace::hasDecorationShadows() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityProvidesShadow);
}

inline Qt::Corner Workspace::decorationCloseButtonCorner()
{
    if (!hasDecorationPlugin()) {
        return Qt::TopRightCorner;
    }
    return mgr->factory()->closeButtonCorner();
}

inline bool Workspace::decorationHasAlpha() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityUsesAlphaChannel);
}

inline bool Workspace::decorationSupportsClientGrouping() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityClientGrouping);
}

inline bool Workspace::decorationSupportsFrameOverlap() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityExtendIntoClientArea);
}

inline bool Workspace::decorationSupportsBlurBehind() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityUsesBlurBehind);
}

inline void Workspace::addClientGroup(ClientGroup* group)
{
    clientGroups.append(group);
}

} // namespace

#endif
