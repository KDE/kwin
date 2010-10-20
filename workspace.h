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
#include <QDateTime>
#include <kmanagerselection.h>

#include "kactivitycontroller.h"

#include "plugins.h"
#include "utils.h"
#include "kdecoration.h"
#include "kdecorationfactory.h"
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

namespace KWin
{

namespace TabBox
{
class TabBox;
}

class Client;
class Tile;
class TilingLayout;
class ClientGroup;
class DesktopChangeOSD;
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

        static Workspace* self() { return _self; }

        bool workspaceEvent( XEvent* );
        bool workspaceEvent( QEvent* );

        KDecoration* createDecoration( KDecorationBridge* bridge );

        bool hasClient( const Client* );

        template<typename T> Client* findClient( T predicate ) const;
        template<typename T1, typename T2> void forEachClient( T1 procedure, T2 predicate );
        template<typename T> void forEachClient( T procedure );
        template<typename T> Unmanaged* findUnmanaged( T predicate ) const;
        template<typename T1, typename T2> void forEachUnmanaged( T1 procedure, T2 predicate );
        template<typename T> void forEachUnmanaged( T procedure );

        QRect clientArea( clientAreaOption, const QPoint& p, int desktop ) const;
        QRect clientArea( clientAreaOption, const Client* c ) const;
        QRect clientArea( clientAreaOption, int screen, int desktop ) const;

        QRegion restrictedMoveArea( int desktop, StrutAreas areas = StrutAreaAll ) const;
        QRegion previousRestrictedMoveArea( int desktop, StrutAreas areas = StrutAreaAll ) const;

        /**
         * @internal
         */
        void killWindowId( Window window);

        void killWindow()
            { slotKillWindow(); }

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

        void activateClient( Client*, bool force = false  );
        void requestFocus( Client* c, bool force = false );
        void takeActivity( Client* c, int flags, bool handled ); // Flags are ActivityFlags
        void handleTakeActivity( Client* c, Time timestamp, int flags ); // Flags are ActivityFlags
        bool allowClientActivation( const Client* c, Time time = -1U, bool focus_in = false,
            bool ignore_desktop = false );
        void restoreFocus();
        void gotFocusIn( const Client* );
        void setShouldGetFocus( Client* );
        bool fakeRequestedActivity( Client* c );
        void unfakeActivity( Client* c );
        bool activateNextClient( Client* c );
        bool focusChangeEnabled()
            { return block_focus == 0; }

        void updateColormap();

        /**
         * Indicates that the client c is being moved around by the user.
         */
        void setClientIsMoving( Client* c );

        void place( Client* c, QRect& area );
        void placeSmart( Client* c, const QRect& area );

        QPoint adjustClientPosition( Client* c, QPoint pos, bool unrestricted, double snapAdjust = 1.0 );
        QRect adjustClientSize( Client* c, QRect moveResizeGeom, int mode );
        void raiseClient( Client* c, bool nogroup = false );
        void lowerClient( Client* c, bool nogroup = false );
        void raiseClientRequest( Client* c, NET::RequestSource src, Time timestamp );
        void lowerClientRequest( Client* c, NET::RequestSource src, Time timestamp );
        void restackClientUnderActive( Client* );
        void restack( Client *c, Client *under );
        void updateClientLayer( Client* c );
        void raiseOrLowerClient( Client* );
        void restoreSessionStackingOrder( Client* c );
        void updateStackingOrder( bool propagate_new_clients = false );
        void forceRestacking();

        void clientHidden( Client* );
        void clientAttentionChanged( Client* c, bool set );

        void checkElectricBorder(const QPoint& pos, Time time);
        void restoreElectricBorderSize( ElectricBorder border );
        void reserveElectricBorder( ElectricBorder border );
        void unreserveElectricBorder( ElectricBorder border );
        void reserveElectricBorderActions( bool reserve );
        void reserveElectricBorderSwitching( bool reserve );

