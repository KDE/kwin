#include "main.h"
#include "options.h"
#include "atoms.h"
#include "workspace.h"
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <signal.h>
#define INT8 _X11INT8
#define INT32 _X11INT32
#include <X11/Xproto.h>
#undef INT8
#undef INT32

#define i18n(x) (x)

Options* options;
Atoms* atoms;

static bool initting = FALSE;
int x11ErrorHandler(Display *d, XErrorEvent *e){
    char msg[80], req[80], number[80];
    bool ignore_badwindow = FALSE; //maybe temporary

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

Application::Application( int &argc, char *argv[] )
: KApplication( argc, argv, "kwin" )
{
    initting = TRUE;
    options = new Options;
    atoms = new Atoms;

    // install X11 error handler
    XSetErrorHandler( x11ErrorHandler );

    // create a workspace.
    workspaces += new Workspace();
    initting = FALSE;
    if ( argc > 1 ) {
	QString s = argv[1];
	int i = s.toInt();
	workspaces += new Workspace( (WId ) i );
    }

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

bool Application::x11EventFilter( XEvent *e )
{
     for ( WorkspaceList::Iterator it = workspaces.begin(); it != workspaces.end(); ++it) {
	 if ( (*it)->workspaceEvent( e ) )
	     return TRUE;
     }
     return KApplication::x11EventFilter( e );
}



static void sighandler(int) {
    QApplication::exit();
}

int main( int argc, char * argv[] ) {

    if (signal(SIGTERM, sighandler) == SIG_IGN)
	signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, sighandler) == SIG_IGN)
	signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, sighandler) == SIG_IGN)
	signal(SIGHUP, SIG_IGN);

    Application a( argc, argv );
    fcntl(ConnectionNumber(qt_xdisplay()), F_SETFD, 1);

    return a.exec();
}
