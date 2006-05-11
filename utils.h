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

#include <QWidget>
#include <kmanagerselection.h>
#include <netwm_def.h>
#include <kshortcutdialog.h>
#include <limits.h>

namespace KWinInternal
{

const int SUPPORTED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
    | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
    | NET::UtilityMask | NET::SplashMask;

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

class Client;
class Group;
class Options;

typedef QList< Client* > ClientList;
typedef QList< const Client* > ConstClientList;

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

// Areas, mostly related to Xinerama
enum clientAreaOption
    {
    PlacementArea,         // geometry where a window will be initially placed after being mapped
    MovementArea,          // ???  window movement snapping area?  ignore struts
    MaximizeArea,          // geometry to which a window will be maximized
    MaximizeFullArea,      // like MaximizeArea, but ignore struts - used e.g. for topmenu
    FullScreenArea,        // area for fullscreen windows
    // these below don't depend on xinerama settings
    WorkArea,              // whole workarea (all screens together)
    FullArea,              // whole area (all screens together), ignore struts
    ScreenArea             // one whole screen, ignore struts
    };

enum ShadeMode
    {
    ShadeNone, // not shaded
    ShadeNormal, // normally shaded - isShade() is true only here
    ShadeHover, // "shaded", but visible due to hover unshade
    ShadeActivated // "shaded", but visible due to alt+tab to the window
    };

class Shape 
    {
    public:
        static bool available() { return kwin_has_shape; }
        static bool hasShape( WId w);
        static int shapeEvent();
        static void init();
    private:
        static int kwin_has_shape;
        static int kwin_shape_event;
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

// the docs say it's UrgencyHint, but it's often #defined as XUrgencyHint
#ifndef UrgencyHint
#define UrgencyHint XUrgencyHint
#endif

// for STL-like algo's
#define KWIN_CHECK_PREDICATE( name, check ) \
struct name \
    { \
    inline bool operator()( const Client* cl ) { return check; }; \
    }

#define KWIN_COMPARE_PREDICATE( name, type, check ) \
struct name \
    { \
    typedef type type_helper; /* in order to work also with type being 'const Client*' etc. */ \
    inline name( const type_helper& compare_value ) : value( compare_value ) {}; \
    inline bool operator()( const Client* cl ) { return check; }; \
    const type_helper& value; \
    }

#define KWIN_PROCEDURE( name, action ) \
struct name \
    { \
    inline void operator()( Client* cl ) { action; }; \
    }

KWIN_CHECK_PREDICATE( TruePredicate, cl == cl /*true, avoid warning about 'cl' */ );

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

#ifndef KCMRULES
// Qt dialogs emit no signal when closed :(
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
#endif

} // namespace

#endif
