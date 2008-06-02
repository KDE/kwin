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

#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

#include <QTimer>
#include <QVector>
#include <kshortcut.h>
#include <QCursor>
#include <netwm.h>
#include <kxmessages.h>
#include <QDateTime>
#include <kmanagerselection.h>

#include "utils.h"
#include "kdecoration.h"
#include "sm.h"

#include <X11/Xlib.h>

class QMenu;
class KConfig;
class KActionCollection;
class KStartupInfo;
class KStartupInfoId;
class KStartupInfoData;
class QSlider;
class QPushButton;

namespace KWin
{

class Client;
class TabBox;
class PopupInfo;
class RootInfo;
class PluginMgr;
class Placement;
class Rules;
class WindowRules;

class Workspace : public QObject, public KDecorationDefines
    {
    Q_OBJECT
    public:
        Workspace( bool restore = false );
        virtual ~Workspace();

        static Workspace * self() { return _self; }

        bool workspaceEvent( XEvent * );
        bool workspaceEvent( QEvent * );

        KDecoration* createDecoration( KDecorationBridge* bridge );

        bool hasClient( const Client * );

        template< typename T > Client* findClient( T predicate ) const;
        template< typename T1, typename T2 > void forEachClient( T1 procedure, T2 predicate );
        template< typename T > void forEachClient( T procedure );
        template< typename T > Unmanaged* findUnmanaged( T predicate ) const;
        template< typename T1, typename T2 > void forEachUnmanaged( T1 procedure, T2 predicate );
        template< typename T > void forEachUnmanaged( T procedure );

        QRect clientArea( clientAreaOption, const QPoint& p, int desktop ) const;
        QRect clientArea( clientAreaOption, const Client* c ) const;
        QRect clientArea( clientAreaOption, int screen, int desktop ) const;

        /**
         * @internal
         */
        void killWindowId( Window window);

        void killWindow() { slotKillWindow(); }

        bool initializing() const;

        /**
         * Returns the active client, i.e. the client that has the focus (or None
         * if no client has the focus)
         */
        Client* activeClient() const;
        // Client that was activated, but it's not yet really activeClient(), because
        // we didn't process yet the matching FocusIn event. Used mostly in focus
        // stealing prevention code.
        Client* mostRecentlyActivatedClient() const;

        void activateClient( Client*, bool force = false  );
        void requestFocus( Client* c, bool force = false );
        void takeActivity( Client* c, int flags, bool handled ); // flags are ActivityFlags
        void handleTakeActivity( Client* c, Time timestamp, int flags ); // flags are ActivityFlags
        bool allowClientActivation( const Client* c, Time time = -1U, bool focus_in = false, bool ignore_desktop = false );
        void restoreFocus();
        void gotFocusIn( const Client* );
        void setShouldGetFocus( Client* );
        bool fakeRequestedActivity( Client* c );
        void unfakeActivity( Client* c );
        bool activateNextClient( Client* c );
        bool focusChangeEnabled() { return block_focus == 0; }

        void updateColormap();

        /**
         * Indicates that the client c is being moved around by the user.
         */
        void setClientIsMoving( Client *c );

        void place( Client *c, QRect& area );
        void placeSmart( Client* c, const QRect& area );

        QPoint adjustClientPosition( Client* c, QPoint pos );
        QRect adjustClientSize( Client* c, QRect moveResizeGeom, int mode );
        void raiseClient( Client* c, bool nogroup = false );
        void lowerClient( Client* c, bool nogroup = false );
        void raiseClientRequest( Client* c, NET::RequestSource src, Time timestamp );
        void lowerClientRequest( Client* c, NET::RequestSource src, Time timestamp );
        void restackClientUnderActive( Client* );
        void updateClientLayer( Client* c );
        void raiseOrLowerClient( Client * );
        void restoreSessionStackingOrder( Client* c );
        void reconfigure();
        void forceRestacking();

        void clientHidden( Client*  );
        void clientAttentionChanged( Client* c, bool set );

