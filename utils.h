/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_UTILS_H
#define KWIN_UTILS_H

#include <config.h>
#include <config-X11.h>
#include <config-kwin.h>

#include <X11/Xlib.h>

#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xcomposite.h>
#if XCOMPOSITE_MAJOR > 0 || XCOMPOSITE_MINOR >= 3
#define HAVE_XCOMPOSITE_OVERLAY
#endif
#endif

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <fixx11h.h>

#include <QWidget>
#include <kmanagerselection.h>
#include <netwm_def.h>
//#include <kshortcutdialog.h> //you really want to use KKeySequenceWidget
#include <limits.h>
#include <QX11Info>

#include <kwinglobals.h>


namespace KWin
{

#ifndef HAVE_XDAMAGE
typedef long Damage;
struct XDamageNotifyEvent
    {
    };
#endif

const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
    | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
    | NET::UtilityMask | NET::SplashMask | NET::DropdownMenuMask | NET::PopupMenuMask
    | NET::TooltipMask | NET::NotificationMask | NET::ComboBoxMask | NET::DNDIconMask;

const long ClientWinMask = KeyPressMask | KeyReleaseMask |
                          ButtonPressMask | ButtonReleaseMask |
                  KeymapStateMask |
                  ButtonMotionMask |
                  PointerMotionMask | // need this, too!
                  EnterWindowMask | LeaveWindowMask |
                  FocusChangeMask |
                  ExposureMask |
                  StructureNotifyMask |
                  SubstructureRedirectMask;

const QPoint invalidPoint( INT_MIN, INT_MIN );

class Toplevel;
class Client;
class Unmanaged;
class Deleted;
class Group;
class Options;

typedef QList< Toplevel* > ToplevelList;
typedef QList< const Toplevel* > ConstToplevelList;
typedef QList< Client* > ClientList;
typedef QList< const Client* > ConstClientList;
typedef QList< Unmanaged* > UnmanagedList;
typedef QList< const Unmanaged* > ConstUnmanagedList;
typedef QList< Deleted* > DeletedList;
typedef QList< const Deleted* > ConstDeletedList;

typedef QList< Group* > GroupList;
typedef QList< const Group* > ConstGroupList;

extern Options* options;

enum Layer
    {
    UnknownLayer = -1,
    FirstLayer = 0,
    DesktopLayer = FirstLayer,
    BelowLayer,
    NormalLayer,
    DockLayer,
    AboveLayer,
    ActiveLayer, // active fullscreen, or active dialog
    NumLayers // number of layers, must be last
    };

// yes, I know this is not 100% like standard operator++
inline void operator++( Layer& lay )
    {
    lay = static_cast< Layer >( lay + 1 );
    }

// for Client::takeActivity()
enum ActivityFlags
    {
    ActivityFocus = 1 << 0, // focus the window
    ActivityFocusForce = 1 << 1, // focus even if Dock etc.
    ActivityRaise = 1 << 2 // raise the window
    };

// Some KWin classes, mainly Client and Workspace, are very tighly coupled,
// and some of the methods of one class may be called only from speficic places.
// Those methods have additional allowed_t argument. If you pass Allowed
// as an argument to any function, make sure you really know what you're doing.
enum allowed_t { Allowed };

// some enums to have more readable code, instead of using bools
enum ForceGeometry_t { NormalGeometrySet, ForceGeometrySet };



enum ShadeMode
    {
    ShadeNone, // not shaded
    ShadeNormal, // normally shaded - isShade() is true only here
    ShadeHover, // "shaded", but visible due to hover unshade
    ShadeActivated // "shaded", but visible due to alt+tab to the window
    };

class Extensions
    {
    public:
        static void init();
        static bool shapeAvailable() { return has_shape; }
        static int shapeNotifyEvent();
        static bool randrAvailable() { return has_randr; }
        static int randrNotifyEvent();
        static bool damageAvailable() { return has_damage; }
        static int damageNotifyEvent();
        static bool compositeAvailable() { return has_composite; }
        static bool compositeOverlayAvailable() { return has_composite && has_composite_overlay; }
        static bool fixesAvailable() { return has_fixes; }
        static bool hasShape( Window w );
    private:
        static bool has_shape;
        static int shape_event_base;
        static bool has_randr;
        static int randr_event_base;
        static bool has_damage;
        static int damage_event_base;
        static bool has_composite;
        static bool has_composite_overlay;
        static bool has_fixes;
    };

class Motif 
    {
    public:
        static void readFlags( WId w, bool& noborder, bool& resize, bool& move,
            bool& minimize, bool& maximize, bool& close );
        struct MwmHints 
            {
            ulong flags;
            ulong functions;
            ulong decorations;
            long input_mode;
            ulong status;
            };
        enum {
            MWM_HINTS_FUNCTIONS = (1L << 0),
            MWM_HINTS_DECORATIONS =  (1L << 1),

