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

#include <qframe.h>
#include <QPixmap>
#include <netwm.h>
#include <kdebug.h>
#include <assert.h>
#include <kshortcut.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <fixx11h.h>

#include "utils.h"
#include "options.h"
#include "workspace.h"
#include "kdecoration.h"
#include "rules.h"
#include "toplevel.h"

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

class Client
    : public Toplevel
    {
    Q_OBJECT
    public:
        Client( Workspace *ws );
        Window window() const;
        Window frameId() const;
        Window wrapperId() const;
        Window decorationId() const;

        const Client* transientFor() const;
        Client* transientFor();
        bool isTransient() const;
        bool groupTransient() const;
        bool wasOriginallyGroupTransient() const;
        ClientList mainClients() const; // call once before loop , is not indirect
        bool hasTransient( const Client* c, bool indirect ) const;
        const ClientList& transients() const; // is not indirect
        void checkTransient( Window w );
        Client* findModal();
        const Group* group() const;
        Group* group();
        void checkGroup( Group* gr = NULL, bool force = false );
        const WindowRules* rules() const;
        void removeRule( Rules* r );
        void setupWindowRules( bool ignore_temporary );
        void applyWindowRules();
        virtual NET::WindowType windowType( bool direct = false, int supported_types = SUPPORTED_WINDOW_TYPES_MASK ) const;
    // returns true for "special" windows and false for windows which are "normal"
    // (normal=window which has a border, can be moved by the user, can be closed, etc.)
    // true for Desktop, Dock, Splash, Override and TopMenu (and Toolbar??? - for now)
    // false for Normal, Dialog, Utility and Menu (and Toolbar??? - not yet) TODO
        bool isSpecialWindow() const;
        bool hasNETSupport() const;

        QSize minSize() const;
        QSize maxSize() const;
        QPoint clientPos() const; // inside of geometry()
        QSize clientSize() const;

        bool windowEvent( XEvent* e );
        virtual bool eventFilter( QObject* o, QEvent* e );

        bool manage( Window w, bool isMapped );

        void releaseWindow( bool on_shutdown = false );

        enum Sizemode // how to resize the window in order to obey constains (mainly aspect ratios)
            {
            SizemodeAny,
            SizemodeFixedW, // try not to affect width
            SizemodeFixedH, // try not to affect height
            SizemodeMax // try not to make it larger in either direction
            };
        QSize adjustedSize( const QSize&, Sizemode mode = SizemodeAny ) const;
        QSize adjustedSize() const;

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

    // !isMinimized() && not hidden, i.e. normally visible on some virtual desktop
        bool isShown( bool shaded_is_shown ) const;

        bool isShade() const; // true only for ShadeNormal
        ShadeMode shadeMode() const; // prefer isShade()
        void setShade( ShadeMode mode );
        bool isShadeable() const;

        bool isMinimized() const;
        bool isMaximizable() const;
        QRect geometryRestore() const;
        MaximizeMode maximizeModeRestore() const;
        MaximizeMode maximizeMode() const;
        bool isMinimizable() const;
        void setMaximize( bool vertically, bool horizontally );

        void setFullScreen( bool set, bool user );
        bool isFullScreen() const;
        bool isFullScreenable( bool fullscreen_hack = false ) const;
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
        int sessionStackingOrder() const;

        void setModal( bool modal );
        bool isModal() const;

    // auxiliary functions, depend on the windowType
        bool wantsTabFocus() const;
        bool wantsInput() const;

        bool isResizable() const;
        bool isMovable() const;
        bool isCloseable() const; // may be closed by the user (may have a close button)

        void takeActivity( int flags, bool handled, allowed_t ); // takes ActivityFlags as arg (in utils.h)
        void takeFocus( allowed_t );
        void demandAttention( bool set = true );

        void setMask( const QRegion& r, int mode = X::Unsorted );
        QRegion mask() const;

        void updateDecoration( bool check_workspace_pos, bool force = false );
        void checkBorderSizes();

    // shape extensions
        bool shape() const;
        void updateShape();
        
        virtual double opacity() const;
        void setOpacity( double opacity );

        void setGeometry( int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet );
        void setGeometry( const QRect& r, ForceGeometry_t force = NormalGeometrySet );
        void move( int x, int y, ForceGeometry_t force = NormalGeometrySet );
        void move( const QPoint & p, ForceGeometry_t force = NormalGeometrySet );
        // plainResize() simply resizes
        void plainResize( int w, int h, ForceGeometry_t force = NormalGeometrySet );
        void plainResize( const QSize& s, ForceGeometry_t force = NormalGeometrySet );
        // resizeWithChecks() resizes according to gravity, and checks workarea position
        void resizeWithChecks( int w, int h, ForceGeometry_t force = NormalGeometrySet );
        void resizeWithChecks( const QSize& s, ForceGeometry_t force = NormalGeometrySet );
        void keepInArea( QRect area, bool partial = false );

        void growHorizontal();
        void shrinkHorizontal();
        void growVertical();
        void shrinkVertical();

        bool providesContextHelp() const;
        KShortcut shortcut() const;
        void setShortcut( const QString& cut );

        bool performMouseCommand( Options::MouseCommand, QPoint globalPos, bool handled = false );

        QByteArray windowRole() const;
        QByteArray sessionId();
        QByteArray resourceName() const;
        QByteArray resourceClass() const;
        QByteArray wmCommand();
        QByteArray wmClientMachine( bool use_localhost ) const;
        Window   wmClientLeader() const;
        pid_t pid() const;

        QRect adjustedClientArea( const QRect& desktop, const QRect& area ) const;

        Colormap colormap() const;

    // updates visibility depending on being shaded, virtual desktop, etc.
        void updateVisibility();
    // hides a client - basically like minimize, but without effects, it's simply hidden
        void hideClient( bool hide );

        QString caption( bool full = true ) const;
        void updateCaption();

        void keyPressEvent( uint key_code ); // FRAME ??
        void updateMouseGrab();
        Window moveResizeGrabWindow() const;

        const QPoint calculateGravitation( bool invert, int gravity = 0 ) const; // FRAME public?

        void NETMoveResize( int x_root, int y_root, NET::Direction direction );
        void NETMoveResizeWindow( int flags, int x, int y, int width, int height );
        void restackWindow( Window above, int detail, NET::RequestSource source, Time timestamp, bool send_event = false );
        
        void gotPing( Time timestamp );

        static QByteArray staticWindowRole(WId);
        static QByteArray staticSessionId(WId);
        static QByteArray staticWmCommand(WId);
        static QByteArray staticWmClientMachine(WId);
        static Window   staticWmClientLeader(WId);

        void checkWorkspacePosition();
        void updateUserTime( Time time = CurrentTime );
        Time userTime() const;
        bool hasUserTimeSupport() const;
        bool ignoreFocusStealing() const;

    // does 'delete c;'
        static void deleteClient( Client* c, allowed_t );

        static bool resourceMatch( const Client* c1, const Client* c2 );
        static bool belongToSameApplication( const Client* c1, const Client* c2, bool active_hack = false );
        static void readIcons( Window win, QPixmap* icon, QPixmap* miniicon );

        void minimize( bool avoid_animation = false );
        void unminimize( bool avoid_animation = false );
        void closeWindow();
        void killWindow();
        void maximize( MaximizeMode );
        void toggleShade();
        void showContextHelp();
        void cancelShadeHover();
        void cancelAutoRaise();
        void destroyClient();
        void checkActiveModal();
        bool hasStrut() const;

        bool isMove() const 
            {
            return moveResizeMode && mode == PositionCenter;
            }
        bool isResize() const 
            {
            return moveResizeMode && mode != PositionCenter;
            }
        
    private slots:
        void autoRaise();
        void shadeHover();
        void shortcutActivated();

    private:
        friend class Bridge; // FRAME
        virtual void processMousePressEvent( QMouseEvent* e );

    private: // TODO cleanup the order of things in the .h file
    // use Workspace::createClient()
        virtual ~Client(); // use destroyClient() or releaseWindow()

        Position mousePosition( const QPoint& ) const;
        void setCursor( Position m );
        void setCursor( const QCursor& c );

        void  animateMinimizeOrUnminimize( bool minimize );
        QPixmap animationPixmap( int w );
    // transparent stuff
        void drawbound( const QRect& geom );
        void clearbound();
        void doDrawbound( const QRect& geom, bool clear );

    // handlers for X11 events
        bool mapRequestEvent( XMapRequestEvent* e );
        void unmapNotifyEvent( XUnmapEvent*e );
        void destroyNotifyEvent( XDestroyWindowEvent*e );
        void configureRequestEvent( XConfigureRequestEvent* e );
        void propertyNotifyEvent( XPropertyEvent* e );
        void clientMessageEvent( XClientMessageEvent* e );
        void enterNotifyEvent( XCrossingEvent* e );
        void leaveNotifyEvent( XCrossingEvent* e );
        void visibilityNotifyEvent( XVisibilityEvent* e );
        void focusInEvent( XFocusInEvent* e );
        void focusOutEvent( XFocusOutEvent* e );

        bool buttonPressEvent( Window w, int button, int state, int x, int y, int x_root, int y_root );
        bool buttonReleaseEvent( Window w, int button, int state, int x, int y, int x_root, int y_root );
        bool motionNotifyEvent( Window w, int state, int x, int y, int x_root, int y_root );

        void processDecorationButtonPress( int button, int state, int x, int y, int x_root, int y_root );

    protected:
        virtual void debug( kdbgstream& stream ) const;

    private slots:
        void pingTimeout();
        void processKillerExited();
        void demandAttentionKNotify();

    private:
    // ICCCM 4.1.3.1, 4.1.4 , NETWM 2.5.1
        void setMappingState( int s );
        int mappingState() const;
        bool isIconicState() const;
        bool isNormalState() const;
        bool isManaged() const; // returns false if this client is not yet managed
        void updateAllowedActions( bool force = false );
        QSize sizeForClientSize( const QSize&, Sizemode mode = SizemodeAny, bool noframe = false ) const;
        void changeMaximize( bool horizontal, bool vertical, bool adjust );
        void checkMaximizeGeometry();
        int checkFullScreenHack( const QRect& geom ) const; // 0 - none, 1 - one xinerama screen, 2 - full area
        void updateFullScreenHack( const QRect& geom );
        void getWmNormalHints();
        void getMotifHints();
        void getIcons();
        void getWmClientLeader();
        void getWmClientMachine();
        void fetchName();
        void fetchIconicName();
        QString readName() const;
        void setCaption( const QString& s, bool force = false );
        bool hasTransientInternal( const Client* c, bool indirect, ConstClientList& set ) const;
        void updateWindowRules();
        void finishWindowRules();
        void setShortcutInternal( const KShortcut& cut );

        void updateWorkareaDiffs();
        void checkDirection( int new_diff, int old_diff, QRect& rect, const QRect& area );
        static int computeWorkareaDiff( int left, int right, int a_left, int a_right );
        void configureRequest( int value_mask, int rx, int ry, int rw, int rh, int gravity, bool from_tool );
        NETExtendedStrut strut() const;
        int checkShadeGeometry( int w, int h );
        void postponeGeometryUpdates( bool postpone );

        bool startMoveResize();
        void finishMoveResize( bool cancel );
        void leaveMoveResize();
        void checkUnrestrictedMoveResize();
        void handleMoveResize( int x, int y, int x_root, int y_root );
        void positionGeometryTip();
        void grabButton( int mod );
        void ungrabButton( int mod );
        void resetMaximize();
        void resizeDecoration( const QSize& s );

        void pingWindow();
        void killProcess( bool ask, Time timestamp = CurrentTime );
        void updateUrgency();
        static void sendClientMessage( Window w, Atom a, Atom protocol,
            long data1 = 0, long data2 = 0, long data3 = 0 );

        void embedClient( Window w, const XWindowAttributes &attr );    
        void detectNoBorder();
        void destroyDecoration();
        void updateFrameExtents();

        void rawShow(); // just shows it
        void rawHide(); // just hides it

        Time readUserTimeMapTimestamp( const KStartupInfoId* asn_id, const KStartupInfoData* asn_data,
            bool session ) const;
        Time readUserCreationTime() const;
        static bool sameAppWindowRoleMatch( const Client* c1, const Client* c2, bool active_hack );
        void startupIdChanged();
        static bool hasShape( Window w );
        
        Window client;
        Window wrapper;
        KDecoration* decoration;
        Bridge* bridge;
        int desk;
        bool buttonDown;
        bool moveResizeMode;
        bool move_faked_activity;
        Window move_resize_grab_window;
        bool unrestrictedMoveResize;

        Position mode;
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
        void cleanGrouping();
        void checkGroupTransients();
        void setTransient( Window new_transient_for_id );
        Client* transient_for;
        Window transient_for_id;
        Window original_transient_for_id;
        ClientList transients_list; // SELI make this ordered in stacking order?
        ShadeMode shade_mode;
        uint active :1;
        uint deleting : 1; // true when doing cleanup and destroying the client
        uint keep_above : 1; // NET::KeepAbove (was stays_on_top)
        uint is_shape :1;
        uint skip_taskbar :1;
        uint original_skip_taskbar :1; // unaffected by KWin
        uint Pdeletewindow :1; // does the window understand the DeleteWindow protocol?
        uint Ptakefocus :1;// does the window understand the TakeFocus protocol?
        uint Ptakeactivity : 1; // does it support _NET_WM_TAKE_ACTIVITY
        uint Pcontexthelp : 1; // does the window understand the ContextHelp protocol?
        uint Pping : 1; // does it support _NET_WM_PING?
        uint input :1; // does the window want input in its wm_hints
        uint skip_pager : 1;
        uint motif_noborder : 1;
        uint motif_may_resize : 1;
        uint motif_may_move :1;
        uint motif_may_close : 1;
        uint keep_below : 1; // NET::KeepBelow
        uint minimized : 1;
        uint hidden : 1; // forcibly hidden by calling hide()
        uint modal : 1; // NET::Modal
        uint noborder : 1;
        uint user_noborder : 1;
        uint not_obscured : 1;
        uint urgency : 1; // XWMHints, UrgencyHint
        uint ignore_focus_stealing : 1; // don't apply focus stealing prevention to this client
        uint demands_attention : 1;
        WindowRules client_rules;
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
        MaximizeMode maxmode_restore;
        int workarea_diff_x, workarea_diff_y;
        WinInfo* info;
        QTimer* autoRaiseTimer;
        QTimer* shadeHoverTimer;
        Colormap cmap;
        QByteArray resource_name;
        QByteArray resource_class;
        QByteArray client_machine;
        QString cap_normal, cap_iconic, cap_suffix;
        WId wmClientLeaderWin;
        QByteArray window_role;
        Group* in_group;
        Window window_group;
        Layer in_layer;
        QTimer* ping_timer;
        KProcess* process_killer;
        Time ping_timestamp;
        Time user_time;
        unsigned long allowed_actions;
        QSize client_size;
        int postpone_geometry_updates; // >0 - new geometry is remembered, but not actually set
        bool pending_geometry_update;
        bool shade_geometry_change;
        int border_left, border_right, border_top, border_bottom;
        QRegion _mask;
        static bool check_active_modal; // see Client::checkActiveModal()
        KShortcut _shortcut;
        int sm_stacking_order;
        friend struct FetchNameInternalPredicate;
        friend struct CheckIgnoreFocusStealingProcedure;
        friend struct ResetupRulesProcedure;
        friend class GeometryUpdatesPostponer;
        void show() { assert( false ); } // SELI remove after Client is no longer QWidget
        void hide() { assert( false ); }
        QTimer* demandAttentionKNotifyTimer;
    };