        void checkElectricBorder(const QPoint &pos, Time time);
        void reserveElectricBorder( ElectricBorder border );
        void unreserveElectricBorder( ElectricBorder border );
        void reserveElectricBorderSwitching( bool reserve );

        /**
         * Returns the current virtual desktop of this workspace
         */
        int currentDesktop() const;
        /**
         * Returns the number of virtual desktops of this workspace
         */
        int numberOfDesktops() const;
        void setNumberOfDesktops( int n );
        void calcDesktopLayout(int* x, int* y, Qt::Orientation* orientation) const;
        int desktopToRight( int desktop, bool wrap ) const;
        int desktopToLeft( int desktop, bool wrap ) const;
        int desktopUp( int desktop, bool wrap ) const;
        int desktopDown( int desktop, bool wrap ) const;

        int activeScreen() const;
        int numScreens() const;
        void checkActiveScreen( const Client* c );
        void setActiveScreenMouse( const QPoint &mousepos );
        QRect screenGeometry( int screen ) const;
        int screenNumber( const QPoint &pos ) const;

    // for TabBox
        Client* currentTabBoxClient() const;
        ClientList currentTabBoxClientList() const;
        int currentTabBoxDesktop() const;
        QList< int > currentTabBoxDesktopList() const;
        void setTabBoxClient(Client*);
        void setTabBoxDesktop(int);
        Client* nextClientFocusChain(Client*) const;
        Client* previousClientFocusChain(Client*) const;
        Client* nextClientStatic(Client*) const;
        Client* previousClientStatic(Client*) const;
        int nextDesktopFocusChain( int iDesktop ) const;
        int previousDesktopFocusChain( int iDesktop ) const;
        int nextDesktopStatic( int iDesktop ) const;
        int previousDesktopStatic( int iDesktop ) const;
        void refTabBox();
        void unrefTabBox();
        void closeTabBox();

         /**
         * Returns the list of clients sorted in stacking order, with topmost client
         * at the last position
         */
        const ClientList& stackingOrder() const;

        ClientList ensureStackingOrder( const ClientList& clients ) const;

        Client* topClientOnDesktop( int desktop, bool unconstrained = false, bool only_normal = true ) const;
        Client* findDesktop( bool topmost, int desktop ) const;
        void sendClientToDesktop( Client* c, int desktop, bool dont_activate );
        void windowToPreviousDesktop( Client* c );
        void windowToNextDesktop( Client* c );
        void sendClientToScreen( Client* c, int screen );

    // KDE4 remove me - and it's also in the DCOP interface :(
        void showWindowMenuAt( unsigned long id, int x, int y );

        void loadEffect( const QString& name );
        void toggleEffect( const QString& name );
        void reloadEffect( const QString& name );

        void unloadEffect( const QString& name );

        QStringList loadedModules() const;


        /**
	 * Shows the menu operations menu for the client and makes it active if
	 * it's not already.
         */
        void showWindowMenu( const QRect &pos, Client* cl );
	/**
	 * Backwards compatibility.
	 */
        void showWindowMenu( int x, int y, Client* cl );
        void showWindowMenu( QPoint pos, Client* cl );

        void updateMinimizedOfTransients( Client* );
        void updateOnAllDesktopsOfTransients( Client* );
        void checkTransients( Window w );

        void performWindowOperation( Client* c, WindowOperation op );

        void storeSession( KConfig* config, SMSavePhase phase );

        SessionInfo* takeSessionInfo( Client* );
        WindowRules findWindowRules( const Client*, bool );
        void rulesUpdated();
        void discardUsedWindowRules( Client* c, bool withdraw );
        void disableRulesUpdates( bool disable );
        bool rulesUpdatesDisabled() const;

    // dcop interface
        void cascadeDesktop();
        void unclutterDesktop();
        void doNotManage( const QString & );
        QList< int > decorationSupportedColors() const;
        bool setCurrentDesktop( int new_desktop );
        void nextDesktop();
        void previousDesktop();
        void circulateDesktopApplications();
        void setCurrentScreen( int new_screen );

