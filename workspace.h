#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <qwidget.h>
#include <qapplication.h>
#include <qpopupmenu.h>
#include <qvaluelist.h>
#include <X11/Xlib.h>

class Client;
class TabBox;

class KGlobalAccel;

typedef QValueList<Client*> ClientList;
typedef QValueList<WId> WIdList;

class Shape {
public:
    static bool hasShape( WId w);
    static int shapeEvent();
};

class Workspace : public QObject
{
    Q_OBJECT
public:
    Workspace();
    Workspace( WId rootwin );
    virtual ~Workspace();

    virtual bool workspaceEvent( XEvent * );

    bool hasClient(Client *);

    Client* findClient( WId w ) const;

    QRect geometry() const;

    bool destroyClient( Client* );

    WId rootWin() const;

    Client* activeClient() const;
    void setActiveClient( Client* );
    void activateClient( Client* );
    void requestFocus( Client* c);

    void doPlacement( Client* c );
    void raiseClient( Client* c );

    void clientHidden( Client*  );

    int currentDesktop() const;
    void setCurrentDesktop( int new_desktop );
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


    void showPopup( const QPoint&, Client* );

    void setDesktopClient( Client* );

    void makeFullScreen( Client* );


protected:
    bool keyPress( XKeyEvent key );
    bool keyRelease( XKeyEvent key );
    bool clientMessage( XClientMessageEvent msg );
	
private:
    KGlobalAccel *keys;
    void init();
    WId root;
    ClientList clients;
    ClientList stacking_order;
    ClientList focus_chain;
    Client* active_client;
    bool control_grab;
    bool tab_grab;
    TabBox* tab_box;
    void freeKeyboard(bool pass);
    QPopupMenu *popup;
    Client* should_get_focus;

    void raiseTransientsOf( ClientList& safeset, Client* c );
    void randomPlacement(Client* c);

    void focusToNull();
    Client* desktop_client;
    int current_desktop;
    int number_of_desktops;

    Client* popup_client;
    QWidget* desktop_widget;

    //experimental
    void setDecoration( int deco );

    void propagateClients( bool onlyStacking = FALSE);

    WIdList dockwins;
    bool addDockwin( WId w );
    bool removeDockwin( WId w );
    void propagateDockwins();
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
    return 4;
}

#endif
