#include "main.h"
#include "options.h"
#include "atoms.h"
#include "workspace.h"
#include <dcopclient.h>
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

static const char *version = "0.9";

static const char *description = I18N_NOOP( "KDE Window Manager." );

static KCmdLineOptions cmdOptions[]  =
{
  { "+[workspace]", I18N_NOOP("A cryptic command line option"), 0 },
  { 0, 0, 0 }
};

Options* options;
Atoms* atoms;

Time kwin_time = CurrentTime;

static bool initting = FALSE;
int x11ErrorHandler(Display *d, XErrorEvent *e){
    char msg[80], req[80], number[80];
    bool ignore_badwindow = TRUE; //maybe temporary

    if (initting &&
	(
	 e->request_code == X_ChangeWindowAttributes
	 || e->request_code == X_GrabKey
	 )
	&& (e->error_code == BadAccess)) {
	fprintf(stderr, i18n("kwin: it looks like there's already a window manager running.  kwin not started\n"));
	exit(1);
    }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
	return 0;

    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    sprintf(number, "%d", e->request_code);
    XGetErrorDatabaseText(d, "XRequest", number, "<unknown>", req, sizeof(req));

    fprintf(stderr, "kwin: %s(0x%lx): %s\n", req, e->resourceid, msg);

    if (initting) {
        fprintf(stderr, i18n("kwin: failure during initialisation; aborting\n"));
        exit(1);
    }
    return 0;
}

Application::Application( )
: KApplication( )
{
    initting = TRUE;
    options = new Options;
    atoms = new Atoms;

    // install X11 error handler
    XSetErrorHandler( x11ErrorHandler );

    // create a workspace.
    workspaces += new Workspace();
    initting = FALSE;

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if ( args->count() ) {
	QString s = args->arg(0);
	int i = s.toInt();
	workspaces += new Workspace( (WId ) i );
    }
    args->clear();

    syncX();
    initting = FALSE;
}


Application::~Application()
{
     for ( WorkspaceList::Iterator it = workspaces.begin(); it != workspaces.end(); ++it) {
	 delete (*it);
     }
     delete options;
}

void kwiniface::logout()
{
exit (0);
}

bool kwiniface::process(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData)
{	
	fprintf(stderr,"Logout Call Recieved\n");
	if ( fun == "logout()" )
	{
		replyType = "void";
		logout();
		return TRUE;
	}
     	else 
	{
	
	return FALSE;
 	}
}



bool Application::x11EventFilter( XEvent *e )
{
    switch ( e->type ) {
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	kwin_time = (e->type == MotionNotify) ?
		    e->xmotion.time : e->xbutton.time;
	break;
    case KeyPress:
    case KeyRelease:
	kwin_time = e->xkey.time;
	break;
    case PropertyNotify:
	kwin_time = e->xproperty.time;
	break;
    default:
	break;
    }

     for ( WorkspaceList::Iterator it = workspaces.begin(); it != workspaces.end(); ++it) {
	 if ( (*it)->workspaceEvent( e ) )
	     return TRUE;
     }
     return KApplication::x11EventFilter( e );
}



static void sighandler(int) {
    QApplication::exit();
}

int main( int argc, char * argv[] ) 
{
    KAboutData aboutData( "kwin", I18N_NOOP("KWin"), version, description);

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions( cmdOptions );

    if (signal(SIGTERM, sighandler) == SIG_IGN)
	signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, sighandler) == SIG_IGN)
	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, sighandler) == SIG_IGN)
	signal(SIGHUP, SIG_IGN);

    Application a;
    fcntl(ConnectionNumber(qt_xdisplay()), F_SETFD, 1);

    DCOPClient *client = kapp->dcopClient();
    client->attach();
    client->registerAs("kwin");


     return a.exec();

}