        QString desktopName( int desk ) const;
        void updateDesktopLayout();
        void setShowingDesktop( bool showing );
        void resetShowingDesktop( bool keep_hidden );
        bool showingDesktop() const;

        bool isNotManaged( const QString& title );  // ### setter or getter ?

        void sendPingToWindow( Window w, Time timestamp ); // called from Client::pingWindow()
        void sendTakeActivity( Client* c, Time timestamp, long flags ); // called from Client::takeActivity()

        void removeClient( Client*, allowed_t ); // only called from Client::destroyClient() or Client::releaseWindow()
        void setActiveClient( Client*, allowed_t );
        Group* findGroup( Window leader ) const;
        void addGroup( Group* group, allowed_t );
        void removeGroup( Group* group, allowed_t );
        Group* findClientLeaderGroup( const Client* c ) const;

        void removeUnmanaged( Unmanaged*, allowed_t ); // only called from Unmanaged::release()
        void removeDeleted( Deleted*, allowed_t );
        void addDeleted( Deleted*, allowed_t );

        bool checkStartupNotification( Window w, KStartupInfoId& id, KStartupInfoData& data );

        void focusToNull(); // SELI public?
        enum FocusChainChange { FocusChainMakeFirst, FocusChainMakeLast, FocusChainUpdate };
        void updateFocusChains( Client* c, FocusChainChange change );

        bool forcedGlobalMouseGrab() const;
        void clientShortcutUpdated( Client* c );
        bool shortcutAvailable( const KShortcut& cut, Client* ignore = NULL ) const;
        bool globalShortcutsDisabled() const;
        void disableGlobalShortcuts( bool disable );
        void disableGlobalShortcutsForClient( bool disable );
        QPoint cursorPos() const;

        void sessionSaveStarted();
        void sessionSaveDone();
        void setWasUserInteraction();
        bool wasUserInteraction() const;
        bool sessionSaving() const;

        bool managingTopMenus() const;
        int topMenuHeight() const;
        void updateCurrentTopMenu();

        int packPositionLeft( const Client* cl, int oldx, bool left_edge ) const;
        int packPositionRight( const Client* cl, int oldx, bool right_edge ) const;
        int packPositionUp( const Client* cl, int oldy, bool top_edge ) const;
        int packPositionDown( const Client* cl, int oldy, bool bottom_edge ) const;

        static QStringList configModules(bool controlCenter);

        void cancelDelayFocus();
        void requestDelayFocus( Client* );
        void updateFocusMousePosition( const QPoint& pos );
        QPoint focusMousePosition() const;

        void toggleTopDockShadows(bool on);

        // when adding repaints caused by a window, you probably want to use
        // either Toplevel::addRepaint() or Toplevel::addWorkspaceRepaint()
        void addRepaint( const QRect& r );
        void addRepaint( int x, int y, int w, int h );
        // creates XComposite overlay window, call initOverlay() afterwards
        bool createOverlay();
        // init overlay and the destination window in it
        void setupOverlay( Window window );
        void showOverlay();
        // destroys XComposite overlay window
        void destroyOverlay();
        Window overlayWindow();

    public slots:
        void addRepaintFull();
        void refresh();
    // keybindings
        void slotSwitchDesktopNext();
        void slotSwitchDesktopPrevious();
        void slotSwitchDesktopRight();
        void slotSwitchDesktopLeft();
        void slotSwitchDesktopUp();
        void slotSwitchDesktopDown();