        //-------------------------------------------------
        // Tiling
    public:
        bool tilingEnabled() const;
        void setTilingEnabled( bool tiling );
        bool tileable( Client *c );
        void createTile( Client *c );
        // updates geometry of tiles on all desktops,
        // this rearranges the tiles.
        void updateAllTiles();

        // The notification functions are called from 
        // various points in existing code so that
        // tiling can take any action if required.
        // They are defined in tiling.cpp
        void notifyTilingWindowResize( Client *c, const QRect &moveResizeGeom, const QRect &orig );
        void notifyTilingWindowMove( Client *c, const QRect &moveResizeGeom, const QRect &orig );
        void notifyTilingWindowResizeDone( Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled );
        void notifyTilingWindowMoveDone( Client *c, const QRect &moveResizeGeom, const QRect &orig, bool canceled );
        void notifyTilingWindowDesktopChanged( Client *c, int old_desktop );
        void notifyTilingWindowActivated( Client *c );
        void notifyTilingWindowMinimizeToggled( Client *c );
        void notifyTilingWindowMaximized( Client *c, WindowOperation op );

        Position supportedTilingResizeMode( Client *c, Position currentMode );

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
        void setNumberOfDesktops( int count );
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
        bool setCurrentDesktop( int current );

        /**
         * Generate a desktop layout from EWMH _NET_DESKTOP_LAYOUT property parameters.
         */
        void setNETDesktopLayout( Qt::Orientation orientation, int width, int height, int startingCorner );

        /**
         * @returns The ID of the desktop at the point @a coords or 0 if no desktop exists at that
         * point. @a coords is to be in grid units.
         */
        int desktopAtCoords( QPoint coords ) const;
        /**
         * @returns The coords of desktop @a id in grid units.
         */
        QPoint desktopGridCoords( int id ) const;
        /**
         * @returns The coords of the top-left corner of desktop @a id in pixels.
         */
        QPoint desktopCoords( int id ) const;

        /**
         * @returns The ID of the desktop above desktop @a id. Wraps around to the bottom of
         * the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopAbove( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop to the right of desktop @a id. Wraps around to the
         * left of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopToRight( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop below desktop @a id. Wraps around to the top of the
         * layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopBelow( int id = 0, bool wrap = true ) const;
        /**
         * @returns The ID of the desktop to the left of desktop @a id. Wraps around to the
         * right of the layout if @a wrap is set. If @a id is not set use the current one.
         */
        int desktopToLeft( int id = 0, bool wrap = true ) const;

        /**
         * @returns Whether or not the layout is allowed to be modified by the user.
         */
        bool isDesktopLayoutDynamic() const;
        /**
         * Sets whether or not this layout can be modified by the user.
         */
        void setDesktopLayoutDynamicity( bool dynamicity );
        /**
         * Create new desktop at the point @a coords
         * @returns The ID of the created desktop
         */
        int addDesktop( QPoint coords );
        /**
         * Deletes the desktop with the ID @a id. All desktops with an ID greater than the one that
         * was deleted will have their IDs' decremented.
         */
        void deleteDesktop( int id );

    private:
        int desktopCount_;
        QSize desktopGridSize_;
        int* desktopGrid_;
        int currentDesktop_;
        QString activity_;
        QStringList allActivities_;
        QStringList openActivities_;
        bool desktopLayoutDynamicity_;

        KActivityController activityController_;

        bool tilingEnabled_;
        // Each tilingLayout is for one virtual desktop.
        // The length is always one more than the number of
        // virtual desktops so that we can quickly index them
        // without having to remember to subtract one.
        QVector<TilingLayout *> tilingLayouts;

        //-------------------------------------------------
        // Unsorted

    public:
        int activeScreen() const;
        int numScreens() const;
        void checkActiveScreen( const Client* c );
        void setActiveScreenMouse( const QPoint& mousepos );
        QRect screenGeometry( int screen ) const;
        int screenNumber( const QPoint& pos ) const;
        QString currentActivity() const { return activity_; }
        QStringList activityList() const { return allActivities_; }

