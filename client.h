/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_CLIENT_H
#define KWIN_CLIENT_H

#include "utils.h"
#include "options.h"
#include "workspace.h"
#include "kdecoration.h"
#include <qframe.h>
#include <qvbox.h>
#include <qpixmap.h>
#include <netwm.h>
#include <kdebug.h>
#include <assert.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fixx11h.h>

class NETWinInfo;
class QTimer;
class KProcess;
class KStartupInfoData;

namespace KWinInternal
{

class Workspace;
class Client;
class WinInfo;
class SessionInfo;
class Bridge;

class Client : public QObject, public KDecorationDefines
    {
    Q_OBJECT
    public:
        Client( Workspace *ws );
        Window window() const;
        Window frameId() const;
        Window wrapperId() const;
        Window decorationId() const;

        Workspace* workspace() const;
        const Client* transientFor() const;
        Client* transientFor();
        bool isTransient() const;
        bool groupTransient() const;
        ClientList mainClients() const; // call once before loop
        bool hasTransient( const Client* c, bool indirect ) const;
        const ClientList& transients() const;
        void checkTransient( Window w );
        Client* findModal();
        const Group* group() const;
        Group* group();
    // prefer isXXX() instead
        NET::WindowType windowType( bool strict = false, int supported_types = SUPPORTED_WINDOW_TYPES_MASK ) const;

        QRect geometry() const;
        QSize size() const;
        QPoint pos() const;
        QRect rect() const;
        int x() const;
        int y() const;
        int width() const;
        int height() const;
        QPoint clientPos() const; // inside of geometry()
        QSize clientSize() const;

        void windowEvent( XEvent* e );
        bool decorationEvent( XEvent* e );

        bool manage( Window w, bool isMapped );

        void releaseWindow( bool on_shutdown = false );

        QSize adjustedSize( const QSize& ) const;

        QPixmap icon() const;
        QPixmap miniIcon() const;

        bool isActive() const;
        void setActive( bool );

        int desktop() const;
        void setDesktop( int );
        bool isOnDesktop( int d ) const;
        bool isOnCurrentDesktop() const;
        bool isOnAllDesktops() const;
        void setOnAllDesktops( bool set );

    // !isShade() && !isMinimized() && not hidden, i.e. normally visible on some virtual desktop
    // SELI this may possibly clash with QWidget::isShown(), as long as Client is a QWidget
        bool isShown() const;

        enum ShadeMode
            {
            ShadeNone, // not shaded
            ShadeNormal, // normally shaded - isShade() is true only here
            ShadeHover, // "shaded", but visible due to hover unshade
            ShadeActivated // "shaded", but visible due to alt+tab to the window
            };
        bool isShade() const;
        void setShade( ShadeMode mode );
        bool isShadeable() const;

        bool isMinimized() const;
        bool isMaximizable() const;
        QRect geometryRestore() const;
        MaximizeMode maximizeMode() const;
        bool isMinimizable() const;
        void setMaximize( bool vertically, bool horizontally );

        void setFullScreen( bool set, bool user );
        bool isFullScreen() const;
        bool isFullScreenable() const;
        bool userCanSetFullScreen() const;
        QRect geometryFSRestore() const { return geom_fs_restore; } // only for session saving
        int fullScreenMode() const { return fullscreen_mode; } // only for session saving

        bool isUserNoBorder() const;
        void setUserNoBorder( bool set );
        bool userCanSetNoBorder() const;
        bool noBorder() const;

        bool skipTaskbar( bool from_outside = false ) const;
        void setSkipTaskbar( bool set, bool from_outside );

        bool skipPager() const;
        void setSkipPager( bool );

        bool keepAbove() const;
        void setKeepAbove( bool );
        bool keepBelow() const;
        void setKeepBelow( bool );
        Layer layer() const;
        Layer belongsToLayer() const;
        void invalidateLayer();

        void setModal( bool modal );
        bool isModal() const;

        bool storeSettings() const;
        void setStoreSettings( bool );