        void slotSwitchToDesktop( int );
        void slotSwitchToDesktop1() { return slotSwitchToDesktop( 1 ); }
        void slotSwitchToDesktop2() { return slotSwitchToDesktop( 2 ); }
        void slotSwitchToDesktop3() { return slotSwitchToDesktop( 3 ); }
        void slotSwitchToDesktop4() { return slotSwitchToDesktop( 4 ); }
        void slotSwitchToDesktop5() { return slotSwitchToDesktop( 5 ); }
        void slotSwitchToDesktop6() { return slotSwitchToDesktop( 6 ); }
        void slotSwitchToDesktop7() { return slotSwitchToDesktop( 7 ); }
        void slotSwitchToDesktop8() { return slotSwitchToDesktop( 8 ); }
        void slotSwitchToDesktop9() { return slotSwitchToDesktop( 9 ); }
        void slotSwitchToDesktop10() { return slotSwitchToDesktop( 10 ); }
        void slotSwitchToDesktop11() { return slotSwitchToDesktop( 11 ); }
        void slotSwitchToDesktop12() { return slotSwitchToDesktop( 12 ); }
        void slotSwitchToDesktop13() { return slotSwitchToDesktop( 13 ); }
        void slotSwitchToDesktop14() { return slotSwitchToDesktop( 14 ); }
        void slotSwitchToDesktop15() { return slotSwitchToDesktop( 15 ); }
        void slotSwitchToDesktop16() { return slotSwitchToDesktop( 16 ); }
        void slotSwitchToDesktop17() { return slotSwitchToDesktop( 17 ); }
        void slotSwitchToDesktop18() { return slotSwitchToDesktop( 18 ); }
        void slotSwitchToDesktop19() { return slotSwitchToDesktop( 19 ); }
        void slotSwitchToDesktop20() { return slotSwitchToDesktop( 20 ); }
    //void slotSwitchToWindow( int );
        void slotWindowToDesktop( int );
        void slotWindowToDesktop1() { return slotWindowToDesktop( 1 ); }
        void slotWindowToDesktop2() { return slotWindowToDesktop( 2 ); }
        void slotWindowToDesktop3() { return slotWindowToDesktop( 3 ); }
        void slotWindowToDesktop4() { return slotWindowToDesktop( 4 ); }
        void slotWindowToDesktop5() { return slotWindowToDesktop( 5 ); }
        void slotWindowToDesktop6() { return slotWindowToDesktop( 6 ); }
        void slotWindowToDesktop7() { return slotWindowToDesktop( 7 ); }
        void slotWindowToDesktop8() { return slotWindowToDesktop( 8 ); }
        void slotWindowToDesktop9() { return slotWindowToDesktop( 9 ); }
        void slotWindowToDesktop10() { return slotWindowToDesktop( 10 ); }
        void slotWindowToDesktop11() { return slotWindowToDesktop( 11 ); }
        void slotWindowToDesktop12() { return slotWindowToDesktop( 12 ); }
        void slotWindowToDesktop13() { return slotWindowToDesktop( 13 ); }
        void slotWindowToDesktop14() { return slotWindowToDesktop( 14 ); }
        void slotWindowToDesktop15() { return slotWindowToDesktop( 15 ); }
        void slotWindowToDesktop16() { return slotWindowToDesktop( 16 ); }
        void slotWindowToDesktop17() { return slotWindowToDesktop( 17 ); }
        void slotWindowToDesktop18() { return slotWindowToDesktop( 18 ); }
        void slotWindowToDesktop19() { return slotWindowToDesktop( 19 ); }
        void slotWindowToDesktop20() { return slotWindowToDesktop( 20 ); }
    //void slotWindowToListPosition( int );
        void slotSwitchToScreen( int );
        void slotSwitchToScreen0() { return slotSwitchToScreen( 0 ); }
        void slotSwitchToScreen1() { return slotSwitchToScreen( 1 ); }
        void slotSwitchToScreen2() { return slotSwitchToScreen( 2 ); }
        void slotSwitchToScreen3() { return slotSwitchToScreen( 3 ); }
        void slotSwitchToScreen4() { return slotSwitchToScreen( 4 ); }
        void slotSwitchToScreen5() { return slotSwitchToScreen( 5 ); }
        void slotSwitchToScreen6() { return slotSwitchToScreen( 6 ); }
        void slotSwitchToScreen7() { return slotSwitchToScreen( 7 ); }
        void slotWindowToScreen( int );
        void slotWindowToScreen0() { return slotWindowToScreen( 0 ); }
        void slotWindowToScreen1() { return slotWindowToScreen( 1 ); }
        void slotWindowToScreen2() { return slotWindowToScreen( 2 ); }
        void slotWindowToScreen3() { return slotWindowToScreen( 3 ); }
        void slotWindowToScreen4() { return slotWindowToScreen( 4 ); }
        void slotWindowToScreen5() { return slotWindowToScreen( 5 ); }
        void slotWindowToScreen6() { return slotWindowToScreen( 6 ); }
        void slotWindowToScreen7() { return slotWindowToScreen( 7 ); }
        void slotSwitchToNextScreen();
        void slotWindowToNextScreen();

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