        // Tab box
        Client* currentTabBoxClient() const;
        ClientList currentTabBoxClientList() const;
        int currentTabBoxDesktop() const;
        QList<int> currentTabBoxDesktopList() const;
        void setTabBoxClient( Client* );
        void setTabBoxDesktop( int );
        Client* nextClientFocusChain( Client* ) const;
        Client* previousClientFocusChain( Client* ) const;
        Client* nextClientStatic( Client* ) const;
        Client* previousClientStatic( Client* ) const;
        int nextDesktopFocusChain( int iDesktop ) const;
        int previousDesktopFocusChain( int iDesktop ) const;
        int nextDesktopStatic( int iDesktop ) const;
        int previousDesktopStatic( int iDesktop ) const;
        void refTabBox();
        void unrefTabBox();
        void closeTabBox( bool abort = false );

        // Tabbing
        void addClientGroup( ClientGroup* group );
        void removeClientGroup( ClientGroup* group );
        /// Returns the index of c in clientGroupList.
        int indexOfClientGroup( ClientGroup* group );
        /// Change the client c_id to the group with index g_id
        void moveItemToClientGroup( ClientGroup* oldGroup, int oldIndex, ClientGroup* group, int index = -1 );
        Client* findSimilarClient( Client* c );
        QList<ClientGroup*> clientGroups; // List of existing clients groups with no special order

        /**
         * Returns the list of clients sorted in stacking order, with topmost client
         * at the last position
         */
        const ClientList& stackingOrder() const;
        ToplevelList xStackingOrder() const;
        ClientList ensureStackingOrder( const ClientList& clients ) const;

        Client* topClientOnDesktop( int desktop, int screen, bool unconstrained = false,
            bool only_normal = true ) const;
        Client* findDesktop( bool topmost, int desktop ) const;
        void sendClientToDesktop( Client* c, int desktop, bool dont_activate );
        void toggleClientOnActivity( Client* c, const QString &activity, bool dont_activate );
        void windowToPreviousDesktop( Client* c );
        void windowToNextDesktop( Client* c );
        void sendClientToScreen( Client* c, int screen );

        // KDE4 remove me - And it's also in the DCOP interface :(
        void showWindowMenuAt( unsigned long id, int x, int y );

        void toggleCompositing();
        void loadEffect( const QString& name );
        void toggleEffect( const QString& name );
        void reconfigureEffect( const QString& name );
        void unloadEffect( const QString& name );

        QStringList loadedEffects() const;
        QStringList listOfEffects() const;


        /**
         * Shows the menu operations menu for the client and makes it active if
         * it's not already.
         */
        void showWindowMenu( const QRect& pos, Client* cl );
        /**
         * Backwards compatibility.
         */
        void showWindowMenu( int x, int y, Client* cl );
        void showWindowMenu( QPoint pos, Client* cl );

        void updateMinimizedOfTransients( Client* );
        void updateOnAllDesktopsOfTransients( Client* );
        void updateOnAllActivitiesOfTransients( Client* );
        void checkTransients( Window w );

        void performWindowOperation( Client* c, WindowOperation op );

        void storeSession( KConfig* config, SMSavePhase phase );
        void storeClient( KConfigGroup &cg, int num, Client *c );
        void storeSubSession( const QString &name, QSet<QByteArray> sessionIds );

        SessionInfo* takeSessionInfo( Client* );
        WindowRules findWindowRules( const Client*, bool );
        void rulesUpdated();
        void discardUsedWindowRules( Client* c, bool withdraw );
        void disableRulesUpdates( bool disable );
        bool rulesUpdatesDisabled() const;

        bool hasDecorationShadows() const;
        bool decorationHasAlpha() const;
        bool decorationSupportsClientGrouping() const; // Returns true if the decoration supports tabs.
        bool decorationSupportsFrameOverlap() const;

