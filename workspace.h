/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <qwidget.h>
#include <qapplication.h>
#include <qpopupmenu.h>
#include <qguardedptr.h>
#include <qvaluelist.h>
#include <qlist.h>
#include <qtimer.h>
#include <config.h>
#include "options.h"
#include "KWinInterface.h"

#include <X11/Xlib.h>
#ifdef HAVE_XINERAMA
extern "C" {
#include <X11/extensions/Xinerama.h>
};
#endif

class KConfig;
class KGlobalAccel;

namespace KWinInternal {

class Client;
class TabBox;
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

struct SessionInfoPrivate;
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

    private:
    SessionInfoPrivate* d;
};


class Shape {
public:
    static bool hasShape( WId w);
    static int shapeEvent();
};

class Motif {
public:
    static bool noBorder( WId w );
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

    bool hasClient(Client *);

    Client* findClient( WId w ) const;

    QRect geometry() const;

    enum clientAreaOption { PlacementArea, MovementArea, MaximizeArea };


    // default is MaximizeArea
    QRect clientArea();  // ### KDE3: remove me!
    QRect clientArea(clientAreaOption);

    void removeClient( Client* );

    bool destroyClient( Client* );

    void killWindowAtPosition(int x, int y);

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

    void setFocusChangeEnabled(bool b) { focus_change = b; }
    bool focusChangeEnabled() { return focus_change; }

    void doPlacement( Client* c );
    QPoint adjustClientPosition( Client* c, QPoint pos );
    void raiseClient( Client* c );
    void lowerClient( Client* c );
    void raiseOrLowerClient( Client * );
    void reconfigure();

    void clientHidden( Client*  );

    void clientReady( Client* );

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

    QWidget* desktopWidget();

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



    QPopupMenu* clientPopup( Client* );
    void showWindowMenuAt( unsigned long id, int x, int y );

    void setDesktopClient( Client* );

    void iconifyOrDeiconifyTransientsOf( Client* );
    void setStickyTransientsOf( Client*, bool sticky );

    bool hasCaption( const QString& caption );

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

    QString desktopName( int desk );

    bool isNotManaged( const QString& title );

public slots:
    void refresh();
    // keybindings
    void slotSwitchDesktop1();
    void slotSwitchDesktop2();
    void slotSwitchDesktop3();
    void slotSwitchDesktop4();
    void slotSwitchDesktop5();
    void slotSwitchDesktop6();
    void slotSwitchDesktop7();
    void slotSwitchDesktop8();
    void slotSwitchDesktop9();
    void slotSwitchDesktop10();
    void slotSwitchDesktop11();
    void slotSwitchDesktop12();
    void slotSwitchDesktop13();
    void slotSwitchDesktop14();
    void slotSwitchDesktop15();
    void slotSwitchDesktop16();
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
    void slotWindowCloseAll();
    void slotWindowMove();
    void slotWindowResize();

    void slotWindowNextDesktop();
    void slotWindowPreviousDesktop();

    void slotMouseEmulation();

    void slotResetAllClientsDelayed();
    void slotResetAllClients();

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
    bool keyPress( XKeyEvent key );
    bool keyRelease( XKeyEvent key );
    bool keyPressMouseEmulation( XKeyEvent key );
    bool netCheck( XEvent* e );
    void checkStartOnDesktop( WId w );

private:
    void init();
    void createKeybindings();

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

    Client* findClientWidthId( WId w ) const;

    void propagateClients( bool onlyStacking = FALSE);

    bool addSystemTrayWin( WId w );
    bool removeSystemTrayWin( WId w );
    void propagateSystemTrayWins();
    SystemTrayWindow findSystemTrayWin( WId w );

    //CT needed for cascading+
    struct CascadingInfo {
      QPoint pos;
      int col;
      int row;
    };

    // desktop names and number of desktops
    void loadDesktopSettings();
    void saveDesktopSettings();

    // mouse emulation
    WId getMouseEmulationWindow();
    enum MouseEmulation { EmuPress, EmuRelease, EmuMove };
    unsigned int sendFakedMouseEvent( QPoint pos, WId win, MouseEmulation type, int button, unsigned int state ); // returns the new state

    // ------------------

    SystemTrayWindowList systemTrayWins;

    int current_desktop;
    int number_of_desktops;
    QArray<int> desktop_focus_chain;

    QGuardedPtr<Client> popup_client;

    void loadSessionInfo();

    QWidget* desktop_widget;

    QList<SessionInfo> session;
    QList<SessionInfo> fakeSession;
    void loadFakeSessionInfo();
    void storeFakeSessionInfo( Client* c );
    void writeFakeSessionInfo();
    QValueList<CascadingInfo> cci;

    Client* desktop_client;
    Client* active_client;
    Client* last_active_client;
    Client* should_get_focus;
    Client* most_recently_raised;

    ClientList clients;
    ClientList stacking_order;
    ClientList focus_chain;

    bool control_grab;
    bool tab_grab;
    unsigned int walkThroughDesktopsKeycode,walkBackThroughDesktopsKeycode;
    unsigned int walkThroughDesktopListKeycode,walkBackThroughDesktopListKeycode;
    unsigned int walkThroughWindowsKeycode,walkBackThroughWindowsKeycode;
    bool mouse_emulation;
    unsigned int mouse_emulation_state;
    WId mouse_emulation_window;
    bool focus_change;

    TabBox* tab_box;

    QPopupMenu *popup;
    QPopupMenu *desk_popup;

    KGlobalAccel *keys;
    WId root;

    PluginMgr *mgr;

    RootInfo *rootInfo;
    QWidget* supportWindow;

    QRect area;

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
#ifdef HAVE_XINERAMA
    int numHeads;
    XineramaScreenInfo *xineramaInfo;
    XineramaScreenInfo dummy_xineramaInfo;
#endif
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

};

#endif