        void slotWalkThroughDesktops();
        void slotWalkBackThroughDesktops();
        void slotWalkThroughDesktopList();
        void slotWalkBackThroughDesktopList();
        void slotWalkThroughWindows();
        void slotWalkBackThroughWindows();

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

        void slotMouseEmulation();
        void slotDisableGlobalShortcuts();

        void slotSettingsChanged( int category );

        void slotReconfigure();
        void slotReinitCompositing();

        void slotKillWindow();

        void slotGrabWindow();
        void slotGrabDesktop();

        void slotSetupWindowShortcut();
        void setupWindowShortcutDone( bool );
        void slotToggleCompositing();

        void updateClientArea();

    private slots:
        void desktopPopupAboutToShow();
        void clientPopupAboutToShow();
        void slotSendToDesktop( QAction* );
        void clientPopupActivated( QAction* );
        void configureWM();
        void desktopResized();
        void slotUpdateToolWindows();
        void lostTopMenuSelection();
        void lostTopMenuOwner();
        void delayFocus();
        void gotTemporaryRulesMessage( const QString& );
        void cleanupTemporaryRules();
        void writeWindowRules();
        void slotBlockShortcuts(int data);
        void slotReloadConfig();
        void setPopupClientOpacity( QAction* action );
        void setupCompositing();
        void performCompositing();
        void lostCMSelection();
        void updateElectricBorders();
        void resetCursorPosTime();

    protected:
        bool keyPressMouseEmulation( XKeyEvent& ev );

    private:
        void init();
        void initShortcuts();
        void readShortcuts();
        void initDesktopPopup();
        void setupWindowShortcut( Client* c );
        void checkCursorPos();

        bool startKDEWalkThroughWindows();
        bool startWalkThroughDesktops( TabBoxMode mode ); // TabBoxDesktopMode | TabBoxDesktopListMode
        bool startWalkThroughDesktops();
        bool startWalkThroughDesktopList();
        void KDEWalkThroughWindows( bool forward );
        void CDEWalkThroughWindows( bool forward );
        void walkThroughDesktops( bool forward );
        void KDEOneStepThroughWindows( bool forward );
        void oneStepThroughDesktops( bool forward, TabBoxMode mode ); // TabBoxDesktopMode | TabBoxDesktopListMode
        void oneStepThroughDesktops( bool forward );
        void oneStepThroughDesktopList( bool forward );
        bool establishTabBoxGrab();
        void removeTabBoxGrab();

        void updateStackingOrder( bool propagate_new_clients = false );
        ToplevelList rootStackingOrder() const;
        void propagateClients( bool propagate_new_clients ); // called only from updateStackingOrder
        ClientList constrainedStackingOrder();
        void raiseClientWithinApplication( Client* c );
        void lowerClientWithinApplication( Client* c );
        bool allowFullClientRaising( const Client* c, Time timestamp );
        bool keepTransientAbove( const Client* mainwindow, const Client* transient );
        void blockStackingUpdates( bool block );
        void addTopMenu( Client* c );
        void removeTopMenu( Client* c );
        void setupTopMenuHandling();
        void updateTopMenuGeometry( Client* c = NULL );
        void updateToolWindows( bool also_hide );