        // D-Bus interface
        void cascadeDesktop();
        void unclutterDesktop();
        void doNotManage( const QString& );
        QList<int> decorationSupportedColors() const;
        void nextDesktop();
        void previousDesktop();
        void circulateDesktopApplications();
        bool compositingActive();
        bool waitForCompositingSetup();
        void toggleTiling();
        void nextTileLayout();
        void previousTileLayout();
        void storeActivity( const QString &id );
        void loadActivity( const QString &id );

        void setCurrentScreen( int new_screen );

        QString desktopName( int desk ) const;
        void setShowingDesktop( bool showing );
        void resetShowingDesktop( bool keep_hidden );
        bool showingDesktop() const;

        bool isNotManaged( const QString& title );  // TODO: Setter or getter?

        void sendPingToWindow( Window w, Time timestamp ); // Called from Client::pingWindow()
        void sendTakeActivity( Client* c, Time timestamp, long flags ); // Called from Client::takeActivity()

        void removeClient( Client*, allowed_t ); // Only called from Client::destroyClient() or Client::releaseWindow()
        void setActiveClient( Client*, allowed_t );
        Group* findGroup( Window leader ) const;
        void addGroup( Group* group, allowed_t );
        void removeGroup( Group* group, allowed_t );
        Group* findClientLeaderGroup( const Client* c ) const;

        void removeUnmanaged( Unmanaged*, allowed_t ); // Only called from Unmanaged::release()
        void removeDeleted( Deleted*, allowed_t );
        void addDeleted( Deleted*, allowed_t );

        bool checkStartupNotification( Window w, KStartupInfoId& id, KStartupInfoData& data );

        void focusToNull(); // SELI TODO: Public?
        enum FocusChainChange
            {
            FocusChainMakeFirst,
            FocusChainMakeLast,
            FocusChainUpdate
            };
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
        void addRepaint( const QRegion& r );
        void addRepaint( int x, int y, int w, int h );
        /// Creates XComposite overlay window, call initOverlay() afterwards
        bool createOverlay();
        /// Init overlay and the destination window in it
        void setupOverlay( Window window );
        void showOverlay();
        void hideOverlay(); // hides and resets overlay window
        void setOverlayShape( const QRegion& reg );
        /// Destroys XComposite overlay window
        void destroyOverlay();
        Window overlayWindow();
        void checkUnredirect( bool force = false );
        void checkCompositeTimer();

        // Mouse polling
        void startMousePolling();
        void stopMousePolling();

        void raiseElectricBorderWindows();
        void showElectricBorderWindowOutline();
        void hideElectricBorderWindowOutline();

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

        void slotWalkThroughDesktops();
        void slotWalkBackThroughDesktops();
        void slotWalkThroughDesktopList();
        void slotWalkBackThroughDesktopList();
        void slotWalkThroughWindows();
        void slotWalkBackThroughWindows();
        void slotWalkThroughWindowsAlternative();
        void slotWalkBackThroughWindowsAlternative();

        void slotWalkThroughDesktopsKeyChanged( const QKeySequence& seq );
        void slotWalkBackThroughDesktopsKeyChanged( const QKeySequence& seq );
        void slotWalkThroughDesktopListKeyChanged( const QKeySequence& seq );
        void slotWalkBackThroughDesktopListKeyChanged( const QKeySequence& seq );
        void slotWalkThroughWindowsKeyChanged( const QKeySequence& seq );
        void slotWalkBackThroughWindowsKeyChanged( const QKeySequence& seq );
        void slotMoveToTabLeftKeyChanged( const QKeySequence& seq );
        void slotMoveToTabRightKeyChanged( const QKeySequence& seq );
        void slotWalkThroughWindowsAlternativeKeyChanged( const QKeySequence& seq );
        void slotWalkBackThroughWindowsAlternativeKeyChanged( const QKeySequence& seq );

        void slotSwitchWindowUp();
        void slotSwitchWindowDown();
        void slotSwitchWindowRight();
        void slotSwitchWindowLeft();

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