    // auxiliary functions, depend on the windowType
        bool wantsTabFocus() const;
        bool wantsInput() const;
        bool isMovable() const;
        bool isDesktop() const;
        bool isDock() const;
        bool isToolbar() const;
        bool isTopMenu() const;
        bool isMenu() const;
        bool isNormalWindow() const; // normal as in 'NET::Normal or NET::Unknown non-transient'
        bool isDialog() const;
        bool isSplash() const;
        bool isUtility() const;
        bool isOverride() const; // not override redirect, but NET::Override
    // returns true for "special" windows and false for windows which are "normal"
    // (normal=window which has a border, can be moved by the user, can be closed, etc.)
    // true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
    // false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
        bool isSpecialWindow() const;

        bool isResizable() const;
        bool isCloseable() const; // may be closed by the user (may have a close button)

        void takeFocus( bool force, allowed_t );
        void demandAttention( bool set = true );

        void setMask( const QRegion& r, int mode = X::Unsorted );

        void updateDecoration( bool check_workspace_pos, bool force = false );
        void checkBorderSizes();

    // shape extensions
        bool shape() const;
        void updateShape();

        void setGeometry( int x, int y, int w, int h, bool force = false );
        void setGeometry( const QRect& r, bool force = false );
        void move( int x, int y, bool force = false );
        void move( const QPoint & p, bool force = false );
        void resize( int w, int h, bool force = false );
        void resize( const QSize& s, bool force = false );

        void growHorizontal();
        void shrinkHorizontal();
        void growVertical();
        void shrinkVertical();

        bool providesContextHelp() const;

        bool performMouseCommand( Options::MouseCommand, QPoint globalPos );

        QCString windowRole() const;
        QCString sessionId();
        QCString resourceName() const;
        QCString resourceClass() const;
        QCString wmCommand();
        QCString wmClientMachine() const;
        Window   wmClientLeader() const;
        pid_t pid() const;

        QRect adjustedClientArea( const QRect& area ) const;

        Colormap colormap() const;

    // hides a client - basically like minimize, but without effects, it's simply hidden
        void hideClient( bool hide );
    // updates visibility depending on whether it's on the current desktop
        void virtualDesktopChange();

        QString caption() const;

        void keyPressEvent( uint key_code ); // FRAME ??

        const QPoint calculateGravitation( bool invert ) const; // FRAME public?

        void NETMoveResize( int x_root, int y_root, NET::Direction direction );

        void gotPing( Time timestamp );

        static QCString staticWindowRole(WId);
        static QCString staticSessionId(WId);
        static QCString staticWmCommand(WId);
        static QCString staticWmClientMachine(WId);
        static Window   staticWmClientLeader(WId);

        void checkWorkspacePosition();
        void updateUserTime( Time time = CurrentTime );
        Time userTime() const;

    // does 'delete c;'
        static void deleteClient( Client* c, allowed_t );

        static bool resourceMatch( const Client* c1, const Client* c2 );
        static bool belongToSameApplication( const Client* c1, const Client* c2, bool active_hack = false );
        static void readIcons( Window win, QPixmap* icon, QPixmap* miniicon );

    public slots: // FRAME these probably don't need to be slots anymore
        void minimize();
        void unminimize();
        void closeWindow();
        void killWindow();
        void maximize( MaximizeMode );
        void toggleOnAllDesktops();
        void toggleShade();
        void showContextHelp();
        void autoRaise();
        void shadeHover();
        void destroyClient();

    private:
        friend class Bridge; // FRAME
        virtual void processMousePressEvent( QMouseEvent* e );

    private: // TODO cleanup the order of things in the .h file
    // use Workspace::createClient()
        virtual ~Client(); // use destroyClient() or releaseWindow()

        MousePosition mousePosition( const QPoint& ) const;
        void setCursor( MousePosition m );
        void setCursor( const QCursor& c );

        void  animateMinimizeOrUnminimize( bool minimize );
        QPixmap animationPixmap( int w );
    // transparent stuff
        void drawbound( const QRect& geom );
        void clearbound();
        void doDrawbound( const QRect& geom, bool clear );