    // this is the right way to create a new client
        Client* createClient( Window w, bool is_mapped );
        void addClient( Client* c, allowed_t );
        Unmanaged* createUnmanaged( Window w );
        void addUnmanaged( Unmanaged* c, allowed_t );

        Window findSpecialEventWindow( XEvent* e );

        void randomPlacement(Client* c);
        void smartPlacement(Client* c);
        void cascadePlacement(Client* c, bool re_init = false);

    // desktop names and number of desktops
        void loadDesktopSettings();
        void saveDesktopSettings();

    // mouse emulation
        WId getMouseEmulationWindow();
        enum MouseEmulation { EmuPress, EmuRelease, EmuMove };
        unsigned int sendFakedMouseEvent( const QPoint &pos, WId win, MouseEmulation type, int button, unsigned int state ); // returns the new state

        void tabBoxKeyPress( int key );
        void tabBoxKeyRelease( const XKeyEvent& ev );

    // electric borders
        void destroyElectricBorders();
        bool electricBorderEvent(XEvent * e);
        void electricBorderSwitchDesktop( ElectricBorder border, const QPoint& pos );

    // ------------------

        void helperDialog( const QString& message, const Client* c );

        QMenu* clientPopup();
        void closeActivePopup();

        void updateClientArea( bool force );

        void finishCompositing();
        bool windowRepaintsPending() const;

        int current_desktop;
        int number_of_desktops;
        QVector<int> desktop_focus_chain;

        QWidget* active_popup;
        Client* active_popup_client;

        void loadSessionInfo();
        void loadWindowRules();
        void editWindowRules( Client* c, bool whole_app );

        QList<SessionInfo*> session;
        QList<Rules*> rules;
        KXMessages temporaryRulesMessages;
        QTimer rulesUpdatedTimer;
        bool rules_updates_disabled;
        static const char* windowTypeToTxt( NET::WindowType type );
        static NET::WindowType txtToWindowType( const char* txt );
        static bool sessionInfoWindowTypeMatch( Client* c, SessionInfo* info );

        Client* active_client;
        Client* last_active_client;
        Client* most_recently_raised; // used _only_ by raiseOrLowerClient()
        Client* movingClient;
        Client* pending_take_activity;
        int active_screen;

    // delay(ed) window focus timer and client
        QTimer* delayFocusTimer;
        Client* delayfocus_client;
        QPoint focusMousePos;

        ClientList clients;
        ClientList desktops;
        UnmanagedList unmanaged;
        DeletedList deleted;

        ClientList unconstrained_stacking_order; // topmost last
        ClientList stacking_order; // topmost last
        bool force_restacking;
        QVector< ClientList > focus_chain; // currently ative last
        ClientList global_focus_chain; // this one is only for things like tabbox's MRU
        ClientList should_get_focus; // last is most recent
        ClientList attention_chain;

        bool showing_desktop;
        ClientList showing_desktop_clients;
        int block_showing_desktop;

        GroupList groups;

        bool was_user_interaction;
        bool session_saving;
        int session_active_client;
        int session_desktop;

        bool control_grab;
        bool tab_grab;
    //KKeyNative walkThroughDesktopsKeycode, walkBackThroughDesktopsKeycode;
    //KKeyNative walkThroughDesktopListKeycode, walkBackThroughDesktopListKeycode;
    //KKeyNative walkThroughWindowsKeycode, walkBackThroughWindowsKeycode;
        KShortcut cutWalkThroughDesktops, cutWalkThroughDesktopsReverse;
        KShortcut cutWalkThroughDesktopList, cutWalkThroughDesktopListReverse;
        KShortcut cutWalkThroughWindows, cutWalkThroughWindowsReverse;
        bool mouse_emulation;
        unsigned int mouse_emulation_state;
        WId mouse_emulation_window;
        int block_focus;