        void reconfigure();
        void slotReconfigure();
        void slotReinitCompositing();
        void resetCompositing();

        void slotKillWindow();

        void slotGrabWindow();
        void slotGrabDesktop();

        void slotSetupWindowShortcut();
        void setupWindowShortcutDone( bool );
        void slotToggleCompositing();

        void updateClientArea();
        void suspendCompositing();
        void suspendCompositing( bool suspend );

        // user actions, usually bound to shortcuts
        // and also provided through the D-BUS interface.
        void slotToggleTiling();
        void slotToggleFloating();
        void slotNextTileLayout();
        void slotPreviousTileLayout();

        // Changes the focused client
        void slotFocusTileLeft();
        void slotFocusTileRight();
        void slotFocusTileTop();
        void slotFocusTileBottom();
        // swaps active and adjacent client.
        void slotMoveTileLeft();
        void slotMoveTileRight();
        void slotMoveTileTop();
        void slotMoveTileBottom();
        void belowCursor();
        
        // NOTE: debug method
        void dumpTiles() const;

        void slotSwitchToTabLeft(); // Slot to move left the active Client.
        void slotSwitchToTabRight(); // Slot to move right the active Client.
        void slotRemoveFromGroup(); // Slot to remove the active client from its group.

    private slots:
        void groupTabPopupAboutToShow(); // Popup to add to another group
        void switchToTabPopupAboutToShow(); // Popup to move in the group
        void slotAddToTabGroup( QAction* ); // Add client to a group
        void slotSwitchToTab( QAction* ); // Change the tab
        void desktopPopupAboutToShow();
        void activityPopupAboutToShow();
        void clientPopupAboutToShow();
        void slotSendToDesktop( QAction* );
        void slotToggleOnActivity( QAction* );
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
        void finishCompositing();
        void fallbackToXRenderCompositing();
        void performCompositing();
        void performMousePoll();
        void lostCMSelection();
        void updateElectricBorders();
        void resetCursorPosTime();
        void delayedCheckUnredirect();

        void updateCurrentActivity(const QString &new_activity);
        void activityRemoved(const QString &activity);
        void activityAdded(const QString &activity);

    protected:
        bool keyPressMouseEmulation( XKeyEvent& ev );

    Q_SIGNALS:
        Q_SCRIPTABLE void compositingToggled( bool active );
    
    //Signals required for the scripting interface
    signals:
        void desktopPresenceChanged(KWin::Client*, int);
	void currentDesktopChanged(int);
	void clientAdded(KWin::Client*);
	void clientRemoved(KWin::Client*);
	void clientActivated(KWin::Client*);
	void groupAdded(KWin::Group*);

    private:
        void init();
        void initShortcuts();
        void readShortcuts();
        void initDesktopPopup();
        void initActivityPopup();
        void discardPopup();
        void setupWindowShortcut( Client* c );
        void checkCursorPos();

        enum Direction
            {
            DirectionNorth,
            DirectionEast,
            DirectionSouth,
            DirectionWest
            };
        void switchWindow( Direction direction );
        bool startKDEWalkThroughWindows( TabBoxMode mode ); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
        bool startWalkThroughDesktops( TabBoxMode mode ); // TabBoxDesktopMode | TabBoxDesktopListMode
        bool startWalkThroughDesktops();
        bool startWalkThroughDesktopList();
        void navigatingThroughWindows( bool forward, const KShortcut& shortcut, TabBoxMode mode ); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
        void KDEWalkThroughWindows( bool forward );
        void CDEWalkThroughWindows( bool forward );
        void walkThroughDesktops( bool forward );
        void KDEOneStepThroughWindows( bool forward, TabBoxMode mode ); // TabBoxWindowsMode | TabBoxWindowsAlternativeMode
        void oneStepThroughDesktops( bool forward, TabBoxMode mode ); // TabBoxDesktopMode | TabBoxDesktopListMode
        void oneStepThroughDesktops( bool forward );
        void oneStepThroughDesktopList( bool forward );
        bool establishTabBoxGrab();
        void removeTabBoxGrab();