// helper for Client::postponeGeometryUpdates() being called in pairs (true/false)
class GeometryUpdatesPostponer
    {
    public:
        GeometryUpdatesPostponer( Client* c )
            : cl( c ) { cl->postponeGeometryUpdates( true ); }
        ~GeometryUpdatesPostponer()
            { cl->postponeGeometryUpdates( false ); }
    private:
        Client* cl;
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
    return handle();
    }

inline Window Client::wrapperId() const
    {
    return wrapper;
    }

inline Window Client::decorationId() const
    {
    return decoration != NULL ? decoration->widget()->winId() : None;
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

// needed because verifyTransientFor() may set transient_for_id to root window,
// if the original value has a problem (window doesn't exist, etc.)
inline bool Client::wasOriginallyGroupTransient() const
    {
    return original_transient_for_id == workspace()->rootWin();
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

inline QByteArray Client::resourceName() const
    {
    return resource_name; // it is always lowercase
    }

inline QByteArray Client::resourceClass() const
    {
    return resource_class; // it is always lowercase
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
  This is always true for onAllDesktops clients.
 */
inline bool Client::isOnDesktop( int d ) const
    {
    return desk == d || /*desk == 0 ||*/ isOnAllDesktops();
    }

inline
bool Client::isShown( bool shaded_is_shown ) const
    {
    return !isMinimized() && ( !isShade() || shaded_is_shown ) && !hidden;
    }

inline
bool Client::isShade() const
    {
    return shade_mode == ShadeNormal;
    }

inline
ShadeMode Client::shadeMode() const
    {
    return shade_mode;
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

inline Client::MaximizeMode Client::maximizeModeRestore() const
    {
    return maxmode_restore;
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

inline bool Client::hasNETSupport() const
    {
    return info->hasNETSupport();
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

inline int Client::sessionStackingOrder() const
    {
    return sm_stacking_order;
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

inline QByteArray Client::windowRole() const
    {
    return window_role;
    }

inline QPoint Client::clientPos() const
    {
    return QPoint( border_left, border_top );
    }

inline QSize Client::clientSize() const
    {
    return client_size;
    }

inline void Client::setGeometry( const QRect& r, ForceGeometry_t force )
    {
    setGeometry( r.x(), r.y(), r.width(), r.height(), force );
    }

inline void Client::move( const QPoint & p, ForceGeometry_t force )
    {
    move( p.x(), p.y(), force );
    }

inline void Client::plainResize( const QSize& s, ForceGeometry_t force )
    {
    plainResize( s.width(), s.height(), force );
    }

inline void Client::resizeWithChecks( const QSize& s, ForceGeometry_t force )
    {
    resizeWithChecks( s.width(), s.height(), force );
    }

inline bool Client::hasUserTimeSupport() const
    {
    return info->userTime() != -1U;
    }
    
inline bool Client::ignoreFocusStealing() const
    {
    return ignore_focus_stealing;
    }

inline const WindowRules* Client::rules() const
    {
    return &client_rules;
    }

KWIN_PROCEDURE( CheckIgnoreFocusStealingProcedure, Client, cl->ignore_focus_stealing = options->checkIgnoreFocusStealing( cl ));

inline Window Client::moveResizeGrabWindow() const
    {
    return move_resize_grab_window;
    }

inline KShortcut Client::shortcut() const
    {
    return _shortcut;
    }

inline void Client::removeRule( Rules* rule )
    {
    client_rules.remove( rule );
    }

KWIN_COMPARE_PREDICATE( WindowMatchPredicate, Client, Window, cl->window() == value );
KWIN_COMPARE_PREDICATE( FrameIdMatchPredicate, Client, Window, cl->frameId() == value );
KWIN_COMPARE_PREDICATE( WrapperIdMatchPredicate, Client, Window, cl->wrapperId() == value );

} // namespace

#endif
