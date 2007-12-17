/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "main.h"

//#define QT_CLEAN_NAMESPACE
#include <ksharedconfig.h>

#include <kglobal.h>
#include <klocale.h>
#include <stdlib.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kcrash.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <QX11Info>
#include <stdio.h>
#include <fixx11h.h>
#include <QtDBus/QtDBus>

#include <kdialog.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <QLabel>
#include <QComboBox>
#include <QVBoxLayout>

#include "atoms.h"
#include "options.h"
#include "sm.h"
#include "utils.h"
#include "effects.h"

#define INT8 _X11INT8
#define INT32 _X11INT32
#include <X11/Xproto.h>
#undef INT8
#undef INT32

namespace KWin
{

Options* options;

Atoms* atoms;

int screen_number = -1;

static bool initting = false;
// Whether to run Xlib in synchronous mode and print backtraces for X errors.
// Note that you most probably need to configure cmake with "-D__KDE_HAVE_GCC_VISIBILITY=0"
// and -rdynamic in CXXFLAGS for kBacktrace() to work.
static bool kwin_sync = false;

static
int x11ErrorHandler(Display *d, XErrorEvent *e)
    {
    char msg[80], req[80], number[80];
    bool ignore_badwindow = true; //maybe temporary

    if (initting &&
        (
         e->request_code == X_ChangeWindowAttributes
         || e->request_code == X_GrabKey
         )
        && (e->error_code == BadAccess))
        {
        fputs(i18n("kwin: it looks like there's already a window manager running. kwin not started.\n").toLocal8Bit(), stderr);
        exit(1);
        }

    if (ignore_badwindow && (e->error_code == BadWindow || e->error_code == BadColor))
        return 0;

    XGetErrorText(d, e->error_code, msg, sizeof(msg));
    sprintf(number, "%d", e->request_code);
    XGetErrorDatabaseText(d, "XRequest", number, "<unknown>", req, sizeof(req));

    fprintf(stderr, "kwin: %s(0x%lx): %s\n", req, e->resourceid, msg);

    if( kwin_sync )
	kDebug() << kBacktrace();

    if (initting)
        {
        fputs(i18n("kwin: failure during initialization; aborting").toLocal8Bit(), stderr);
        exit(1);
        }
    return 0;
    }


class AlternativeWMDialog : public KDialog
{
    public:
        AlternativeWMDialog() : KDialog()
        {
            setButtons( KDialog::Ok | KDialog::Cancel );

            QWidget* mainWidget = new QWidget( this );
            QVBoxLayout* layout = new QVBoxLayout( mainWidget );
            QString text = i18n("KWin is unstable.\n"
                                "It seems to have crashed several times in a row.\n"
                                "You can select another window manager to run:");
            QLabel* textLabel = new QLabel( text, mainWidget );
            layout->addWidget( textLabel );
            wmList = new QComboBox( mainWidget );
            wmList->setEditable( true );
            layout->addWidget( wmList );

            addWM( "metacity" );
            addWM( "openbox" );
            addWM( "fvwm2" );
            addWM( "kwin" );

            setMainWidget(mainWidget);

            raise();
            centerOnScreen( this );
        }
        void addWM( const QString& wm )
        {
            // TODO: check if wm is installed
            if( !KStandardDirs::findExe( wm ).isEmpty() )
            {
                wmList->addItem( wm );
            }
        }
        QString selectedWM() const  { return wmList->currentText(); }

