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
#include "options.h"
#include "plugins.h"
#include "KWinInterface.h"

#include <X11/Xlib.h>


class Client;
class TabBox;

class KConfig;
class KGlobalAccel;
class RootInfo;

typedef QValueList<Client*> ClientList;

class DockWindow
{
public:
    DockWindow()
	: dockWin(0),dockFor(0)
    {}
    DockWindow( WId w )
	: dockWin(w),dockFor(0)
    {}
    DockWindow( WId w, WId wf  )
	: dockWin(w),dockFor(wf)
    {}

    bool operator==( const DockWindow& other )
    { return dockWin == other.dockWin; }
    WId dockWin;
    WId dockFor;
};

typedef QValueList<DockWindow> DockWindowList;


struct SessionInfo
{
    QCString sessionId;
    QCString windowRole;
    int x;
    int y;
    int width;
    int height;
    int desktop;
    bool iconified;
    bool sticky;
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

    /**
     * @return the area available for clients. This is the desktop
     * geometry adjusted for edge-anchored windows.
     * Placement algorithms should refer to this rather than geometry().
     * @sa geometry()
     */
    QRect clientArea();

    bool destroyClient( Client* );

    void killWindowAtPosition(int x, int y);

    WId rootWin() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    Client* activeClient() const;

    void setActiveClient( Client* );
    void activateClient( Client* );
    void requestFocus( Client* c);

    void setEnableFocusChange(bool b) { focus_change = b; }
    bool focusChangeEnabled() { return focus_change; }

    void doPlacement( Client* c );
    QPoint adjustClientPosition( Client* c, QPoint pos );
    void raiseClient( Client* c );
    void lowerClient( Client* c, bool dropFocus=true );

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

    void grabKey(KeySym keysym, unsigned int mod);

    Client* nextClient(Client*) const;
    Client* previousClient(Client*) const;
    Client* nextStaticClient(Client*) const;
    Client* previousStaticClient(Client*) const;

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const ClientList& stackingOrder() const;

    //#### TODO right layers as default
    Client* topClientOnDesktop( int fromLayer = 0, int toLayer = 0) const;


    QPopupMenu* clientPopup( Client* );

    void setDesktopClient( Client* );

    bool iconifyMeansWithdraw( Client* );
    void iconifyOrDeiconifyTransientsOf( Client* );
    void setStickyTransientsOf( Client*, bool sticky );

    bool hasCaption( const QString& caption );

    void performWindowOperation( Client* c, Options::WindowOperation op );

    Client* clientFactory(Workspace *ws, WId w);

    void storeSession( KConfig* config );

    SessionInfo* takeSessionInfo( Client* );

    /**
     * When the area that is available for clients (that which is not
     * taken by windows like panels, the top-of-screen menu etc) may
     * have changed, this will recalculate the available space.
     */
    virtual void updateClientArea();


    // dcop interface
    void cascadeDesktop();
    void unclutterDesktop();


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

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowIconify();
    void slotWindowShade();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();

    void slotMouseEmulation();

    void slotResetAllClients();

    void slotLogout();

    void slotKillWindow();

private slots:
    void desktopPopupAboutToShow();
    void clientPopupAboutToShow();
    void sendToDesktop( int );
    void clientPopupActivated( int );

protected:
    bool keyPress( XKeyEvent key );
    bool keyRelease( XKeyEvent key );
    bool keyPressMouseEmulation( XKeyEvent key );
    bool netCheck( XEvent* e );

private:
    void init();
    void createKeybindings();
    void freeKeyboard(bool pass);

    void raiseTransientsOf( ClientList& safeset, Client* c );
    void lowerTransientsOf( ClientList& safeset, Client* c );
    void randomPlacement(Client* c);
    void smartPlacement(Client* c);
    void cascadePlacement(Client* c, bool re_init = false);

    void focusToNull();

    Client* findClientWidthId( WId w ) const;

    void propagateClients( bool onlyStacking = FALSE);

    bool addDockwin( WId w );
    bool removeDockwin( WId w );
    void propagateDockwins();
    DockWindow findDockwin( WId w );

    //CT needed for cascading+
    struct CascadingInfo {
      QPoint pos;
      int col;
      int row;
    };
    
    // mouse emulation
    WId getMouseEmulationWindow();
    enum MouseEmulation { EmuPress, EmuRelease, EmuMove };
    unsigned int sendFakedMouseEvent( QPoint pos, WId win, MouseEmulation type, int button, unsigned int state ); // returns the new state

    // ------------------

    DockWindowList dockwins;

    int current_desktop;
    int number_of_desktops;

    QGuardedPtr<Client> popup_client;

    void loadSessionInfo();

    QWidget* desktop_widget;

    QList<SessionInfo> session;
    QValueList<CascadingInfo> cci;

    Client* desktop_client;
    Client* active_client;
    Client* should_get_focus;

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

    PluginMgr mgr;

    RootInfo *rootInfo;
    QWidget* supportWindow;

    QRect area;
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

#endif
