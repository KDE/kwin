#ifndef CLIENT_H
#define CLIENT_H

#include "options.h"
#include <kwm.h>
#include <qframe.h>
#include <qvbox.h>
#include <qpixmap.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class Workspace;
class Client;

class WindowWrapper : public QWidget
{
    Q_OBJECT
public:
    WindowWrapper( WId w, Client *parent=0, const char* name=0);
    ~WindowWrapper();

    WId window() const;
    void releaseWindow();
    void invalidateWindow();
    QSize sizeHint() const;
    QSizePolicy sizePolicy() const;

    void setActive( bool );

protected:
    void resizeEvent( QResizeEvent * );
    void showEvent( QShowEvent* );
    void hideEvent( QHideEvent* );
    bool x11Event( XEvent * );		// X11 event

private:
    WId win;
    Time lastMouseEventTime;
    bool reparented;
};

inline WId WindowWrapper::window() const
{
    return win;
}



class Client : public QWidget
{
    Q_OBJECT
public:
    Client( Workspace *ws, WId w, QWidget *parent=0, const char *name=0, WFlags f = 0);
    ~Client();

    WId window() const;
    WindowWrapper* windowWrapper() const;
    Workspace* workspace() const;
    void releaseWindow();
    void invalidateWindow();
    WId transientFor() const;
    bool isTransient() const;
    Client* mainClient();

    bool avoid() const { return avoid_; }
    int anchorEdge() const { return anchorEdge_; }

    virtual bool windowEvent( XEvent * );

    void manage( bool isMapped = FALSE );

    void setMappingState( int s );
    int mappingState() const;

    void requestActivation();
    void withdraw();

    QSize adjustedSize( const QSize& ) const;

    QPixmap icon() const;
    QPixmap miniIcon() const;


    // is the window in withdrawn state?
    bool isWithdrawn(){
	return state == WithdrawnState;
    }
    // is the window in iconic state?
    bool isIconified(){
	return state == IconicState;
    }
    // is the window in normal state?
    bool isNormal(){
	return state == NormalState;
    }

    bool isActive() const;
    void setActive( bool );

    int desktop() const;
    void setDesktop( int );
    bool isOnDesktop( int d ) const;

    bool isShade() const;
    virtual void setShade( bool );

    bool isMaximized() const;
    enum MaximizeMode { MaximizeVertical, MaximizeHorizontal, MaximizeFull };

    bool isSticky() const;
    void setSticky( bool );

    void takeFocus();

    void setMask( const QRegion & );

    // transparent stuff
    virtual void drawbound( const QRect& geom );
    virtual void clearbound();


    // shape extensions
    bool shape() const;
    void updateShape();

    void setGeometry( int x, int y, int w, int h );
    void  setGeometry( const QRect &r )
    { setGeometry( r.left(), r.top(), r.width(), r.height() ); }
    void move( int x, int y );
    void move( const QPoint & p )
    { move( p.x(), p.y() ); }


    virtual bool wantsTabFocus() const { return TRUE;} //### just for now

    bool providesContextHelp() const;

    bool performMouseCommand( Options::MouseCommand, QPoint globalPos );

    QCString windowRole();
    QCString sessionId();

public slots:
    void iconify();
    void closeWindow();
    void maximize( MaximizeMode );
    void maximize();
    void toggleSticky();
    void contextHelp();

protected:
    void paintEvent( QPaintEvent * );
    void mousePressEvent( QMouseEvent * );
    void mouseReleaseEvent( QMouseEvent * );
    void mouseMoveEvent( QMouseEvent * );
    void keyPressEvent( QKeyEvent * );
    void resizeEvent( QResizeEvent * );
    virtual void windowWrapperShowEvent( QShowEvent* ){}
    virtual void windowWrapperHideEvent( QHideEvent* ){}
    void enterEvent( QEvent * );
    void leaveEvent( QEvent * );
    void showEvent( QShowEvent* );
    void hideEvent( QHideEvent* );
    bool x11Event( XEvent * );		// X11 event