        void propagateClients( bool propagate_new_clients ); // Called only from updateStackingOrder
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
        void fixPositionAfterCrash( Window w, const XWindowAttributes& attr );

        /// This is the right way to create a new client
        Client* createClient( Window w, bool is_mapped );
        void addClient( Client* c, allowed_t );
        Unmanaged* createUnmanaged( Window w );
        void addUnmanaged( Unmanaged* c, allowed_t );

        Window findSpecialEventWindow( XEvent* e );

        void randomPlacement( Client* c );
        void smartPlacement( Client* c );
        void cascadePlacement( Client* c, bool re_init = false );

        // Desktop names and number of desktops
        void loadDesktopSettings();
        void saveDesktopSettings();

        // Mouse emulation
        WId getMouseEmulationWindow();
        enum MouseEmulation { EmuPress, EmuRelease, EmuMove };
        unsigned int sendFakedMouseEvent( const QPoint& pos, WId win, MouseEmulation type, int button, unsigned int state ); // returns the new state

        void tabBoxKeyPress( int key );
        void tabBoxKeyRelease( const XKeyEvent& ev );

        // Electric borders
        void destroyElectricBorders();
        bool electricBorderEvent(XEvent * e);
        void electricBorderSwitchDesktop( ElectricBorder border, const QPoint& pos );

        //---------------------------------------------------------------------

        void helperDialog( const QString& message, const Client* c );

        QMenu* clientPopup();
        void closeActivePopup();

        void updateClientArea( bool force );

        bool windowRepaintsPending() const;
        void setCompositeTimer();
        void checkCompositePaintTime( int msec );

        QVector<int> desktop_focus_chain;

        QWidget* active_popup;
        Client* active_popup_client;

        void loadSessionInfo();
        void addSessionInfo( KConfigGroup &cg );
        void loadSubSessionInfo( const QString &name );

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

        // try to get a decent tile, either the one with
        // focus or the one below the mouse.
        Tile* getNiceTile() const;
        void removeTile( Client *c );
        // int, and not Tile::Direction because 
        // we are using a forward declaration for Tile
        Tile* findAdjacentTile( Tile *ref, int d );
        void focusTile( int d );
        void moveTile( int d );

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

        bool control_grab;
        bool tab_grab;
        //KKeyNative walkThroughDesktopsKeycode, walkBackThroughDesktopsKeycode;
        //KKeyNative walkThroughDesktopListKeycode, walkBackThroughDesktopListKeycode;
        //KKeyNative walkThroughWindowsKeycode, walkBackThroughWindowsKeycode;
        KShortcut cutWalkThroughDesktops, cutWalkThroughDesktopsReverse;
        KShortcut cutWalkThroughDesktopList, cutWalkThroughDesktopListReverse;
        KShortcut cutWalkThroughWindows, cutWalkThroughWindowsReverse;
        KShortcut cutWalkThroughGroupWindows, cutWalkThroughGroupWindowsReverse;
        KShortcut cutWalkThroughWindowsAlternative, cutWalkThroughWindowsAlternativeReverse;
        bool mouse_emulation;
        unsigned int mouse_emulation_state;
        WId mouse_emulation_window;
        int block_focus;

        TabBox::TabBox* tab_box;
        DesktopChangeOSD* desktop_change_osd;

        QMenu* popup;
        QMenu* advanced_popup;
        QMenu* trans_popup;
        QActionGroup* trans_popup_group;
        QMenu* desk_popup;
        QMenu* activity_popup;
        QMenu* add_tabs_popup; // Menu to add the group to other group
        QMenu* switch_to_tab_popup; // Menu to change tab

        void modalActionsSwitch( bool enabled );

        KActionCollection* keys;
        KActionCollection* client_keys;
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
        KActionCollection* disable_shortcuts_keys;
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