    private:
        QComboBox* wmList;
};

int Application::crashes = 0;

Application::Application( )
: KApplication( ), owner( screen_number )
    {
    if( KCmdLineArgs::parsedArgs( "qt" )->isSet( "sync" ))
	{
	kwin_sync = true;
	XSynchronize( display(), True );
	kDebug() << "Running KWin in sync mode";
	}
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    KSharedConfig::Ptr config = KGlobal::config();
    if (!config->isImmutable() && args->isSet("lock"))
        {
#warning this shouldn not be necessary
        //config->setReadOnly(true);
        config->reparseConfiguration();
        }

    if (screen_number == -1)
        screen_number = DefaultScreen(display());

    if( !owner.claim( args->isSet( "replace" ), true ))
        {
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").toLocal8Bit(), stderr);
        ::exit(1);
        }
    connect( &owner, SIGNAL( lostOwnership()), SLOT( lostSelection()));

    KCrash::setEmergencySaveFunction( Application::crashHandler );
    crashes = args->getOption("crashes").toInt();
    if(crashes >= 4)
    {
        // Something has gone seriously wrong
        AlternativeWMDialog dialog;
        QString cmd = "kwin";
        if( dialog.exec() == QDialog::Accepted )
        {
            cmd = dialog.selectedWM();
        }
        else
            ::exit(1);
        if( cmd.length() > 500 )
        {
            kDebug() << "Command is too long, truncating";
            cmd = cmd.left(500);
        }
        kDebug() << "Starting" << cmd << "and exiting";
        char buf[1024];
        sprintf(buf, "%s &", cmd.toAscii().data());
        system(buf);
        ::exit(1);
    }
    // Disable compositing if we have had too many crashes
    if( crashes >= 2 )
    {
        kDebug() << "Too many crashes recently, disabling compositing";
        KConfigGroup compgroup( config, "Compositing" );
        compgroup.writeEntry( "Enabled", false );
    }
    // Reset crashes count if we stay up for more that 15 seconds
    QTimer::singleShot( 15*1000, this, SLOT( resetCrashesCount() ));

    // if there was already kwin running, it saved its configuration after loosing the selection -> reread
    config->reparseConfiguration();

    initting = true; // startup....

    // install X11 error handler
    XSetErrorHandler( x11ErrorHandler );

    // check  whether another windowmanager is running
    XSelectInput(display(), rootWindow(), SubstructureRedirectMask  );
    syncX(); // trigger error now

    atoms = new Atoms;

    initting = false; // TODO

    // This tries to detect compositing options and can use GLX. GLX problems
    //  (X errors) shouldn't cause kwin to abort, so this is out of the
    //  critical startup section where x errors cause kwin to abort.
    options = new Options;

    // create workspace.
    (void) new Workspace( isSessionRestored() );

    syncX(); // trigger possible errors, there's still a chance to abort

    initting = false; // startup done, we are up and running now.

    XEvent e;
    e.xclient.type = ClientMessage;
    e.xclient.message_type = XInternAtom( display(), "_KDE_SPLASH_PROGRESS", False );
    e.xclient.display = display();
    e.xclient.window = rootWindow();
    e.xclient.format = 8;
    strcpy( e.xclient.data.b, "wm" );
    XSendEvent( display(), rootWindow(), False, SubstructureNotifyMask, &e );
    }

Application::~Application()
    {
    delete Workspace::self();
    if( owner.ownerWindow() != None ) // if there was no --replace (no new WM)
        XSetInputFocus( display(), PointerRoot, RevertToPointerRoot, xTime() );
    delete options;
    delete effects;
    delete atoms;
    }

void Application::lostSelection()
    {
    sendPostedEvents();
    delete Workspace::self();
    // remove windowmanager privileges
    XSelectInput(display(), rootWindow(), PropertyChangeMask );
    quit();
    }

bool Application::x11EventFilter( XEvent *e )
    {
    if ( Workspace::self() && Workspace::self()->workspaceEvent( e ) )
             return true;
    return KApplication::x11EventFilter( e );
    }

bool Application::notify( QObject* o, QEvent* e )
    {
    if( Workspace::self()->workspaceEvent( e ))
        return true;
    return KApplication::notify( o, e );
    }

static void sighandler(int)
    {
    QApplication::exit();
    }

void Application::crashHandler(int signal)
    {
    crashes++;

    fprintf( stderr, "Application::crashHandler() called with signal %d; recent crashes: %d\n", signal, crashes );
    char cmd[1024];
    sprintf( cmd, "kwin --crashes %d &", crashes );

    sleep( 1 );
    system( cmd );
    }

void Application::resetCrashesCount()
    {
    crashes = 0;
    }


} // namespace

