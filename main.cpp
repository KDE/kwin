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

extern Time qt_x_time; // workaround for Qt < 2.3.2
Time kwin_time = CurrentTime;
int kwin_screen_number = -1;

static bool initting = FALSE;
static DCOPClient * client = 0;

static void crashHandler(int)
{
   KCrash::setCrashHandler(0); // Exit on next crash.
   client->detach();  // Unregister with dcop.
   system("kwin&"); // Try to restart
}

int x11ErrorHandler(Display *d, XErrorEvent *e){
    char msg[80], req[80], number[80];
    bool ignore_badwindow = TRUE; //maybe temporary

    if (initting &&
	(
	 e->request_code == X_ChangeWindowAttributes
	 || e->request_code == X_GrabKey
	 )
	&& (e->error_code == BadAccess)) {
	fprintf(stderr, i18n("kwin: it looks like there's already a window manager running. kwin not started.\n").local8Bit());
	exit(1);
    }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
	return 0;

    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    sprintf(number, "%d", e->request_code);
    XGetErrorDatabaseText(d, "XRequest", number, "<unknown>", req, sizeof(req));

    fprintf(stderr, "kwin: %s(0x%lx): %s\n", req, e->resourceid, msg);

    if (initting) {
        fprintf(stderr, i18n("kwin: failure during initialization; aborting").local8Bit());
        exit(1);
    }
    return 0;
}

/*!
  Updates kwin_time by receiving a current timestamp from the server.

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
    kwin_time = ev.xproperty.time;
    qt_x_time = kwin_time;
}


Application::Application( )
: KApplication( )
{
    if (kwin_screen_number == -1)
        kwin_screen_number = DefaultScreen(qt_xdisplay());

    initting = TRUE; // startup....

    // install X11 error handler
    XSetErrorHandler( x11ErrorHandler );

    // check  whether another windowmanager is running
    XSelectInput(qt_xdisplay(), qt_xrootwin(), SubstructureRedirectMask  );
    syncX(); // trigger error now

    options = new Options;
    atoms = new Atoms;

    // create workspace.
    (void) new Workspace( isSessionRestored() );

    syncX(); // trigger possible errors, there's still a chance to abort

    initting = FALSE; // startup done, we are up and running now.
    dcopClient()->send( "ksplash", "", "upAndRunning(QString)", QString("wm started"));
}


Application::~Application()
{
     delete Workspace::self();
     delete options;
}


bool Application::x11EventFilter( XEvent *e )
{
    switch ( e->type ) {
    case ButtonPress:
    case ButtonRelease:
	kwin_time = e->xbutton.time;
	qt_x_time = kwin_time; // workaround for Qt < 2.3.2
	break;
    case MotionNotify:
	kwin_time = e->xmotion.time;
	qt_x_time = kwin_time; // workaround for Qt < 2.3.2
	break;
    case KeyPress:
    case KeyRelease:
	kwin_time = e->xkey.time;
	qt_x_time = kwin_time; // workaround for Qt < 2.3.2
	break;
    case PropertyNotify:
	kwin_time = e->xproperty.time;
	qt_x_time = kwin_time; // workaround for Qt < 2.3.2
	break;
    case EnterNotify:
    case LeaveNotify:
	kwin_time = e->xcrossing.time;
	qt_x_time = kwin_time; // workaround for Qt < 2.3.2
    default:
	break;
    }

     if ( Workspace::self()->workspaceEvent( e ) )
	     return TRUE;
     return KApplication::x11EventFilter( e );
}

void Application::commitData( QSessionManager& /*sm*/ )
{
    // nothing to do, really
}

void Application::saveState( QSessionManager& sm )
{
    KApplication::saveState( sm );
    static bool firstTime = true;
    if ( firstTime ) {
	firstTime = false;
	return; // no need to save this state.
    }

    sm.release();

    if ( !sm.isPhase2() ) {
	sm.requestPhase2();
	return;
    }

    Workspace::self()->storeSession( kapp->sessionConfig() );
    kapp->sessionConfig()->sync();
}


static void sighandler(int) {
    QApplication::exit();
}

static const char *version = "0.9";
static const char *description = I18N_NOOP( "The KDE window manager." );

extern "C" { int kdemain(int, char *[]); }

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
			  "(c) 1999-2000, The KDE Developers");
    aboutData.addAuthor("Matthias Ettrich",0, "ettrich@kde.org");
    aboutData.addAuthor("Daniel M. Duley",0, "mosfet@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    if (signal(SIGTERM, sighandler) == SIG_IGN)
	signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, sighandler) == SIG_IGN)
	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, sighandler) == SIG_IGN)
	signal(SIGHUP, SIG_IGN);

    Application a;
    KCrash::setCrashHandler(crashHandler); // Try to restart on crash
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