            MWM_FUNC_ALL = (1L << 0),
            MWM_FUNC_RESIZE = (1L << 1),
            MWM_FUNC_MOVE = (1L << 2),
            MWM_FUNC_MINIMIZE = (1L << 3),
            MWM_FUNC_MAXIMIZE = (1L << 4),
            MWM_FUNC_CLOSE = (1L << 5)
            };
    };

class KWinSelectionOwner
    : public KSelectionOwner
    {
    Q_OBJECT
    public:
        KWinSelectionOwner( int screen );
    protected:
        virtual bool genericReply( Atom target, Atom property, Window requestor );
        virtual void replyTargets( Atom property, Window requestor );
        virtual void getAtoms();
    private:
        Atom make_selection_atom( int screen );
        static Atom xa_version;
    };

// Class which saves original value of the variable, assigns the new value
// to it, and in the destructor restores the value.
// Used in Client::isMaximizable() and so on.
// It also casts away contness and generally this looks like a hack.
template< typename T >
class TemporaryAssign
    {
    public:
        TemporaryAssign( const T& var, const T& value )
            : variable( var ), orig( var )
            {
            const_cast< T& >( variable ) = value;
            }
        ~TemporaryAssign()
            {
            const_cast< T& >( variable ) = orig;
            }
    private:
        const T& variable;
        T orig;
    };

QByteArray getStringProperty(WId w, Atom prop, char separator=0);
void updateXTime();
void grabXServer();
void ungrabXServer();
bool grabbedXServer();

class Scene;
extern Scene* scene;
inline bool compositing() { return scene != NULL; }

// the docs say it's UrgencyHint, but it's often #defined as XUrgencyHint
#ifndef UrgencyHint
#define UrgencyHint XUrgencyHint
#endif

// for STL-like algo's
#define KWIN_CHECK_PREDICATE( name, cls, check ) \
struct name \
    { \
    inline bool operator()( const cls* cl ) { return check; }; \
    }

#define KWIN_COMPARE_PREDICATE( name, cls, type, check ) \
struct name \
    { \
    typedef type type_helper; /* in order to work also with type being 'const Client*' etc. */ \
    inline name( const type_helper& compare_value ) : value( compare_value ) {}; \
    inline bool operator()( const cls* cl ) { return check; }; \
    const type_helper& value; \
    }

#define KWIN_PROCEDURE( name, cls, action ) \
struct name \
    { \
    inline void operator()( cls* cl ) { action; }; \
    }

KWIN_CHECK_PREDICATE( TruePredicate, Client, cl == cl /*true, avoid warning about 'cl' */ );

template< typename T >
Client* findClientInList( const ClientList& list, T predicate )
    {
    for ( ClientList::ConstIterator it = list.begin(); it != list.end(); ++it) 
        {
        if ( predicate( const_cast< const Client* >( *it)))
            return *it;
        }
    return NULL;
    }

template< typename T >
Unmanaged* findUnmanagedInList( const UnmanagedList& list, T predicate )
    {
    for ( UnmanagedList::ConstIterator it = list.begin(); it != list.end(); ++it) 
        {
        if ( predicate( const_cast< const Unmanaged* >( *it)))
            return *it;
        }
    return NULL;
    }

inline
int timestampCompare( Time time1, Time time2 ) // like strcmp()
    {
    return NET::timestampCompare( time1, time2 );
    }

inline
Time timestampDiff( Time time1, Time time2 ) // returns time2 - time1
    {
    return NET::timestampDiff( time1, time2 );
    }

bool isLocalMachine( const QByteArray& host );

QPoint cursorPos();

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states
int qtToX11Button( Qt::MouseButton button );
Qt::MouseButton x11ToQtMouseButton( int button );
int qtToX11State( Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );
Qt::MouseButtons x11ToQtMouseButtons( int state );
Qt::KeyboardModifiers x11ToQtKeyboardModifiers( int state );

#ifndef KCMRULES
// Qt dialogs emit no signal when closed :(

// KShortcutDialog is gone, and K(Shortcut/KeySequence)Widget should replace it and be
// good enough to not need any further hacks here. If they aren't, CONTACT ME before
// you add hacks again. --ahartmetz
#if 0
class ShortcutDialog
    : public KShortcutDialog
    {
    Q_OBJECT
    public:
        ShortcutDialog( const KShortcut& cut );
        virtual void accept();
    signals:
        void dialogDone( bool ok );
    protected:
        virtual void done( int r ) { KShortcutDialog::done( r ); emit dialogDone( r == Accepted ); }
    };
#endif //0
#endif //KCMRULES

} // namespace

#endif