static const char version[] = "3.0";
static const char description[] = I18N_NOOP( "KDE window manager" );

extern "C"
KDE_EXPORT int kdemain( int argc, char * argv[] )
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
        QByteArray multiHead = getenv("KDE_MULTIHEAD");
        if (multiHead.toLower() == "true")
            {

            Display* dpy = XOpenDisplay( NULL );
            if ( !dpy )
                {
                fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
                        argv[0], XDisplayName(NULL ) );
                exit (1);
                }

            int number_of_screens = ScreenCount( dpy );
            KWin::screen_number = DefaultScreen( dpy );
            int pos; // temporarily needed to reconstruct DISPLAY var if multi-head
            QByteArray display_name = XDisplayString( dpy );
            XCloseDisplay( dpy );
            dpy = 0;

            if ((pos = display_name.lastIndexOf('.')) != -1 )
                display_name.remove(pos,10); // 10 is enough to be sure we removed ".s"

            QString envir;
            if (number_of_screens != 1)
                {
                for (int i = 0; i < number_of_screens; i++ )
                    {
		    // if execution doesn't pass by here, then kwin
		    // acts exactly as previously
                    if ( i != KWin::screen_number && fork() == 0 )
                        {
                        KWin::screen_number = i;
			// break here because we are the child process, we don't
			// want to fork() anymore
                        break;
                        }
                    }
		// in the next statement, display_name shouldn't contain a screen
		//   number. If it had it, it was removed at the "pos" check
                envir.sprintf("DISPLAY=%s.%d", display_name.data(), KWin::screen_number);

                if (putenv( strdup(envir.toAscii())) )
                    {
                    fprintf(stderr,
                            "%s: WARNING: unable to set DISPLAY environment variable\n",
                            argv[0]);
                    perror("putenv()");
                    }
                }
            }
        }

    KAboutData aboutData( "kwin", 0, ki18n("KWin"),
                          version, ki18n(description), KAboutData::License_GPL,
                          ki18n("(c) 1999-2005, The KDE Developers"));
    aboutData.addAuthor(ki18n("Matthias Ettrich"),KLocalizedString(), "ettrich@kde.org");
    aboutData.addAuthor(ki18n("Cristian Tibirna"),KLocalizedString(), "tibirna@kde.org");
    aboutData.addAuthor(ki18n("Daniel M. Duley"),KLocalizedString(), "mosfet@kde.org");
    aboutData.addAuthor(ki18n("Luboš Luňák"), ki18n( "Maintainer" ), "l.lunak@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions args;
    args.add("lock", ki18n("Disable configuration options"));
    args.add("replace", ki18n("Replace already-running ICCCM2.0-compliant window manager"));
    args.add("crashes <n>", ki18n("Indicate that KWin has recently crashed n times"));
    KCmdLineArgs::addCmdLineOptions( args );

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);
    // HACK this is needed for AIGLX
    setenv( "LIBGL_ALWAYS_INDIRECT","1", true );
    KWin::Application a;
    KWin::SessionManager weAreIndeed;
    KWin::SessionSaveDoneHelper helper;
    KGlobal::locale()->insertCatalog( "kwin_effects" );

    fcntl(XConnectionNumber(KWin::display()), F_SETFD, 1);

    QString appname;
    if (KWin::screen_number == 0)
        appname = "org.kde.kwin";
    else
        appname.sprintf("org.kde.kwin-screen-%d", KWin::screen_number);

    QDBusConnection::sessionBus().interface()->registerService( appname, QDBusConnectionInterface::DontQueueService );

    return a.exec();
    }

#include "main.moc"
