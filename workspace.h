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
#include <X11/Xlib.h>
#include "options.h"
#include "plugins.h"
class Client;
class TabBox;

class KConfig;
class KGlobalAccel;

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

class Workspace : public QObject
{
    Q_OBJECT
public:
    Workspace( bool restore = FALSE );
    virtual ~Workspace();

    virtual bool workspaceEvent( XEvent * );

    bool hasClient(Client *);

    Client* findClient( WId w ) const;

    QRect geometry() const;
    QRect clientArea() const;

    bool destroyClient( Client* );

    WId rootWin() const;

    Client* activeClient() const;
    void setActiveClient( Client* );
    void activateClient( Client* );
    void requestFocus( Client* c);

    void doPlacement( Client* c );
    QPoint adjustClientPosition( Client* c, QPoint pos );
    void raiseClient( Client* c );

    void clientHidden( Client*  );

    int currentDesktop() const;
    int numberOfDesktops() const;
    void setNumberOfDesktops( int n );

    QWidget* desktopWidget();

    void grabKey(KeySym keysym, unsigned int mod);

    Client* nextClient(Client*) const;
    Client* previousClient(Client*) const;
    Client* nextStaticClient(Client*) const;
    Client* previousStaticClient(Client*) const;

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

private slots:
    void desktopPopupAboutToShow();
    void clientPopupAboutToShow();
    void sendToDesktop( int );
    void clientPopupActivated( int );

protected:
    bool keyPress( XKeyEvent key );
    bool keyRelease( XKeyEvent key );
    bool keyPressMouseEmulation( XKeyEvent key );
    bool clientMessage( XClientMessageEvent msg );

private:
    void init();
    KGlobalAccel *keys;
    void createKeybindings();
    WId root;
    ClientList clients;
    ClientList stacking_order;
    ClientList focus_chain;
    Client* active_client;
    bool control_grab;
    bool tab_grab;
    bool mouse_emulation;
    TabBox* tab_box;
    void freeKeyboard(bool pass);
    QGuardedPtr<Client> popup_client;
    QPopupMenu *popup;
    QPopupMenu *desk_popup;
    Client* should_get_focus;

    void raiseTransientsOf( ClientList& safeset, Client* c );
    void randomPlacement(Client* c);
    void smartPlacement(Client* c);
    void cascadePlacement(Client* c, bool re_init = false);

    enum CleanupType { Cascade, Unclutter };
    void deskCleanup(CleanupType);

    void focusToNull();
    Client* desktop_client;
    int current_desktop;
    int number_of_desktops;
    Client* findClientWidthId( WId w ) const;

    QWidget* desktop_widget;

    void propagateClients( bool onlyStacking = FALSE);

    DockWindowList dockwins;
    bool addDockwin( WId w );
    bool removeDockwin( WId w );
    void propagateDockwins();
    DockWindow findDockwin( WId w );

    QList<SessionInfo> session;
    void loadSessionInfo();

    //CT needed for cascading+
    struct CascadingInfo {
      QPoint pos;
      int col;
      int row;
    };
    QValueList<CascadingInfo> cci;
    // -cascading
    Atom kwm_command;

    PluginMgr mgr;
};

inline WId Workspace::rootWin() const
{
    return root;
}

/*!
  Returns the active client, i.e. the client that has the focus (or None if no
  client has the focus)
 */
inline Client* Workspace::activeClient() const
{
    return active_client;
}


/*!
  Returns the current virtual desktop of this workspace
 */
inline int Workspace::currentDesktop() const
{
    return current_desktop;
}

/*!
  Returns the number of virtual desktops of this workspace
 */
inline int Workspace::numberOfDesktops() const
{
    return number_of_desktops;
}

#endif
