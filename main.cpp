/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

//#define QT_CLEAN_NAMESPACE
#include <kconfig.h>

#include "main.h"

#include <klocale.h>
#include <stdlib.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "atoms.h"
#include "options.h"
#include "sm.h"

#define INT8 _X11INT8
#define INT32 _X11INT32
#include <X11/Xproto.h>
#undef INT8
#undef INT32

extern Time qt_x_time;

namespace KWinInternal
{

Options* options;

Atoms* atoms;

int screen_number = -1;

static bool initting = FALSE;

static
int x11ErrorHandler(Display *d, XErrorEvent *e)
    {
    char msg[80], req[80], number[80];
    bool ignore_badwindow = TRUE; //maybe temporary

    if (initting &&
        (
         e->request_code == X_ChangeWindowAttributes
         || e->request_code == X_GrabKey
         )
        && (e->error_code == BadAccess)) 
        {
        fputs(i18n("kwin: it looks like there's already a window manager running. kwin not started.\n").local8Bit(), stderr);
        exit(1);
        }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
        return 0;

    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    sprintf(number, "%d", e->request_code);
    XGetErrorDatabaseText(d, "XRequest", number, "<unknown>", req, sizeof(req));

    fprintf(stderr, "kwin: %s(0x%lx): %s\n", req, e->resourceid, msg);

    if (initting) 
        {
        fputs(i18n("kwin: failure during initialization; aborting").local8Bit(), stderr);
        exit(1);
        }
    return 0;
    }

Application::Application( )
: KApplication( ), owner( screen_number )
    {
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    if (!config()->isImmutable() && args->isSet("lock"))
        {
        config()->setReadOnly(true);
        config()->reparseConfiguration();
        }

    if (screen_number == -1)
        screen_number = DefaultScreen(qt_xdisplay());

    if( !owner.claim( args->isSet( "replace" ), true ))
        {
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").local8Bit(), stderr);
        ::exit(1);
        }
    connect( &owner, SIGNAL( lostOwnership()), SLOT( lostSelection()));
    
    // if there was already kwin running, it saved its configuration after loosing the selection -> reread
    config()->reparseConfiguration();

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
    if( owner.ownerWindow() != None ) // if there was no --replace (no new WM)
        XSetInputFocus( qt_xdisplay(), PointerRoot, RevertToPointerRoot, qt_x_time );
    delete options;
    }

void Application::lostSelection()
    {
    delete Workspace::self();
    // remove windowmanager privileges
    XSelectInput(qt_xdisplay(), qt_xrootwin(), PropertyChangeMask );
    quit();
    }

bool Application::x11EventFilter( XEvent *e )
    {
    if ( Workspace::self()->workspaceEvent( e ) )
             return TRUE;
    return KApplication::x11EventFilter( e );
    }

static void sighandler(int) 
    {
    QApplication::exit();
    }


} // namespace

static const char version[] = "2.95";
static const char description[] = I18N_NOOP( "The KDE window manager." );

static KCmdLineOptions args[] =
    {
        { "lock", I18N_NOOP("Disable configuration options"), 0 },
        { "replace", I18N_NOOP("Replace already-running ICCCM2.0-compliant window manager"), 0 },
        KCmdLineLastOption
    };

extern "C"
int kdemain( int argc, char * argv[] )
    {
    bool restored = false;
    for (int arg = 1; arg < argc; arg++) 
        {
        if (! qstrcmp(argv[arg], "-session")) 
            {
            restored = true;
            break;
            }
        }

    if (! restored) 
        {
        // we only do the multihead fork if we are not restored by the session
	// manager, since the session manager will register multiple kwins,
        // one for each screen...
        QCString multiHead = getenv("KDE_MULTIHEAD");
        if (multiHead.lower() == "true") 
            {

            Display* dpy = XOpenDisplay( NULL );
            if ( !dpy ) 
                {
                fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
                        argv[0], XDisplayName(NULL ) );
                exit (1);
                }

            int number_of_screens = ScreenCount( dpy );
            KWinInternal::screen_number = DefaultScreen( dpy );
            int pos; // temporarily needed to reconstruct DISPLAY var if multi-head
            QCString display_name = XDisplayString( dpy );
            XCloseDisplay( dpy );
            dpy = 0;

            if ((pos = display_name.findRev('.')) != -1 )
                display_name.remove(pos,10); // 10 is enough to be sure we removed ".s"

            QCString envir;
            if (number_of_screens != 1) 
                {
                for (int i = 0; i < number_of_screens; i++ ) 
                    {
		    // if execution doesn't pass by here, then kwin
		    // acts exactly as previously
                    if ( i != KWinInternal::screen_number && fork() == 0 ) 
                        {
                        KWinInternal::screen_number = i;
			// break here because we are the child process, we don't
			// want to fork() anymore
                        break;
                        }
                    }
		// in the next statement, display_name shouldn't contain a screen
		//   number. If it had it, it was removed at the "pos" check
                envir.sprintf("DISPLAY=%s.%d", display_name.data(), KWinInternal::screen_number);

                if (putenv( strdup(envir.data())) ) 
                    {
                    fprintf(stderr,
                            "%s: WARNING: unable to set DISPLAY environment variable\n",
                            argv[0]);
                    perror("putenv()");
                    }
                }
            }
        }

    KAboutData aboutData( "kwin", I18N_NOOP("KWin"),
                          version, description, KAboutData::License_GPL,
                          I18N_NOOP("(c) 1999-2003, The KDE Developers"));
    aboutData.addAuthor("Matthias Ettrich",0, "ettrich@kde.org");
    aboutData.addAuthor("Cristian Tibirna",0, "tibirna@kde.org");
    aboutData.addAuthor("Daniel M. Duley",0, "mosfet@kde.org");
    aboutData.addAuthor("Lubos Lunak", 0, "l.lunak@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineArgs::addCmdLineOptions( args );

    if (signal(SIGTERM, KWinInternal::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWinInternal::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWinInternal::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);

    KApplication::disableAutoDcopRegistration();
    KWinInternal::Application a;
    KWinInternal::SessionManaged weAreIndeed;
    KWinInternal::SessionSaveDoneHelper helper;

    fcntl(ConnectionNumber(qt_xdisplay()), F_SETFD, 1);

    QCString appname;
    if (KWinInternal::screen_number == 0)
        appname = "kwin";
    else
        appname.sprintf("kwin-screen-%d", KWinInternal::screen_number);

    DCOPClient* client = a.dcopClient();
    client->registerAs( appname.data(), false);
    client->setDefaultObject( "KWinInterface" );

    return a.exec();
    }

#include "main.moc"