        ElectricBorder electric_current_border;
        Window electric_windows[ELECTRIC_COUNT];
        int electricLeft;
        int electricRight;
        int electricTop;
        int electricBottom;
        Time electric_time_first;
        Time electric_time_last;
        Time electric_time_last_trigger;
        QPoint electric_push_point;
        int electric_reserved[ELECTRIC_COUNT]; // Corners/edges used by something

        Placement* initPositioning;

        QVector<QRect> workarea; // Array of workareas for virtual desktops
        // Array of restricted areas that window cannot be moved into
        QVector<StrutRects> restrictedmovearea;
        // Array of the previous restricted areas that window cannot be moved into
        QVector<StrutRects> oldrestrictedmovearea;
        QVector< QVector<QRect> > screenarea; // Array of workareas per xinerama screen for all virtual desktops

        bool managing_topmenus;
        KSelectionOwner* topmenu_selection;
        KSelectionWatcher* topmenu_watcher;
        ClientList topmenus; // Doesn't own them
        mutable int topmenu_height;
        QWidget* topmenu_space;

        int set_active_client_recursion;
        int block_stacking_updates; // When > 0, stacking updates are temporarily disabled
        bool blocked_propagating_new_clients; // Propagate also new clients after enabling stacking updates?
        Window null_focus_window;
        bool forced_global_mouse_grab;
        friend class StackingUpdatesBlocker;

        KSelectionOwner* cm_selection;
        bool compositingSuspended;
        QTimer compositeTimer;
        QTime lastCompositePaint;
        QTime nextPaintReference;
        QTimer mousePollingTimer;
        int compositeRate;
        int xrrRefreshRate; // used only for compositing
        QRegion repaints_region;
        Window overlay; // XComposite overlay window
        bool overlay_visible;
        bool overlay_shown; // For showOverlay()
        QRegion overlay_shape;
        QSlider* transSlider;
        QPushButton* transButton;
        QTimer unredirectTimer;
        bool forceUnredirectCheck;
        QList< int > composite_paint_times;
        QTimer compositeResetTimer; // for compressing composite resets

        Window outline_left;
        Window outline_right;
        Window outline_top;
        Window outline_bottom;

    private:
        friend bool performTransiencyCheck();
    };

/**
 * Helper for Workspace::blockStackingUpdates() being called in pairs (True/false)
 */
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

/**
 * NET WM Protocol handler class
 */
