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
#include "options.h"
#include "KWinInterface.h"

#include <X11/Xlib.h>

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


struct SessionInfo
{
    QCString sessionId;
    QCString windowRole;

    QCString wmCommand; // compatibility

    QCString resourceName; // for faked session info
    QCString resourceClass;

    QRect geometry;
    QRect restore;
    int maximize;
    int desktop;
    bool iconified;
    bool sticky;
    bool shaded;
    bool staysOnTop;
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

class Workspace : public QObject, virtual public KWinInterface
{
    Q_OBJECT
public:
    Workspace( bool restore = FALSE );
    virtual ~Workspace();

    virtual bool workspaceEvent( XEvent * );

    bool hasClient(Client *);

    Client* findClient( WId w ) const;

    QRect geometry() const;

    QRect clientArea();

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

    void clientHidden( Client*  );

    /**
     * Returns the current virtual desktop of this workspace
     */
    int currentDesktop() const;

    /**
     * Returns the number of virtual desktops of this workspace
     */
    int numberOfDesktops() const;
    void setNumberOfDesktops( int n );

    QWidget* desktopWidget();

    void   grabKey(KeySym keysym, unsigned int mod);
    void ungrabKey(KeySym keysym, unsigned int mod);

    void grabControlTab(bool grab);

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

    void storeSession( KConfig* config );

    SessionInfo* takeSessionInfo( Client* );

    virtual void updateClientArea();


    // dcop interface
    void cascadeDesktop();
    void unclutterDesktop();
    void reconfigure();
    void doNotManage(QString);

    QString desktopName( int desk );

    bool isNotManaged( const QString& title );

public slots:
    void setCurrentDesktop( int new_desktop );
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

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowIconify();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();

    void slotMouseEmulation();

    void slotResetAllClientsDelayed();
    void slotResetAllClients();

    void slotLogout();

    void slotKillWindow();

private slots:
    void desktopPopupAboutToShow();
    void clientPopupAboutToShow();
    void sendToDesktop( int );
    void clientPopupActivated( int );
    void focusEnsurance();
    
protected:
    bool keyPress( XKeyEvent key );
    bool keyRelease( XKeyEvent key );
    bool keyPressMouseEmulation( XKeyEvent key );
    bool netCheck( XEvent* e );

private:
    void init();
    void createKeybindings();
    void freeKeyboard(bool pass);

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
    
    QTimer focusEnsuranceTimer;
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
