/*****************************************************************
kwin - the KDE window manager

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
******************************************************************/

// X11/Qt conflict
#undef Bool

//#define QT_CLEAN_NAMESPACE
#include <kconfig.h>
#include <ksimpleconfig.h>
#include "main.h"
#include "options.h"
#include "atoms.h"
#include "workspace.h"
#include <dcopclient.h>
#include <qsessionmanager.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#define INT8 _X11INT8
#define INT32 _X11INT32
#include <X11/Xproto.h>
#undef INT8
#undef INT32

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kcrash.h>

using namespace KWinInternal;

namespace KWinInternal {
Options* options;
};

Atoms* atoms;

extern Time qt_x_time;
int kwin_screen_number = -1;

static bool initting = FALSE;
static DCOPClient * client = 0;

/*static void crashHandler(int)
{
   KCrash::setCrashHandler(0); // Exit on next crash.
   client->detach();  // Unregister with dcop.
   system("kwin&"); // Try to restart
}*/

int x11ErrorHandler(Display *d, XErrorEvent *e){
    char msg[80], req[80], number[80];
    bool ignore_badwindow = TRUE; //maybe temporary

    if (initting &&
	(
	 e->request_code == X_ChangeWindowAttributes
	 || e->request_code == X_GrabKey
	 )
	&& (e->error_code == BadAccess)) {
	fputs(i18n("kwin: it looks like there's already a window manager running. kwin not started.\n").local8Bit(), stderr);
	exit(1);
    }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
	return 0;

    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    sprintf(number, "%d", e->request_code);
    XGetErrorDatabaseText(d, "XRequest", number, "<unknown>", req, sizeof(req));

    fprintf(stderr, "kwin: %s(0x%lx): %s\n", req, e->resourceid, msg);

    if (initting) {
        fputs(i18n("kwin: failure during initialization; aborting").local8Bit(), stderr);
        exit(1);
    }
    return 0;
}

/*!
  Updates qt_x_time by receiving a current timestamp from the server.

  Use this function only when really necessary. Keep in mind that it's
  a roundtrip to the X-Server.
 */
void kwin_updateTime()
{
    static QWidget* w = 0;
    if ( !w )
	w = new QWidget;
    long data = 1;
    XChangeProperty(qt_xdisplay(), w->winId(), atoms->kwin_running, atoms->kwin_running, 32,
		    PropModeAppend, (unsigned char*) &data, 1);
    XEvent ev;
    XWindowEvent( qt_xdisplay(), w->winId(), PropertyChangeMask, &ev );
    qt_x_time = ev.xproperty.time;
}


Application::Application( )
: KApplication( ), owner( kwin_screen_number )
{
    if (kwin_screen_number == -1)
        kwin_screen_number = DefaultScreen(qt_xdisplay());

    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    if( !owner.claim( args->isSet( "replace" ), true ))
    {
        fputs(i18n("kwin: couldn't claim manager selection, another wm running? (try using --replace)\n").local8Bit(), stderr);
        ::exit(1);
    }
    connect( &owner, SIGNAL( lostOwnership()), SLOT( quit()));
	
    initting = TRUE; // startup....

    // install X11 error handler
    XSetErrorHandler( x11ErrorHandler );

    // check  whether another windowmanager is running
    XSelectInput(qt_xdisplay(), qt_xrootwin(), SubstructureRedirectMask  );
    syncX(); // trigger error now

    options = new Options;
    atoms = new Atoms;

    // create workspace.
    Workspace* ws = new Workspace( isSessionRestored() );

    syncX(); // trigger possible errors, there's still a chance to abort
    
#ifdef XRANDR_SUPPORT
    connect( desktop(), SIGNAL( resized( int )), ws, SLOT( desktopResized()));
#endif

    initting = FALSE; // startup done, we are up and running now.
    dcopClient()->send( "ksplash", "", "upAndRunning(QString)", QString("wm started"));
    
#ifndef NO_LEGACY_SESSION_MANAGEMENT
    if ( isSessionRestored() )
	ws->restoreLegacySession( kapp->sessionConfig() );
#else
    ws = ws; // shut up
#endif
}


Application::~Application()
{
     delete Workspace::self();
     delete options;
}



bool Application::x11EventFilter( XEvent *e )
{
    if ( Workspace::self()->workspaceEvent( e ) )
	     return TRUE;
    bool handled = false;
    switch( e->type )
    {
	case SelectionClear:
	    handled = owner.filterSelectionClear( e->xselectionclear );
	  break;
	case SelectionNotify:
	    handled = owner.filterSelectionNotify( e->xselection );
	  break;
	case SelectionRequest:
	    handled = owner.filterSelectionRequest( e->xselectionrequest );
	  break;
	case DestroyNotify:
	    handled = owner.filterDestroyNotify( e->xdestroywindow );
	  break;
	default:
	  break;
    }
    if( handled )
	return true;
    return KApplication::x11EventFilter( e );
}

class SessionManaged : public KSessionManaged
{
    public:
	bool saveState( QSessionManager& sm ) {
	    sm.release();
	    if ( !sm.isPhase2() ) {
		sm.requestPhase2();
		return true;
	    }
	    Workspace::self()->storeSession( kapp->sessionConfig() );
	    kapp->sessionConfig()->sync();
	    return true;
	}
} ;

static void sighandler(int) {
    QApplication::exit();
}

static const char *version = "0.95";
static const char *description = I18N_NOOP( "The KDE window manager." );