        TabBox* tab_box;
        PopupInfo* popupinfo;

        QMenu *popup;
        QMenu *advanced_popup;
        QMenu *trans_popup;
        QMenu *desk_popup;

        void modalActionsSwitch( bool enabled );

        KActionCollection *keys;
        KActionCollection *client_keys;
        QAction *mResizeOpAction;
        QAction *mMoveOpAction;
        QAction *mMaximizeOpAction;
        QAction *mShadeOpAction;
        QAction *mKeepAboveOpAction;
        QAction *mKeepBelowOpAction;
        QAction *mFullScreenOpAction;
        QAction *mNoBorderOpAction;
        QAction *mMinimizeOpAction;
        QAction *mCloseOpAction;
        ShortcutDialog* client_keys_dialog;
        Client* client_keys_client;
        KActionCollection *disable_shortcuts_keys;
        bool global_shortcuts_disabled;
        bool global_shortcuts_disabled_for_client;

        PluginMgr *mgr;

        RootInfo *rootInfo;
        QWidget* supportWindow;

    // swallowing
        QStringList doNotManageList;

    // colormap handling
        Colormap default_colormap;
        Colormap installed_colormap;

    // Timer to collect requests for 'reconfigure'
        QTimer reconfigureTimer;

        QTimer updateToolWindowsTimer;

        static Workspace *_self;

        bool workspaceInit;

        KStartupInfo* startup;

        ElectricBorder electric_current_border;
        Window electric_windows[ ELECTRIC_COUNT ];
        int electricLeft;
        int electricRight;
        int electricTop;
        int electricBottom;
        Time electric_time_first;
        Time electric_time_last;
        QPoint electric_push_point;
        int electric_reserved[ ELECTRIC_COUNT ]; // corners/edges used by something

        Qt::Orientation layoutOrientation;
        int layoutX;
        int layoutY;

        Placement *initPositioning;

        QVector< QRect > workarea; //  array of workareas for virtual desktops
        QVector< QVector< QRect > > screenarea; // array of workareas per xinerama screen for all virtual desktops

        bool managing_topmenus;
        KSelectionOwner* topmenu_selection;
        KSelectionWatcher* topmenu_watcher;
        ClientList topmenus; // doesn't own them
        mutable int topmenu_height;
        QWidget* topmenu_space;

        int set_active_client_recursion;
        int block_stacking_updates; // when >0, stacking updates are temporarily disabled
        bool blocked_propagating_new_clients; // propagate also new clients after enabling stacking updates?
        Window null_focus_window;
        bool forced_global_mouse_grab;
        friend class StackingUpdatesBlocker;

        KSelectionOwner* cm_selection;
        bool compositingSuspended;
        QTimer compositeTimer;
        QTime lastCompositePaint;
        int compositeRate;
        QRegion repaints_region;
        Window overlay; // XComposite overlay window
        bool overlay_visible;
        bool overlay_shown; // for showOverlay()
        QSlider *transSlider;
        QPushButton *transButton;

