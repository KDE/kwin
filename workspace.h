/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

#include <qwidget.h>
#include <qapplication.h>
#include <qpopupmenu.h>
#include <qguardedptr.h>
#include <qvaluelist.h>
#include <qptrlist.h>
#include <qtimer.h>
#include "options.h"
#include "KWinInterface.h"
#include <kshortcut.h>
#include <qcursor.h>
#include <netwm_def.h>

#include <X11/Xlib.h>

class KConfig;
class KGlobalAccel;

namespace KWinInternal {

class Client;
class TabBox;
class PopupInfo;
class RootInfo;
class PluginMgr;

typedef QValueList<Client*> ClientList;

class SystemTrayWindow
{
public:
    SystemTrayWindow()
	: win(0),winFor(0)
    {}
    SystemTrayWindow( WId w )
	: win(w),winFor(0)
    {}
    SystemTrayWindow( WId w, WId wf  )
	: win(w),winFor(wf)
    {}

    bool operator==( const SystemTrayWindow& other )
    { return win == other.win; }
    WId win;
    WId winFor;
};

typedef QValueList<SystemTrayWindow> SystemTrayWindowList;

struct SessionInfo
{
    QCString sessionId;
    QCString windowRole;
    QCString wmCommand;
    QCString wmClientMachine;
    QCString resourceName;
    QCString resourceClass;

    QRect geometry;
    QRect restore;
    int maximize;
    int desktop;
    bool iconified;
    bool sticky;
    bool shaded;
    bool staysOnTop;
    bool skipTaskbar;
    bool skipPager;
    NET::WindowType windowType;
};


class Shape {
public:
    static bool hasShape( WId w);
    static int shapeEvent();
};

class Motif {
public:
    static bool noBorder( WId w );
    static bool funcFlags( WId w, bool& resize, bool& move, bool& minimize,
        bool& maximize, bool& close );
    struct MwmHints {
        ulong flags;
        ulong functions;
        ulong decorations;
        long input_mode;
        ulong status;
    };
    enum {  MWM_HINTS_FUNCTIONS = (1L << 0),
            MWM_HINTS_DECORATIONS =  (1L << 1),

            MWM_FUNC_ALL = (1L << 0),
            MWM_FUNC_RESIZE = (1L << 1),
            MWM_FUNC_MOVE = (1L << 2),
            MWM_FUNC_MINIMIZE = (1L << 3),
            MWM_FUNC_MAXIMIZE = (1L << 4),
            MWM_FUNC_CLOSE = (1L << 5)
        };
};

class WorkspacePrivate;
// NOTE: this class has to keep binary compatibility, just like other
// KWin classes accessible from the plugins

class Workspace : public QObject, virtual public KWinInterface
{
    Q_OBJECT
public:
    Workspace( bool restore = FALSE );
    virtual ~Workspace();

    static Workspace * self() { return _self; }

    virtual bool workspaceEvent( XEvent * );

    bool hasClient(Client *); // ### make const in KDE 4

    Client* findClient( WId w ) const;

    QRect geometry() const;

    enum clientAreaOption { PlacementArea, MovementArea, MaximizeArea };


    // default is MaximizeArea
    QRect clientArea(clientAreaOption, const QPoint& p, int desktop);
    QRect clientArea(const QPoint& p, int desktop);
    // KDE4 remove the following 3 methods
    inline QRect clientArea(clientAreaOption opt) { return clientArea(opt, QCursor::pos()); }
    QRect clientArea(clientAreaOption, const QPoint& p);
    QRect clientArea(const QPoint& p);

    void removeClient( Client* );

    bool destroyClient( Client* );

    /**
     * @internal
     * @obsolete
     */
    void killWindowAtPosition(int x, int y);
    /**
     * @internal
     */
    void killWindowId( Window window);

    void killWindow() { slotKillWindow(); }

    WId rootWin() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    Client* activeClient() const;

    void setActiveClient( Client* );
    void activateClient( Client*, bool force = FALSE  );
    void requestFocus( Client* c, bool force = FALSE );

    void updateColormap();

    void setFocusChangeEnabled(bool b) { focus_change = b; }  // KDE 3.0: No longer used
    bool focusChangeEnabled() { return focus_change; }  // KDE 3.0: No longer used

