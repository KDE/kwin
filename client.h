/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/
#ifndef CLIENT_H
#define CLIENT_H

#include "options.h"
#include <qframe.h>
#include <qvbox.h>
#include <qpixmap.h>
#include <qtimer.h>
#include <netwm_def.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

class NETWinInfo;

namespace KWinInternal {

class Workspace;
class Client;
class WinInfo;

class WindowWrapperPrivate;
class ClientPrivate;

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

    void show();
    void hide();

    void map();
    void unmap();

protected:
    void resizeEvent( QResizeEvent * );
    bool x11Event( XEvent * );		// X11 event

private slots:
    void deferredResize();

private:
    WId win;
    Time lastMouseEventTime;
    bool reparented;
    WindowWrapperPrivate* d;
};

inline WId WindowWrapper::window() const
{
    return win;
}


// NOTE: this class has to keep binary compatibility, just like other
// KWin classes accessible from the plugins
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
    NET::WindowType windowType() const;

    virtual bool windowEvent( XEvent * );

    bool manage( bool isMapped = FALSE, bool doNotShow = FALSE, bool isInitial = TRUE );

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
    virtual void setShade( bool , int = 0 );
    void giveUpShade();

    bool isMaximized() const;
    enum MaximizeMode { MaximizeRestore = 0, MaximizeVertical = 1, MaximizeHorizontal = 2, MaximizeFull = 3,  MaximizeAdjust };
    bool isMaximizable() const;
    QRect geometryRestore() const;
    MaximizeMode maximizeMode() const;
    bool isMinimizable() const;

    bool isSticky() const;
    void setSticky( bool );

    bool staysOnTop() const;
    void setStaysOnTop( bool );

    bool skipTaskbar() const;
    void setSkipTaskbar( bool );

    bool skipPager() const;
    void setSkipPager( bool );

    bool storeSettings() const;
    void setStoreSettings( bool );

    // auxiliary functions, depend on the windowType
    bool wantsTabFocus() const;
    bool wantsInput() const;
    bool isMovable() const;
    bool isDesktop() const;
    bool isDock() const;
    bool isTool() const;
    bool isMenu() const;

    bool isResizable() const;

    void takeFocus( bool force = FALSE );

    void setMask( const QRegion & );
    const QRegion& getMask() const;

    // transparent stuff
    virtual void drawbound( const QRect& geom );
    virtual void clearbound();

    // fullscreen hint, for stacking
    bool isFullScreen() const;

    // shape extensions
    bool shape() const;
    void updateShape();

    void setGeometry( int x, int y, int w, int h );
    void  setGeometry( const QRect &r )
    { setGeometry( r.left(), r.top(), r.width(), r.height() ); }
    void move( int x, int y );
    void move( const QPoint & p )
    { move( p.x(), p.y() ); }

    bool providesContextHelp() const;

    bool performMouseCommand( Options::MouseCommand, QPoint globalPos );

    QCString windowRole();
    QCString sessionId();
    QCString resourceName();
    QCString resourceClass();
    QCString wmCommand();
    QCString wmClientMachine();
    Window   wmClientLeader();

    QRect adjustedClientArea( const QRect& area ) const;

    Colormap colormap() const;

    void cloneMode(Client *);

    void show();
    void hide();

    void maximizeRaw( bool vertically, bool horizontally );

    QString caption() const;
    void setCaption( const QString &);

public slots:
    void iconify();
    void closeWindow();
    void killWindow();
    void maximize( MaximizeMode m );
    void maximize();
    void toggleSticky();
    void toggleShade();
    void contextHelp();
    void autoRaise();
    void shadeHover();

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
    bool x11Event( XEvent * );		// X11 event

    virtual void activateLayout();

    bool eventFilter( QObject *, QEvent * );


    virtual void init();
    virtual void captionChange( const QString& name );
    virtual void iconChange();
    virtual void activeChange( bool );
    virtual void maximizeChange( bool );
    virtual void stickyChange( bool );
    virtual void shadeChange( bool );


    enum MousePosition {
	Nowhere, TopLeft , BottomRight, BottomLeft, TopRight, Top, Bottom, Left, Right, Center
    };

    virtual MousePosition mousePosition( const QPoint& ) const;
    virtual void setMouseCursor( MousePosition m );


    virtual void  animateIconifyOrDeiconify( bool iconify );
    virtual QPixmap animationPixmap( int w );


    // handlers for X11 events
    bool mapRequest( XMapRequestEvent& e );
    bool unmapNotify( XUnmapEvent& e );
    bool configureRequest( XConfigureRequestEvent& e );
    bool propertyNotify( XPropertyEvent& e );
    bool clientMessage( XClientMessageEvent& e );

    NETWinInfo * netWinInfo();

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
    void sendSyntheticConfigureNotify();
    int state;
    QRect original_geometry;
    QRect geom; //### TODO
    WId transient_for;
    uint transient_for_defined;
    uint shaded :1;
    uint hover_unshade :1;
    uint active :1;
    uint is_sticky :1;
    uint stays_on_top : 1;
    uint is_shape :1;
    uint may_move :1;
    uint is_fullscreen :1;
    uint skip_taskbar :1;
    uint Pdeletewindow :1; // does the window understand the DeleteWindow protocol?
    uint Ptakefocus :1;// does the window understand the TakeFocus protocol?
    uint Pcontexthelp : 1; // does the window understand the ContextHelp protocol?
    uint input :1; // does the window want input in its wm_hints
    uint store_settings : 1;
    uint skip_pager : 1;
    void getWMHints();
    void getWindowProtocols();
    QPixmap icon_pix;
    QPixmap miniicon_pix;
    QRect geom_restore;
    MaximizeMode max_mode;
    QRegion mask;
    WinInfo* info;
    QTimer* autoRaiseTimer;
    QTimer* shadeHoverTimer;
    Colormap cmap;
    QCString resource_name;
    QCString resource_class;
    void verifyTransientFor();
    friend class WindowWrapper;
    QString cap;
    WId wmClientLeaderWin;
    void getWmClientLeader();

 public:
    static QCString staticWindowRole(WId);
    static QCString staticSessionId(WId);
    static QCString staticWmCommand(WId);
    static QCString staticWmClientMachine(WId);
    static Window   staticWmClientLeader(WId);
    
 private:
    ClientPrivate* d;

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
    return transient_for != 0 || transient_for_defined;
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

inline bool Client::isSticky() const
{
    return is_sticky;
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
    return max_mode != MaximizeRestore;
}

inline QRect Client::geometryRestore() const
{
    return geom_restore;
}

inline Client::MaximizeMode Client::maximizeMode() const
{
    return max_mode;
}

inline bool Client::staysOnTop() const
{
    return stays_on_top;
}

inline bool Client::skipTaskbar() const
{
    return skip_taskbar;
}

inline bool Client::skipPager() const
{
    return skip_pager;
}

inline bool Client::storeSettings() const
{
    return store_settings;
}

inline void Client::setStoreSettings( bool b )
{
    store_settings = b;
}


inline bool Client::shape() const
{
    return is_shape;
}


inline bool Client::isFullScreen() const
{
    return is_fullscreen;
}

inline const QRegion& Client::getMask() const
{
    return mask;
}


inline Colormap Client::colormap() const
{
    return cmap;
}

class NoBorderClient : public Client
{
    Q_OBJECT
public:
    NoBorderClient( Workspace *ws, WId w, QWidget *parent=0, const char *name=0 );
    ~NoBorderClient();
    void setShade( bool , int ){}
};

};

#endif