    // handlers for X11 events
        void mapRequestEvent( XMapRequestEvent* e );
        void unmapNotifyEvent( XUnmapEvent*e );
        void configureRequestEvent( XConfigureRequestEvent* e );
        void propertyNotifyEvent( XPropertyEvent* e );
        void clientMessageEvent( XClientMessageEvent* e );
        bool buttonPressEvent( XButtonEvent* e );
        bool buttonReleaseEvent( XButtonEvent* e );
        bool motionNotifyEvent( XMotionEvent* e );
        void enterNotifyEvent( XCrossingEvent* e );
        void leaveNotifyEvent( XCrossingEvent* e );
        void visibilityNotifyEvent( XVisibilityEvent* e );
        void convertCrossingToMotionEvent( XCrossingEvent* e );
        void focusInEvent( XFocusInEvent* e );
        void focusOutEvent( XFocusOutEvent* e );

        void processDecorationButtonPress( int button, int state, int x, int y, int x_root, int y_root );

        NETWinInfo * netWinInfo(); // APICLEAN ???

    private slots:
        void pingTimeout();
        void processKillerExited();

    private:
    // ICCCM 4.1.3.1, 4.1.4 , NETWM 2.5.1
        void setMappingState( int s );
        int mappingState() const;
        bool isIconicState() const;
        bool isNormalState() const;
        bool isManaged() const; // returns false if this client is not yet managed
        void updateAllowedActions( bool force = false );
        QSize sizeForClientSize( const QSize&, bool ignore_height = FALSE ) const;
        void changeMaximize( bool horizontal, bool vertical, bool adjust );
        void getWmNormalHints();
        void getIcons();
        void getWmClientLeader();
        void fetchName();
        void fetchIconicName();

        void updateWorkareaDiffs( const QRect& area = QRect());
        void checkDirection( int new_diff, int old_diff, QRect& rect, const QRect& area );
        static int computeWorkareaDiff( int left, int right, int a_left, int a_right );

        bool startMoveResize();
        void finishMoveResize( bool cancel );
        void leaveMoveResize();
        void positionGeometryTip();
        bool grabInput();
        void ungrabInput();
        void updateMouseGrab();
        void grabButton( int mod );
        void ungrabButton( int mod );
        void resetMaximize();
        void resizeDecoration( const QSize& s );

        void pingWindow();
        void killProcess( bool ask, Time timestamp = CurrentTime );

        void embedClient( Window w );    
        void detectNoBorder();
        void destroyDecoration();
        void updateFrameStrut();

        void rawShow(); // just shows it
        void rawHide(); // just hides it

        Time readUserTimeMapTimestamp( const KStartupInfoData* asn_data,
            const SessionInfo* session ) const;
        Time readUserCreationTime() const;
        static bool sameAppWindowRoleMatch( const Client* c1, const Client* c2, bool active_hack );
        void startupIdChanged();

        Window client;
        Window wrapper;
        Window frame;
        KDecoration* decoration;
        Workspace* wspace;
        Bridge* bridge;
        int desk;
        bool buttonDown;
        bool moveResizeMode;
        bool move_faked_activity;
        bool unrestrictedMoveResize;
        bool isMove() const 
            {
            return moveResizeMode && mode == Center;
            }
        bool isResize() const 
            {
            return moveResizeMode && !isMove();
            }