    /**
     * Indicates that the client c is being moved around by the user.
     */
    void setClientIsMoving( Client *c );

    void place(Client *c);
    void doPlacement(Client* c ); // obsolete KDE4 remove

    QPoint adjustClientPosition( Client* c, QPoint pos );
    void raiseClient( Client* c );
    void lowerClient( Client* c );
    void stackClientUnderActive( Client* );
    void raiseOrLowerClient( Client * );
    void reconfigure();

    void clientHidden( Client*  );

    void clientReady( Client* );
    void clientMoved(const QPoint &pos, unsigned long time);

    /**
     * Returns the current virtual desktop of this workspace
     */
    int currentDesktop() const;
    int nextDesktop( int iDesktop ) const;
    int previousDesktop( int iDesktop ) const;

    /**
     * Returns the number of virtual desktops of this workspace
     */
    int numberOfDesktops() const;
    void setNumberOfDesktops( int n );

    QWidget* desktopWidget();   // ### make const in KDE 4

    Client* nextClient(Client*) const;
    Client* previousClient(Client*) const;
    Client* nextStaticClient(Client*) const;
    Client* previousStaticClient(Client*) const;

     /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const ClientList& stackingOrder() const;

    Client* topClientOnDesktop() const;
    void sendClientToDesktop( Client* c, int desktop );



    /**
     * @deprecated Use showWindowMenu()
     */
    QPopupMenu* clientPopup( Client* );
    /**
     * @deprecated Use showWindowMenu()
     */
    // KDE4 remove me - and it's also in the DCOP interface :(
    void showWindowMenuAt( unsigned long id, int x, int y );

    /**
     * Shows the menu operations menu for the client
     * and makes it active if it's not already.
     */
    void showWindowMenu( int x, int y, Client* cl );
    void showWindowMenu( QPoint pos, Client* cl );

    void iconifyOrDeiconifyTransientsOf( Client* );
    void setStickyTransientsOf( Client*, bool sticky );

    bool hasCaption( const QString& caption );   // ### make const in KDE 4

    void performWindowOperation( Client* c, Options::WindowOperation op );

    void restoreLegacySession( KConfig* config );
    void storeLegacySession( KConfig* config );
    void storeSession( KConfig* config );

    SessionInfo* takeSessionInfo( Client* );

    virtual void updateClientArea();


    // dcop interface
    void cascadeDesktop();
    void unclutterDesktop();
    void doNotManage(QString);
    void setCurrentDesktop( int new_desktop );
    void nextDesktop();
    void previousDesktop();
    void circulateDesktopApplications();

    QString desktopName( int desk );   // ### make const in KDE 4
    void setDesktopLayout(int o, int x, int y);

    bool isNotManaged( const QString& title );  // ### setter or getter ?

public slots:
    void refresh();
    // keybindings
    void slotSwitchDesktopNext();
    void slotSwitchDesktopPrevious();
    void slotSwitchDesktopRight();
    void slotSwitchDesktopLeft();
    void slotSwitchDesktopUp();
    void slotSwitchDesktopDown();

    void slotSwitchToDesktop( int );
    //void slotSwitchToWindow( int );
    void slotWindowToDesktop( int );
    //void slotWindowToListPosition( int );

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowIconify();
    void slotWindowIconifyAll();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();
    void slotWindowRaiseOrLower();

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
    void slotWindowStaysOnTop();
    void slotWindowSticky();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();

    void slotMouseEmulation();

    void slotResetAllClientsDelayed();
    void slotResetAllClients();
    void slotSettingsChanged( int category );

    void slotReconfigure();

    void slotKillWindow();

    void slotGrabWindow();
    void slotGrabDesktop();

private slots:
    void desktopPopupAboutToShow();
    void clientPopupAboutToShow();
    void sendToDesktop( int );
    void clientPopupActivated( int );
    void configureWM();
    void focusEnsurance();

protected:
    bool keyPress( XKeyEvent& ev );
    bool keyRelease( XKeyEvent& ev );
    bool keyPressMouseEmulation( XKeyEvent& ev );
    bool netCheck( XEvent* e );
    void checkStartOnDesktop( WId w );

private:
    void init();
    void initShortcuts();
    void readShortcuts();
    void initDesktopPopup();