extern "C" { int kdemain(int, char *[]); }

static KCmdLineOptions args[] =
{
    { "replace", I18N_NOOP("Replace already running ICCCM2.0 compliant window manager."), 0 },
    KCmdLineLastOption
};

int kdemain( int argc, char * argv[] )
{
    bool restored = false;
    for (int arg = 1; arg < argc; arg++) {
        if (! qstrcmp(argv[arg], "-session")) {
            restored = true;
            break;
        }
    }

    if (! restored) {
        // we only do the multihead fork if we are not restored by the session
	// manager, since the session manager will register multiple kwins,
        // one for each screen...
        QCString multiHead = getenv("KDE_MULTIHEAD");
        if (multiHead.lower() == "true") {

	    Display* dpy = XOpenDisplay( NULL );
	    if ( !dpy ) {
		fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
			argv[0], XDisplayName(NULL ) );
		exit (1);
	    }

	    int number_of_screens = ScreenCount( dpy );
	    kwin_screen_number = DefaultScreen( dpy );
	    int pos; // temporarily needed to reconstruct DISPLAY var if multi-head
	    QCString display_name = XDisplayString( dpy );
	    XCloseDisplay( dpy );
	    dpy = 0;

	    if ((pos = display_name.findRev('.')) != -1 )
		display_name.remove(pos,10); // 10 is enough to be sure we removed ".s"

	    QCString envir;
	    if (number_of_screens != 1) {
		for (int i = 0; i < number_of_screens; i++ ) {
		    // if execution doesn't pass by here, then kwin
		    // acts exactly as previously
		    if ( i != kwin_screen_number && fork() == 0 ) {
			kwin_screen_number = i;
			// break here because we are the child process, we don't
			// want to fork() anymore
			break;
		    }
		}
		// in the next statement, display_name shouldn't contain a screen
		//   number. If it had it, it was removed at the "pos" check
		envir.sprintf("DISPLAY=%s.%d", display_name.data(), kwin_screen_number);

		if (putenv( strdup(envir.data())) ) {
		    fprintf(stderr,
			    "%s: WARNING: unable to set DISPLAY environment variable\n",
			    argv[0]);
		    perror("putenv()");
		}
	    }
	}
    }

    KAboutData aboutData( "kwin", I18N_NOOP("KWin"),
			  version, description, KAboutData::License_BSD,
			  I18N_NOOP("(c) 1999-2002, The KDE Developers"));
    aboutData.addAuthor("Matthias Ettrich",0, "ettrich@kde.org");
    aboutData.addAuthor("Cristian Tibirna",0, "tibirna@kde.org");
    aboutData.addAuthor("Daniel M. Duley",0, "mosfet@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions( args );

    if (signal(SIGTERM, sighandler) == SIG_IGN)
	signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, sighandler) == SIG_IGN)
	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, sighandler) == SIG_IGN)
	signal(SIGHUP, SIG_IGN);

    Application a;
    SessionManaged weAreIndeed;
//     KCrash::setCrashHandler(crashHandler); // Try to restart on crash
    fcntl(ConnectionNumber(qt_xdisplay()), F_SETFD, 1);

    QCString appname;
    if (kwin_screen_number == 0)
	appname = "kwin";
    else
	appname.sprintf("kwin-screen-%d", kwin_screen_number);

    client = a.dcopClient();
    client->attach();
    client->registerAs( appname.data(), false);
    client->setDefaultObject( "KWinInterface" );

    return a.exec();
}

//************************************
// KWinSelectionOwner
//************************************

KWinSelectionOwner::KWinSelectionOwner( int screen_P )
    : KSelectionOwner( make_selection_atom( screen_P ), screen_P )
    {
    }

Atom KWinSelectionOwner::make_selection_atom( int screen_P )
    {
    if( screen_P < 0 )
	screen_P = DefaultScreen( qt_xdisplay());
    char tmp[ 30 ];
    sprintf( tmp, "WM_S%d", screen_P );
    return XInternAtom( qt_xdisplay(), tmp, False );
    }
    
void KWinSelectionOwner::getAtoms()
    {
    KSelectionOwner::getAtoms();
    if( xa_version == None )
        {
        Atom atoms[ 1 ];
        const char* const names[] =
            { "VERSION" };
        XInternAtoms( qt_xdisplay(), const_cast< char** >( names ), 1, False, atoms );
        xa_version = atoms[ 0 ];
        }
    }

void KWinSelectionOwner::replyTargets( Atom property_P, Window requestor_P )
    {
    KSelectionOwner::replyTargets( property_P, requestor_P );
    Atom atoms[ 1 ] = { xa_version };
    // PropModeAppend !
    XChangeProperty( qt_xdisplay(), requestor_P, property_P, XA_ATOM, 32, PropModeAppend,
        reinterpret_cast< unsigned char* >( atoms ), 1 );
    }

bool KWinSelectionOwner::genericReply( Atom target_P, Atom property_P, Window requestor_P )
    {
    if( target_P == xa_version )
        {
        Q_INT32 version[] = { 2, 0 };
        XChangeProperty( qt_xdisplay(), requestor_P, property_P, XA_INTEGER, 32,
            PropModeReplace, reinterpret_cast< unsigned char* >( &version ), 2 );
        }
    else
        return KSelectionOwner::genericReply( target_P, property_P, requestor_P );
    return true;    
    }

Atom KWinSelectionOwner::xa_version = None;

#include "main.moc"
