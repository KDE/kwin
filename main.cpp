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
#include <QTextStream>

//#define QT_CLEAN_NAMESPACE
#include <ksharedconfig.h>

#include <kdeversion.h>
#include <kglobal.h>
#include <klocale.h>
#include <stdlib.h>
#include <kcmdlineargs.h>
#include <k4aboutdata.h>
#include <kcrash.h>
#include <signal.h>
#include <fcntl.h>
#include <QX11Info>
#include <stdio.h>
#include <fixx11h.h>
#include <QtDBus/QtDBus>
#include <QMessageBox>
#include <QEvent>

#include <kdialog.h>
#include <QStandardPaths>
#include <kdebug.h>
#include <kde_file.h>
#include <QLabel>
#include <KComboBox>
#include <QVBoxLayout>
#include <KGlobalSettings>

#include "config-workspace.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif // HAVE_MALLOC_H

#include <ksmserver_interface.h>

#include "atoms.h"
#include "options.h"
#include "sm.h"
#include "utils.h"
#include "effects.h"
#include "workspace.h"
#include "xcbutils.h"

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
bool is_multihead = false;

bool initting = false;

/**
 * Whether to run Xlib in synchronous mode and print backtraces for X errors.
 * Note that you most probably need to configure cmake with "-D__KDE_HAVE_GCC_VISIBILITY=0"
 * and -rdynamic in CXXFLAGS for kBacktrace() to work.
 */
static bool kwin_sync = false;

//************************************
// KWinSelectionOwner
//************************************

KWinSelectionOwner::KWinSelectionOwner(int screen_P)
    : KSelectionOwner(make_selection_atom(screen_P), screen_P)
{
}

Atom KWinSelectionOwner::make_selection_atom(int screen_P)
{
    if (screen_P < 0)
        screen_P = DefaultScreen(display());
    char tmp[ 30 ];
    sprintf(tmp, "WM_S%d", screen_P);
    return XInternAtom(display(), tmp, False);
}

void KWinSelectionOwner::getAtoms()
{
    KSelectionOwner::getAtoms();
    if (xa_version == None) {
        Atom atoms[ 1 ];
        const char* const names[] =
        { "VERSION" };
        XInternAtoms(display(), const_cast< char** >(names), 1, False, atoms);
        xa_version = atoms[ 0 ];
    }
}

void KWinSelectionOwner::replyTargets(xcb_atom_t property_P, xcb_window_t requestor_P)
{
    KSelectionOwner::replyTargets(property_P, requestor_P);
    Atom atoms[ 1 ] = { xa_version };
    // PropModeAppend !
    XChangeProperty(display(), requestor_P, property_P, XA_ATOM, 32, PropModeAppend,
    reinterpret_cast< unsigned char* >(atoms), 1);
}

bool KWinSelectionOwner::genericReply(xcb_atom_t target_P, xcb_atom_t property_P, xcb_window_t requestor_P)
{
    if (target_P == xa_version) {
        long version[] = { 2, 0 };
        XChangeProperty(display(), requestor_P, property_P, XA_INTEGER, 32,
        PropModeReplace, reinterpret_cast< unsigned char* >(&version), 2);
    } else
        return KSelectionOwner::genericReply(target_P, property_P, requestor_P);
    return true;
}

Atom KWinSelectionOwner::xa_version = None;

class AlternativeWMDialog : public KDialog
{
public:
    AlternativeWMDialog()
        : KDialog() {
        setButtons(KDialog::Ok | KDialog::Cancel);

        QWidget* mainWidget = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(mainWidget);
        QString text = i18n(
                           "KWin is unstable.\n"
                           "It seems to have crashed several times in a row.\n"
                           "You can select another window manager to run:");
        QLabel* textLabel = new QLabel(text, mainWidget);
        layout->addWidget(textLabel);
        wmList = new KComboBox(mainWidget);
        wmList->setEditable(true);
        layout->addWidget(wmList);

        addWM(QStringLiteral("metacity"));
        addWM(QStringLiteral("openbox"));
        addWM(QStringLiteral("fvwm2"));
        addWM(QStringLiteral(KWIN_NAME));

        setMainWidget(mainWidget);

        raise();
        centerOnScreen(this);
    }

    void addWM(const QString& wm) {
        // TODO: Check if WM is installed
        if (!QStandardPaths::findExecutable(wm).isEmpty())
            wmList->addItem(wm);
    }
    QString selectedWM() const {
        return wmList->currentText();
    }

private:
    KComboBox* wmList;
};

int Application::crashes = 0;