    bool startKDEWalkThroughWindows();
    bool startWalkThroughDesktops( int mode ); // TabBox::Mode::DesktopMode | DesktopListMode
    bool startWalkThroughDesktops();
    bool startWalkThroughDesktopList();
    void KDEWalkThroughWindows( bool forward );
    void CDEWalkThroughWindows( bool forward );
    void walkThroughDesktops( bool forward );
    void KDEOneStepThroughWindows( bool forward );
    void oneStepThroughDesktops( bool forward, int mode ); // TabBox::Mode::DesktopMode | DesktopListMode
    void oneStepThroughDesktops( bool forward );
    void oneStepThroughDesktopList( bool forward );

    ClientList constrainedStackingOrder( const ClientList& list );

    Client* clientFactory(WId w);

    void raiseTransientsOf( ClientList& safeset, Client* c );
    void lowerTransientsOf( ClientList& safeset, Client* c );
    void randomPlacement(Client* c);
    void smartPlacement(Client* c);
    void cascadePlacement(Client* c, bool re_init = false);

    void focusToNull();

    Client* findClientWithId( WId w ) const;

    void propagateClients( bool onlyStacking = FALSE);

    bool addSystemTrayWin( WId w );
    bool removeSystemTrayWin( WId w );
    void propagateSystemTrayWins();
    SystemTrayWindow findSystemTrayWin( WId w );

    // desktop names and number of desktops
    void loadDesktopSettings();
    void saveDesktopSettings();

    // mouse emulation
    WId getMouseEmulationWindow();
    enum MouseEmulation { EmuPress, EmuRelease, EmuMove };
    unsigned int sendFakedMouseEvent( QPoint pos, WId win, MouseEmulation type, int button, unsigned int state ); // returns the new state

    // electric borders
    void createBorderWindows();
    void destroyBorderWindows();
    void electricBorder(XEvent * e);
    void raiseElectricBorders();

    // ------------------

    void calcDesktopLayout(int &x, int &y);

    QPopupMenu* clientPopup();

    void updateClientArea( bool force );

    SystemTrayWindowList systemTrayWins;

    int current_desktop;
    int number_of_desktops;
    QMemArray<int> desktop_focus_chain;

    Client* popup_client;

    void loadSessionInfo();

    QWidget* desktop_widget;

    QPtrList<SessionInfo> session;
    QPtrList<SessionInfo> fakeSession;
    void loadFakeSessionInfo();
    void storeFakeSessionInfo( Client* c );
    void writeFakeSessionInfo();
    static const char* windowTypeToTxt( NET::WindowType type );
    static NET::WindowType txtToWindowType( const char* txt );
    static bool sessionInfoWindowTypeMatch( Client* c, SessionInfo* info );

    Client* active_client;
    Client* last_active_client;
    Client* should_get_focus;
    Client* most_recently_raised;

    ClientList clients;
    ClientList desktops;
    ClientList stacking_order;
    ClientList focus_chain;

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
    bool focus_change;

    TabBox* tab_box;
    PopupInfo* popupinfo;

    QPopupMenu *popup;
    QPopupMenu *desk_popup;

    KGlobalAccel *keys;
    WId root;

    PluginMgr *mgr;

    RootInfo *rootInfo;
    QWidget* supportWindow;

    QRect area_; // KDE4 remove me, unused

    // swallowing
    QStringList doNotManageList;

    // colormap handling
    Colormap default_colormap;
    Colormap installed_colormap;

    // Timer to collect requests for 'ResetAllClients'
    QTimer resetTimer;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer focusEnsuranceTimer;

    WorkspacePrivate* d;
    static Workspace *_self;

    void addClient( Client* c );
};

inline WId Workspace::rootWin() const
{
    return root;
}

inline Client* Workspace::activeClient() const
{
    return active_client;
}

inline int Workspace::currentDesktop() const
{
    return current_desktop;
}

inline int Workspace::numberOfDesktops() const
{
    return number_of_desktops;
}

inline const ClientList& Workspace::stackingOrder() const
{
    return stacking_order;
}

inline void Workspace::showWindowMenu(QPoint pos, Client* cl)
{
    showWindowMenu(pos.x(), pos.y(), cl);
}


};

#endif