class RootInfo : public NETRootInfo
    {
    private:
        typedef KWin::Client Client;  // Because of NET::Client

    public:
        RootInfo( Workspace* ws, Display* dpy, Window w, const char* name, unsigned long pr[],
            int pr_num, int scr= -1 );

    protected:
        virtual void changeNumberOfDesktops( int n );
        virtual void changeCurrentDesktop( int d );
        virtual void changeActiveWindow( Window w,NET::RequestSource src, Time timestamp, Window active_window );
        virtual void closeWindow( Window w );
        virtual void moveResize( Window w, int x_root, int y_root, unsigned long direction );
        virtual void moveResizeWindow( Window w, int flags, int x, int y, int width, int height );
        virtual void gotPing( Window w, Time timestamp );
        virtual void restackWindow( Window w, RequestSource source, Window above, int detail, Time timestamp );
        virtual void gotTakeActivity( Window w, Time timestamp, long flags );
        virtual void changeShowingDesktop( bool showing );

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

inline int Workspace::desktopAtCoords( QPoint coords ) const
    {
    return desktopGrid_[coords.y() * desktopGridSize_.width() + coords.x()];
    }

inline bool Workspace::isDesktopLayoutDynamic() const
    {
    return desktopLayoutDynamicity_;
    }

inline void Workspace::setDesktopLayoutDynamicity( bool dynamicity )
    {
    desktopLayoutDynamicity_ = dynamicity;
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

inline void Workspace::addGroup( Group* group, allowed_t )
    {
    emit groupAdded(group);
    groups.append( group );
    }

inline void Workspace::removeGroup( Group* group, allowed_t )
    {
    groups.removeAll( group );
    }

inline const ClientList& Workspace::stackingOrder() const
    {
    // TODO: Q_ASSERT( block_stacking_updates == 0 );
    return stacking_order;
    }

inline void Workspace::showWindowMenu( QPoint pos, Client* cl )
    {
    showWindowMenu( QRect( pos, pos ), cl );
    }

inline void Workspace::showWindowMenu( int x, int y, Client* cl )
    {
    showWindowMenu( QRect( QPoint( x, y ), QPoint( x, y )), cl );
    }

inline void Workspace::setWasUserInteraction()
    {
    was_user_interaction = true;
    }

inline bool Workspace::wasUserInteraction() const
    {
    return was_user_interaction;
    }

inline bool Workspace::managingTopMenus() const
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

inline bool Workspace::rulesUpdatesDisabled() const
    {
    return rules_updates_disabled;
    }

inline void Workspace::forceRestacking()
    {
    force_restacking = true;
    StackingUpdatesBlocker blocker( this ); // Do restacking if not blocked
    }

inline void Workspace::updateFocusMousePosition( const QPoint& pos )
    {
    focusMousePos = pos;
    }

inline QPoint Workspace::focusMousePosition() const
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
    for( ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it )
        if( predicate( const_cast<const Client*>( *it )))
            procedure( *it );
    for( ClientList::ConstIterator it = desktops.constBegin(); it != desktops.constEnd(); ++it )
        if( predicate( const_cast<const Client*>( *it )))
            procedure( *it );
    }

template< typename T >
inline void Workspace::forEachClient( T procedure )
    {
    return forEachClient( procedure, TruePredicate() );
    }

template< typename T >
inline Unmanaged* Workspace::findUnmanaged( T predicate ) const
    {
    return findUnmanagedInList( unmanaged, predicate );
    }

template< typename T1, typename T2 >
inline void Workspace::forEachUnmanaged( T1 procedure, T2 predicate )
    {
    for( UnmanagedList::ConstIterator it = unmanaged.constBegin(); it != unmanaged.constEnd(); ++it )
        if( predicate( const_cast<const Unmanaged*>( *it )))
            procedure( *it );
    }

template< typename T >
inline void Workspace::forEachUnmanaged( T procedure )
    {
    return forEachUnmanaged( procedure, TruePredicate() );
    }

KWIN_COMPARE_PREDICATE( ClientMatchPredicate, Client, const Client*, cl == value );

inline bool Workspace::hasClient( const Client* c )
    {
    return findClient( ClientMatchPredicate( c ));
    }

inline void Workspace::checkCompositeTimer()
    {
    if( !compositeTimer.isActive() )
        setCompositeTimer();
    }

inline bool Workspace::hasDecorationShadows() const
    {
    return mgr->factory()->supports( AbilityProvidesShadow );
    }

inline bool Workspace::decorationHasAlpha() const
    {
    return mgr->factory()->supports( AbilityUsesAlphaChannel );
    }

inline bool Workspace::decorationSupportsClientGrouping() const
    {
    return mgr->factory()->supports( AbilityClientGrouping );
    }

inline bool Workspace::decorationSupportsFrameOverlap() const
    {
    return mgr->factory()->supports( AbilityExtendIntoClientArea );
    }

inline void Workspace::addClientGroup( ClientGroup* group )
    {
    clientGroups.append( group );
    }

inline void Workspace::removeClientGroup( ClientGroup* group )
    {
    clientGroups.removeAll( group );
    }

/*
 * Called from D-BUS
 */
inline void Workspace::toggleTiling()
    {
    slotToggleTiling();
    }

/*
 * Called from D-BUS
 */
inline void Workspace::nextTileLayout()
    {
    slotNextTileLayout();
    }

/*
 * Called from D-BUS
 */
inline void Workspace::previousTileLayout()
    {
    slotPreviousTileLayout();
    }

} // namespace

#endif