Application::Application()
    : KApplication()
    , owner(screen_number)
    , m_eventFilter(new XcbEventFilter())
{
    if (KCmdLineArgs::parsedArgs("qt")->isSet("sync")) {
        kwin_sync = true;
        XSynchronize(display(), True);
        kDebug(1212) << "Running KWin in sync mode";
    }
    setQuitOnLastWindowClosed(false);
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    KSharedConfig::Ptr config = KGlobal::config();
    if (!config->isImmutable() && args->isSet("lock")) {
        // TODO: This shouldn't be necessary
        //config->setReadOnly( true );
        config->reparseConfiguration();
    }

    if (screen_number == -1)
        screen_number = QX11Info::appScreen();

    connect(&owner, &KSelectionOwner::failedToClaimOwnership, []{
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").toLocal8Bit().constData(), stderr);
        ::exit(1);
    });
    connect(&owner, SIGNAL(lostOwnership()), SLOT(lostSelection()));
    connect(&owner, &KSelectionOwner::claimedOwnership, [this, args, config]{
        KCrash::setEmergencySaveFunction(Application::crashHandler);
        crashes = args->getOption("crashes").toInt();
        if (crashes >= 4) {
            // Something has gone seriously wrong
            AlternativeWMDialog dialog;
            QString cmd = QStringLiteral(KWIN_NAME);
            if (dialog.exec() == QDialog::Accepted)
                cmd = dialog.selectedWM();
            else
                ::exit(1);
            if (cmd.length() > 500) {
                kDebug(1212) << "Command is too long, truncating";
                cmd = cmd.left(500);
            }
            kDebug(1212) << "Starting" << cmd << "and exiting";
            char buf[1024];
            sprintf(buf, "%s &", cmd.toAscii().data());
            system(buf);
            ::exit(1);
        }
        if (crashes >= 2) {
            // Disable compositing if we have had too many crashes
            kDebug(1212) << "Too many crashes recently, disabling compositing";
            KConfigGroup compgroup(config, "Compositing");
            compgroup.writeEntry("Enabled", false);
        }
        // Reset crashes count if we stay up for more that 15 seconds
        QTimer::singleShot(15 * 1000, this, SLOT(resetCrashesCount()));

        initting = true; // Startup...
        installNativeEventFilter(m_eventFilter.data());
        // first load options - done internally by a different thread
        options = new Options;

        // Check  whether another windowmanager is running
        XSelectInput(display(), rootWindow(), SubstructureRedirectMask);
        Xcb::sync(); // Trigger error now

        atoms = new Atoms;

    //    initting = false; // TODO

        // This tries to detect compositing options and can use GLX. GLX problems
        // (X errors) shouldn't cause kwin to abort, so this is out of the
        // critical startup section where x errors cause kwin to abort.

        // create workspace.
        (void) new Workspace(isSessionRestored());

        Xcb::sync(); // Trigger possible errors, there's still a chance to abort

        initting = false; // Startup done, we are up and running now.

        XEvent e;
        e.xclient.type = ClientMessage;
        e.xclient.message_type = XInternAtom(display(), "_KDE_SPLASH_PROGRESS", False);
        e.xclient.display = display();
        e.xclient.window = rootWindow();
        e.xclient.format = 8;
        strcpy(e.xclient.data.b, "wm");
        XSendEvent(display(), rootWindow(), False, SubstructureNotifyMask, &e);
    });
    // we need to do an XSync here, otherwise the QPA might crash us later on
    Xcb::sync();
    owner.claim(args->isSet("replace"), true);
}

Application::~Application()
{
    delete Workspace::self();
    if (owner.ownerWindow() != None)   // If there was no --replace (no new WM)
        XSetInputFocus(display(), PointerRoot, RevertToPointerRoot, xTime());
    delete options;
    delete effects;
    delete atoms;
}

void Application::lostSelection()
{
    sendPostedEvents();
    delete Workspace::self();
    // Remove windowmanager privileges
    XSelectInput(display(), rootWindow(), PropertyChangeMask);
    quit();
}

bool Application::notify(QObject* o, QEvent* e)
{
    if (Workspace::self()->workspaceEvent(e))
        return true;
    return KApplication::notify(o, e);
}

static void sighandler(int)
{
    QApplication::exit();
}

void Application::crashHandler(int signal)
{
    crashes++;

    fprintf(stderr, "Application::crashHandler() called with signal %d; recent crashes: %d\n", signal, crashes);
    char cmd[1024];
    sprintf(cmd, "%s --crashes %d &",
            QFile::encodeName(QCoreApplication::applicationFilePath()).constData(), crashes);

    sleep(1);
    system(cmd);
}

void Application::resetCrashesCount()
{
    crashes = 0;
}

bool XcbEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long int *result)
{
    Q_UNUSED(result)
    if (!Workspace::self()) {
        // Workspace not yet created
        return false;
    }
    if (eventType != "xcb_generic_event_t") {
        return false;
    }
    return Workspace::self()->workspaceEvent(static_cast<xcb_generic_event_t *>(message));
}

} // namespace

static const char version[] = KDE_VERSION_STRING;
static const char description[] = I18N_NOOP("KDE window manager");