    bool eventFilter( QObject *, QEvent * );


    virtual void init();
    virtual void captionChange( const QString& name );
    virtual void iconChange();
    virtual void activeChange( bool );
    virtual void maximizeChange( bool );
    virtual void stickyChange( bool );


    enum MousePosition {
	Nowhere, TopLeft , BottomRight, BottomLeft, TopRight, Top, Bottom, Left, Right, Center
    };

    virtual MousePosition mousePosition( const QPoint& ) const;
    virtual void setMouseCursor( MousePosition m );

    // handlers for X11 events
    bool mapRequest( XMapRequestEvent& e );
    bool unmapNotify( XUnmapEvent& e );
    bool configureRequest( XConfigureRequestEvent& e );
    bool propertyNotify( XPropertyEvent& e );
    bool clientMessage( XClientMessageEvent& e );

private:
    QSize sizeForWindowSize( const QSize&, bool ignore_height = FALSE ) const;
    void getWmNormalHints();
    void fetchName();
    void gravitate( bool invert );


    WId win;
    WindowWrapper* wwrap;
    Workspace* wspace;
    int desk;
    bool buttonDown;
    bool moveResizeMode;
    bool isMove() const {
	return moveResizeMode && mode == Center;
    }
    bool isResize() const {
	return moveResizeMode && !isMove();
    }
    MousePosition mode;
    QPoint moveOffset;
    QPoint invertedMoveOffset;
    QSize clientSize;
    XSizeHints  xSizeHint;
    void sendSynteticConfigureNotify();
    int state;
    bool active;
    QRect original_geometry;
    QRect geom; //### TODO
    bool shaded;
    WId transient_for;
    bool is_sticky;
    bool is_shape;
    void getWMHints();
    void getWindowProtocols();
    uint Pdeletewindow :1; // does the window understand the DeleteWindow protocol?
    uint Ptakefocus :1;// does the window understand the TakeFocus protocol?
    uint Pcontexthelp : 1; // does the window understand the ContextHelp protocol?
    uint input :1; // does the window want input in its wm_hints
    uint mapped :1; // keeps track of our visiblity within the asynchronous event flow
    QPixmap icon_pix;
    QPixmap miniicon_pix;
    QRect geom_restore;
    QRegion mask;

    bool avoid_;
    int anchorEdge_;
};

inline WId Client::window() const
{
    return win;
}

inline WindowWrapper* Client::windowWrapper() const
{
    return wwrap;
}

inline Workspace* Client::workspace() const
{
    return wspace;
}

inline WId Client::transientFor() const
{
    return transient_for;
}

inline bool Client::isTransient() const
{
    return transient_for != 0;
}



inline int Client::mappingState() const
{
    return state;
}


inline bool Client::isActive() const
{
    return active;
}

/*!
  Returns the virtual desktop within the workspace() the client window
  is located in, -1 if it isn't located on any special desktop. This may be
  if the window wasn't mapped yet or if the window is sticky. Do not use
  desktop() directly, use isOnDesktop() instead.
 */
inline int Client::desktop() const
{
    return desk;
 }

/*!
  Returns whether the client is on visible or iconified on the virtual
  desktop \a d. This is always TRUE for sticky clients.
 */
inline bool Client::isOnDesktop( int d ) const
{
    return desk == d || desk == -1 || isSticky();
}


inline QPixmap Client::icon() const
{
    return icon_pix;
}

inline QPixmap Client::miniIcon() const
{
    return miniicon_pix;
}


/*!
  Is the client maximized?
 */
inline bool Client::isMaximized() const
{
    return !geom_restore.isNull();
}

inline bool Client::isSticky() const
{
    return is_sticky;
}


inline bool Client::shape() const
{
    return is_shape;
}

class NoBorderClient : public Client
{
    Q_OBJECT
public:
    NoBorderClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~NoBorderClient();

    bool wantsTabFocus() const { return FALSE;} //### just for now
};

#endif