    private:
        friend bool performTransiencyCheck();
    };

// helper for Workspace::blockStackingUpdates() being called in pairs (true/false)
class StackingUpdatesBlocker
    {
    public:
        StackingUpdatesBlocker( Workspace* w )
            : ws( w ) { ws->blockStackingUpdates( true ); }
        ~StackingUpdatesBlocker()
            { ws->blockStackingUpdates( false ); }
    private:
        Workspace* ws;
    };

// NET WM Protocol handler class
class RootInfo : public NETRootInfo
    {
    private:
        typedef KWin::Client Client;  // because of NET::Client
    public:
        RootInfo( Workspace* ws, Display *dpy, Window w, const char *name, unsigned long pr[], int pr_num, int scr= -1);
    protected:
        virtual void changeNumberOfDesktops(int n);
        virtual void changeCurrentDesktop(int d);
        virtual void changeActiveWindow(Window w,NET::RequestSource src, Time timestamp, Window active_window);
        virtual void closeWindow(Window w);
        virtual void moveResize(Window w, int x_root, int y_root, unsigned long direction);
        virtual void moveResizeWindow(Window w, int flags, int x, int y, int width, int height );
        virtual void gotPing(Window w, Time timestamp);
        virtual void restackWindow(Window w, RequestSource source, Window above, int detail, Time timestamp);
        virtual void gotTakeActivity(Window w, Time timestamp, long flags );
        virtual void changeShowingDesktop( bool showing );
    private:
        Workspace* workspace;
    };


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

inline int Workspace::currentDesktop() const
    {
    return current_desktop;
    }

inline int Workspace::numberOfDesktops() const
    {
    return number_of_desktops;
    }

inline void Workspace::addGroup( Group* group, allowed_t )
    {
    groups.append( group );
    }

inline void Workspace::removeGroup( Group* group, allowed_t )
    {
    groups.removeAll( group );
    }

inline const ClientList& Workspace::stackingOrder() const
    {
// TODO    Q_ASSERT( block_stacking_updates == 0 );
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

inline
void Workspace::setWasUserInteraction()
    {
    was_user_interaction = true;
    }

inline
bool Workspace::wasUserInteraction() const
    {
    return was_user_interaction;
    }

inline
bool Workspace::managingTopMenus() const
    {
    return managing_topmenus;
    }

inline void Workspace::sessionSaveStarted()
    {
    session_saving = true;
    }

inline void Workspace::sessionSaveDone()
    {
    session_saving = false;
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

inline Window Workspace::overlayWindow()
    {
    return overlay;
    }

inline
bool Workspace::rulesUpdatesDisabled() const
    {
    return rules_updates_disabled;
    }

inline
void Workspace::forceRestacking()
    {
    force_restacking = true;
    StackingUpdatesBlocker blocker( this ); // do restacking if not blocked
    }

inline
void Workspace::updateFocusMousePosition( const QPoint& pos )
    {
    focusMousePos = pos;
    }

inline
QPoint Workspace::focusMousePosition() const
    {
    return focusMousePos;
    }

template< typename T >
inline Client* Workspace::findClient( T predicate ) const
    {
    if( Client* ret = findClientInList( clients, predicate ))
        return ret;
    if( Client* ret = findClientInList( desktops, predicate ))
        return ret;
    return NULL;
    }

template< typename T1, typename T2 >
inline void Workspace::forEachClient( T1 procedure, T2 predicate )
    {
    for ( ClientList::ConstIterator it = clients.begin(); it != clients.end(); ++it)
        if ( predicate( const_cast< const Client* >( *it)))
            procedure( *it );
    for ( ClientList::ConstIterator it = desktops.begin(); it != desktops.end(); ++it)
        if ( predicate( const_cast< const Client* >( *it)))
            procedure( *it );
    }

template< typename T >
inline void Workspace::forEachClient( T procedure )
    {
    return forEachClient( procedure, TruePredicate());
    }

template< typename T >
inline Unmanaged* Workspace::findUnmanaged( T predicate ) const
    {
    return findUnmanagedInList( unmanaged, predicate );
    }

template< typename T1, typename T2 >
inline void Workspace::forEachUnmanaged( T1 procedure, T2 predicate )
    {
    for ( UnmanagedList::ConstIterator it = unmanaged.begin(); it != unmanaged.end(); ++it)
        if ( predicate( const_cast< const Unmanaged* >( *it)))
            procedure( *it );
    }

template< typename T >
inline void Workspace::forEachUnmanaged( T procedure )
    {
    return forEachUnmanaged( procedure, TruePredicate());
    }

KWIN_COMPARE_PREDICATE( ClientMatchPredicate, Client, const Client*, cl == value );
inline bool Workspace::hasClient( const Client* c )
    {
    return findClient( ClientMatchPredicate( c ));
    }

} // namespace

#endif