        MousePosition mode;
        QPoint moveOffset;
        QPoint invertedMoveOffset;
        QRect moveResizeGeom;
        QRect initialMoveResizeGeom;
        XSizeHints  xSizeHint;
        void sendSyntheticConfigureNotify();
        int mapping_state;
        void readTransient();
        Window verifyTransientFor( Window transient_for, bool set );
        void addTransient( Client* cl );
        void removeTransient( Client* cl );
        void removeFromMainClients();
        void checkGroupTransients();
        void setTransient( Window new_transient_for_id );
        Client* transient_for;
        Window transient_for_id;
        Window original_transient_for_id;
        ClientList transients_list; // SELI make this ordered in stacking order?
        ShadeMode shade_mode;
        uint active :1;
        uint keep_above : 1; // NET::KeepAbove (was stays_on_top)
        uint is_shape :1;
        uint may_move :1;
        uint skip_taskbar :1;
        uint original_skip_taskbar :1; // unaffected by KWin
        uint Pdeletewindow :1; // does the window understand the DeleteWindow protocol?
        uint Ptakefocus :1;// does the window understand the TakeFocus protocol?
        uint Pcontexthelp : 1; // does the window understand the ContextHelp protocol?
        uint Pping : 1; // does it support _NET_WM_PING?
        uint input :1; // does the window want input in its wm_hints
        uint store_settings : 1;
        uint skip_pager : 1;
        uint may_resize : 1;
        uint may_maximize : 1;
        uint may_minimize : 1;
        uint may_close : 1;
        uint keep_below : 1; // NET::KeepBelow
        uint minimized : 1;
        uint hidden : 1; // forcibly hidden by calling hide()
        uint modal : 1; // NET::Modal
        uint noborder : 1;
        uint user_noborder : 1;
        uint not_obscured : 1;
        void getWMHints();
        void readIcons();
        void getWindowProtocols();
        QPixmap icon_pix;
        QPixmap miniicon_pix;
        QCursor cursor;
    // FullScreenHack - non-NETWM fullscreen (noborder,size of desktop)
    // DON'T reorder - saved to config files !!!
        enum FullScreenMode { FullScreenNone, FullScreenNormal, FullScreenHack };
        FullScreenMode fullscreen_mode;
        MaximizeMode max_mode;
        QRect geom_restore;
        QRect geom_fs_restore;
        int workarea_diff_x, workarea_diff_y;
        WinInfo* info;
        QTimer* autoRaiseTimer;
        QTimer* shadeHoverTimer;
        Colormap cmap;
        QCString resource_name;
        QCString resource_class;
        QString cap_normal, cap_iconic, cap_suffix;
        WId wmClientLeaderWin;
        QCString window_role;
        void checkGroup();
        Group* in_group;
        Window window_group;
        Layer in_layer;
        QTimer* ping_timer;
        KProcess* process_killer;
        Time ping_timestamp;
        Time user_time;
        bool input_grabbed;
        unsigned long allowed_actions;
        QRect frame_geometry;
        QSize client_size;
        int block_geometry; // >0 - new geometry is remembered, but not actually set
        int border_left, border_right, border_top, border_bottom;
        friend struct FetchNameInternalPredicate;
        void show() { assert( false ); } // SELI remove after Client is no longer QWidget
        void hide() { assert( false ); }
    };

// NET WM Protocol handler class
class WinInfo : public NETWinInfo
    {
    private:
        typedef KWinInternal::Client Client; // because of NET::Client
    public:
        WinInfo( Client* c, Display * display, Window window,
                Window rwin, const unsigned long pr[], int pr_size );
        virtual void changeDesktop(int desktop);
        virtual void changeState( unsigned long state, unsigned long mask );
    private:
        Client * m_client;
    };

inline Window Client::window() const
    {
    return client;
    }

inline Window Client::frameId() const
    {
    return frame;
    }

inline Window Client::wrapperId() const
    {
    return wrapper;
    }

inline Window Client::decorationId() const
    {
    return decoration != NULL ? decoration->widget()->winId() : None;
    }

inline Workspace* Client::workspace() const
    {
    return wspace;
    }

inline const Client* Client::transientFor() const
    {
    return transient_for;
    }

inline Client* Client::transientFor()
    {
    return transient_for;
    }

inline bool Client::groupTransient() const
    {
    return transient_for_id == workspace()->rootWin();
    }

inline bool Client::isTransient() const
    {
    return transient_for_id != None;
    }

inline const ClientList& Client::transients() const
    {
    return transients_list;
    }

inline const Group* Client::group() const
    {
    return in_group;
    }

inline Group* Client::group()
    {
    return in_group;
    }

inline int Client::mappingState() const
    {
    return mapping_state;
    }

inline QCString Client::resourceName() const
    {
    return resource_name;
    }

inline QCString Client::resourceClass() const
    {
    return resource_class;
    }

inline
bool Client::isMinimized() const
    {
    return minimized;
    }

inline bool Client::isActive() const
    {
    return active;
    }

/*!
  Returns the virtual desktop within the workspace() the client window
  is located in, 0 if it isn't located on any special desktop (not mapped yet),
  or NET::OnAllDesktops. Do not use desktop() directly, use
  isOnDesktop() instead.
 */
inline int Client::desktop() const
    {
    return desk;
    }

inline bool Client::isOnAllDesktops() const
    {
    return desk == NET::OnAllDesktops;
    }
/*!
  Returns whether the client is on the virtual desktop \a d.
  This is always TRUE for onAllDesktops clients.
 */
inline bool Client::isOnDesktop( int d ) const
    {
    return desk == d || /*desk == 0 ||*/ isOnAllDesktops();
    }

inline
bool Client::isShown() const
    {
    return !isMinimized() && !isShade() && !hidden;
    }

inline
bool Client::isShade() const
    {
    return shade_mode == ShadeNormal;
    }

inline QPixmap Client::icon() const
    {
    return icon_pix;
    }

inline QPixmap Client::miniIcon() const
    {
    return miniicon_pix;
    }

inline QRect Client::geometryRestore() const
    {
    return geom_restore;
    }

inline Client::MaximizeMode Client::maximizeMode() const
    {
    return max_mode;
    }

inline bool Client::skipTaskbar( bool from_outside ) const
    {
    return from_outside ? original_skip_taskbar : skip_taskbar;
    }

inline bool Client::skipPager() const
    {
    return skip_pager;
    }

inline bool Client::keepAbove() const
    {
    return keep_above;
    }

inline bool Client::keepBelow() const
    {
    return keep_below;
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
    return fullscreen_mode != FullScreenNone;
    }

inline bool Client::isModal() const
    {
    return modal;
    }

inline Colormap Client::colormap() const
    {
    return cmap;
    }

inline pid_t Client::pid() const
    {
    return info->pid();
    }

inline void Client::invalidateLayer()
    {
    in_layer = UnknownLayer;
    }

inline bool Client::isIconicState() const
    {
    return mapping_state == IconicState;
    }

inline bool Client::isNormalState() const
    {
    return mapping_state == NormalState;
    }

inline bool Client::isManaged() const
    {
    return mapping_state != WithdrawnState;
    }

inline
Time Client::userTime() const
    {
    Q_ASSERT( user_time != CurrentTime );
    if( user_time == CurrentTime )
        kdDebug() << kdBacktrace() << endl;
    return user_time;
    }

inline QCString Client::windowRole() const
    {
    return window_role;
    }

inline QRect Client::geometry() const
    {
    return frame_geometry;
    }

inline QSize Client::size() const
    {
    return frame_geometry.size();
    }

inline QPoint Client::pos() const
    {
    return frame_geometry.topLeft();
    }

inline int Client::x() const
    {
    return frame_geometry.x();
    }

inline int Client::y() const
    {
    return frame_geometry.y();
    }

inline int Client::width() const
    {
    return frame_geometry.width();
    }

inline int Client::height() const
    {
    return frame_geometry.height();
    }

inline QRect Client::rect() const
    {
    return QRect( 0, 0, width(), height());
    }

inline QPoint Client::clientPos() const
    {
    return QPoint( border_left, border_top );
    }

inline QSize Client::clientSize() const
    {
    return client_size;
    }

inline void Client::setGeometry( const QRect& r, bool force )
    {
    setGeometry( r.x(), r.y(), r.width(), r.height(), force );
    }

inline void Client::move( const QPoint & p, bool force )
    {
    move( p.x(), p.y(), force );
    }

inline void Client::resize( const QSize& s, bool force )
    {
    resize( s.width(), s.height(), force );
    }

#ifdef NDEBUG
kndbgstream& operator<<( kndbgstream& stream, const Client* );
#else
kdbgstream& operator<<( kdbgstream& stream, const Client* );
#endif

KWIN_COMPARE_PREDICATE( WindowMatchPredicate, Window, cl->window() == value );
KWIN_COMPARE_PREDICATE( FrameIdMatchPredicate, Window, cl->frameId() == value );
KWIN_COMPARE_PREDICATE( WrapperIdMatchPredicate, Window, cl->wrapperId() == value );
KWIN_COMPARE_PREDICATE( DecorationIdMatchPredicate, Window, cl->decorationId() == value );

} // namespace

#endif