extern "C"
KDE_EXPORT int kdemain(int argc, char * argv[])
{
#ifdef M_TRIM_THRESHOLD
    // Prevent fragmentation of the heap by malloc (glibc).
    //
    // The default threshold is 128*1024, which can result in a large memory usage
    // due to fragmentation especially if we use the raster graphicssystem. On the
    // otherside if the threshold is too low, free() starts to permanently ask the kernel
    // about shrinking the heap.
#ifdef HAVE_UNISTD_H
    const int pagesize = sysconf(_SC_PAGESIZE);
#else
    const int pagesize = 4*1024;
#endif // HAVE_UNISTD_H
    mallopt(M_TRIM_THRESHOLD, 5*pagesize);
#endif // M_TRIM_THRESHOLD

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
                argv[0], XDisplayName(NULL));
        exit(1);
    }

    int number_of_screens = ScreenCount(dpy);

    // multi head
    if (number_of_screens != 1 && KGlobalSettings::isMultiHead()) {
        KWin::is_multihead = true;
        KWin::screen_number = DefaultScreen(dpy);
        int pos; // Temporarily needed to reconstruct DISPLAY var if multi-head
        QByteArray display_name = XDisplayString(dpy);
        XCloseDisplay(dpy);
        dpy = 0;

        if ((pos = display_name.lastIndexOf('.')) != -1)
            display_name.remove(pos, 10);   // 10 is enough to be sure we removed ".s"

        QString envir;
        for (int i = 0; i < number_of_screens; i++) {
            // If execution doesn't pass by here, then kwin
            // acts exactly as previously
            if (i != KWin::screen_number && fork() == 0) {
                KWin::screen_number = i;
                // Break here because we are the child process, we don't
                // want to fork() anymore
                break;
            }
        }
        // In the next statement, display_name shouldn't contain a screen
        // number. If it had it, it was removed at the "pos" check
        envir.sprintf("DISPLAY=%s.%d", display_name.data(), KWin::screen_number);

        if (putenv(strdup(envir.toAscii().constData()))) {
            fprintf(stderr, "%s: WARNING: unable to set DISPLAY environment variable\n", argv[0]);
            perror("putenv()");
        }
    }

    K4AboutData aboutData(
        QByteArray(KWIN_NAME),      // The program name used internally
        QByteArray(),               // The message catalog name. If null, program name is used instead
        ki18n("KWin"),              // A displayable program name string
        QByteArray(version),        // The program version string
        ki18n(description),         // Short description of what the app does
        K4AboutData::License_GPL,   // The license this code is released under
        ki18n("(c) 1999-2008, The KDE Developers"));   // Copyright Statement
    aboutData.addAuthor(ki18n("Matthias Ettrich"), KLocalizedString(), "ettrich@kde.org");
    aboutData.addAuthor(ki18n("Cristian Tibirna"), KLocalizedString(), "tibirna@kde.org");
    aboutData.addAuthor(ki18n("Daniel M. Duley"), KLocalizedString(), "mosfet@kde.org");
    aboutData.addAuthor(ki18n("Luboš Luňák"), KLocalizedString(), "l.lunak@kde.org");
    aboutData.addAuthor(ki18n("Martin Gräßlin"), ki18n("Maintainer"), "mgraesslin@kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions args;
    args.add("lock", ki18n("Disable configuration options"));
    args.add("replace", ki18n("Replace already-running ICCCM2.0-compliant window manager"));
    args.add("crashes <n>", ki18n("Indicate that KWin has recently crashed n times"));
    KCmdLineArgs::addCmdLineOptions(args);

    if (KDE_signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        KDE_signal(SIGTERM, SIG_IGN);
    if (KDE_signal(SIGINT, KWin::sighandler) == SIG_IGN)
        KDE_signal(SIGINT, SIG_IGN);
    if (KDE_signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        KDE_signal(SIGHUP, SIG_IGN);

    // Disable the glib event loop integration, since it seems to be responsible
    // for several bug reports about high CPU usage (bug #239963)
    setenv("QT_NO_GLIB", "1", true);

    org::kde::KSMServerInterface ksmserver(QStringLiteral("org.kde.ksmserver"), QStringLiteral("/KSMServer"), QDBusConnection::sessionBus());
    ksmserver.suspendStartup(QStringLiteral(KWIN_NAME));
    KWin::Application a;

    ksmserver.resumeStartup(QStringLiteral(KWIN_NAME));
    KWin::SessionManager weAreIndeed;
    KWin::SessionSaveDoneHelper helper;
#warning insertCatalog needs porting
#if KWIN_QT5_PORTING
    KGlobal::locale()->insertCatalog("kwin_effects");
    KGlobal::locale()->insertCatalog("kwin_scripts");
    KGlobal::locale()->insertCatalog("kwin_scripting");
#endif

    // Announce when KWIN_DIRECT_GL is set for above HACK
    if (qstrcmp(qgetenv("KWIN_DIRECT_GL"), "1") == 0)
        kDebug(1212) << "KWIN_DIRECT_GL set, not forcing LIBGL_ALWAYS_INDIRECT=1";

    fcntl(XConnectionNumber(KWin::display()), F_SETFD, 1);

    QString appname;
    if (KWin::screen_number == 0)
        appname = QStringLiteral("org.kde.kwin");
    else
        appname.sprintf("org.kde.kwin-screen-%d", KWin::screen_number);

    QDBusConnection::sessionBus().interface()->registerService(
        appname, QDBusConnectionInterface::DontQueueService);

    return a.exec();
}

#include "main.moc"
