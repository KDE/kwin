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
#include "config-workspace.h"
// kwin
#include "atoms.h"
#include "options.h"
#include "sm.h"
#include "workspace.h"
#include "xcbutils.h"

// KDE workspace
#include <ksmserver_interface.h>

// KDE
#include <KDE/KAboutData>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KCrash>
#include <KDE/KLocalizedString>
// Qt
#include <qplatformdefs.h>
#include <QDebug>
#include <QComboBox>
#include <qcommandlineparser.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtDBus/QtDBus>
#include <QX11Info>
// TODO: remove once QX11Info provides the X screen
#include <X11/Xlib.h>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif // HAVE_MALLOC_H

namespace KWin
{

Options* options;

Atoms* atoms;

int screen_number = -1;
bool is_multihead = false;

//************************************
// KWinSelectionOwner
//************************************

KWinSelectionOwner::KWinSelectionOwner(int screen_P)
    : KSelectionOwner(make_selection_atom(screen_P), screen_P)
{
}

xcb_atom_t KWinSelectionOwner::make_selection_atom(int screen_P)
{
    if (screen_P < 0)
        screen_P = DefaultScreen(display());
    QByteArray screen(QByteArrayLiteral("WM_S"));
    screen.append(QByteArray::number(screen_P));
    ScopedCPointer<xcb_intern_atom_reply_t> atom(xcb_intern_atom_reply(
        connection(),
        xcb_intern_atom_unchecked(connection(), false, screen.length(), screen.constData()),
        nullptr));
    if (atom.isNull()) {
        return XCB_ATOM_NONE;
    }
    return atom->atom;
}

void KWinSelectionOwner::getAtoms()
{
    KSelectionOwner::getAtoms();
    if (xa_version == XCB_ATOM_NONE) {
        const QByteArray name(QByteArrayLiteral("VERSION"));
        ScopedCPointer<xcb_intern_atom_reply_t> atom(xcb_intern_atom_reply(
            connection(),
            xcb_intern_atom_unchecked(connection(), false, name.length(), name.constData()),
            nullptr));
        if (!atom.isNull()) {
            xa_version = atom->atom;
        }
    }
}

void KWinSelectionOwner::replyTargets(xcb_atom_t property_P, xcb_window_t requestor_P)
{
    KSelectionOwner::replyTargets(property_P, requestor_P);
    xcb_atom_t atoms[ 1 ] = { xa_version };
    // PropModeAppend !
    xcb_change_property(connection(), XCB_PROP_MODE_APPEND, requestor_P,
                        property_P, XCB_ATOM_ATOM, 32, 1, atoms);
}

bool KWinSelectionOwner::genericReply(xcb_atom_t target_P, xcb_atom_t property_P, xcb_window_t requestor_P)
{
    if (target_P == xa_version) {
        int32_t version[] = { 2, 0 };
        xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, requestor_P,
                            property_P, XCB_ATOM_INTEGER, 32, 2, version);
    } else
        return KSelectionOwner::genericReply(target_P, property_P, requestor_P);
    return true;
}

xcb_atom_t KWinSelectionOwner::xa_version = XCB_ATOM_NONE;

class AlternativeWMDialog : public QDialog
{
public:
    AlternativeWMDialog()
        : QDialog() {
        QWidget* mainWidget = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(mainWidget);
        QString text = i18n(
                           "KWin is unstable.\n"
                           "It seems to have crashed several times in a row.\n"
                           "You can select another window manager to run:");
        QLabel* textLabel = new QLabel(text, mainWidget);
        layout->addWidget(textLabel);
        wmList = new QComboBox(mainWidget);
        wmList->setEditable(true);
        layout->addWidget(wmList);

        addWM(QStringLiteral("metacity"));
        addWM(QStringLiteral("openbox"));
        addWM(QStringLiteral("fvwm2"));
        addWM(QStringLiteral(KWIN_NAME));

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(mainWidget);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setDefault(true);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);

        raise();
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
    QComboBox* wmList;
};

int Application::crashes = 0;

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , owner(screen_number)
    , m_eventFilter(new XcbEventFilter())
    , m_replace(false)
    , m_configLock(false)
    , m_operationMode(OperationModeWaylandAndX11)
{
}

void Application::setConfigLock(bool lock)
{
    m_configLock = lock;
}

void Application::setReplace(bool replace)
{
    m_replace = replace;
}

Application::OperationMode Application::operationMode() const
{
    return m_operationMode;
}

void Application::setOperationMode(OperationMode mode)
{
    m_operationMode = mode;
}

bool Application::shouldUseWaylandForCompositing() const
{
    return m_operationMode == OperationModeWaylandAndX11;
}

void Application::start()
{
    setQuitOnLastWindowClosed(false);

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    if (!config->isImmutable() && m_configLock) {
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
    connect(&owner, &KSelectionOwner::claimedOwnership, [this]{
        // we want all QQuickWindows with an alpha buffer
        QQuickWindow::setDefaultAlphaBuffer(true);

        installNativeEventFilter(m_eventFilter.data());
        // first load options - done internally by a different thread
        options = new Options;

        // Check  whether another windowmanager is running
        const uint32_t maskValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
        ScopedCPointer<xcb_generic_error_t> redirectCheck(xcb_request_check(connection(),
                                                                            xcb_change_window_attributes_checked(connection(),
                                                                                                                 rootWindow(),
                                                                                                                 XCB_CW_EVENT_MASK,
                                                                                                                 maskValues)));
        if (!redirectCheck.isNull()) {
            fputs(i18n("kwin: another window manager is running (try using --replace)\n").toLocal8Bit().constData(), stderr);
            ::exit(1);
        }

        atoms->retrieveHelpers();

        // This tries to detect compositing options and can use GLX. GLX problems
        // (X errors) shouldn't cause kwin to abort, so this is out of the
        // critical startup section where x errors cause kwin to abort.

        // create workspace.
        (void) new Workspace(isSessionRestored());

        Xcb::sync(); // Trigger possible errors, there's still a chance to abort

        // Tell KSplash that KWin has started
        QDBusMessage ksplashProgressMessage = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KSplash"),
                                                                             QStringLiteral("/KSplash"),
                                                                             QStringLiteral("org.kde.KSplash"),
                                                                             QStringLiteral("setStage"));
        ksplashProgressMessage.setArguments(QList<QVariant>() << QStringLiteral("wm"));
        QDBusConnection::sessionBus().asyncCall(ksplashProgressMessage);
    });
    crashChecking();
    // we need to do an XSync here, otherwise the QPA might crash us later on
    Xcb::sync();
    owner.claim(m_replace, true);

    atoms = new Atoms;
}

Application::~Application()
{
    delete Workspace::self();
    if (owner.ownerWindow() != XCB_WINDOW_NONE)   // If there was no --replace (no new WM)
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
    delete options;
    delete atoms;
}

void Application::crashChecking()
{
    KCrash::setEmergencySaveFunction(Application::crashHandler);
    if (crashes >= 4) {
        // Something has gone seriously wrong
        AlternativeWMDialog dialog;
        QString cmd = QStringLiteral(KWIN_NAME);
        if (dialog.exec() == QDialog::Accepted)
            cmd = dialog.selectedWM();
        else
            ::exit(1);
        if (cmd.length() > 500) {
            qDebug() << "Command is too long, truncating";
            cmd = cmd.left(500);
        }
        qDebug() << "Starting" << cmd << "and exiting";
        char buf[1024];
        sprintf(buf, "%s &", cmd.toAscii().data());
        system(buf);
        ::exit(1);
    }
    if (crashes >= 2) {
        // Disable compositing if we have had too many crashes
        qDebug() << "Too many crashes recently, disabling compositing";
        KConfigGroup compgroup(KSharedConfig::openConfig(), "Compositing");
        compgroup.writeEntry("Enabled", false);
    }
    // Reset crashes count if we stay up for more that 15 seconds
    QTimer::singleShot(15 * 1000, this, SLOT(resetCrashesCount()));
}

void Application::lostSelection()
{
    sendPostedEvents();
    delete Workspace::self();
    // Remove windowmanager privileges
    Xcb::selectInput(rootWindow(), XCB_EVENT_MASK_PROPERTY_CHANGE);
    quit();
}

bool Application::notify(QObject* o, QEvent* e)
{
    if (Workspace::self()->workspaceEvent(e))
        return true;
    return QApplication::notify(o, e);
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

void Application::setCrashCount(int count)
{
    crashes = count;
}

bool Application::wasCrash()
{
    return crashes > 0;
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

static const char version[] = KWIN_VERSION_STRING;
static const char description[] = I18N_NOOP("KDE window manager");

extern "C"
KWIN_EXPORT int kdemain(int argc, char * argv[])
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

    QLoggingCategory::setFilterRules(QStringLiteral("aurorae.debug = true\n") +
                                     QStringLiteral("kwineffects.debug = true"));

    int primaryScreen = 0;
    xcb_connection_t *c = xcb_connect(nullptr, &primaryScreen);
    if (!c || xcb_connection_has_error(c)) {
        fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
                argv[0], qgetenv("DISPLAY").constData());
        exit(1);
    }

    const int number_of_screens = xcb_setup_roots_length(xcb_get_setup(c));

    // multi head
    auto isMultiHead = []() -> bool {
        QByteArray multiHead = qgetenv("KDE_MULTIHEAD");
        if (!multiHead.isEmpty()) {
            return (multiHead.toLower() == "true");
        }
        return true;
    };
    if (number_of_screens != 1 && isMultiHead()) {
        KWin::is_multihead = true;
        KWin::screen_number = primaryScreen;
        int pos; // Temporarily needed to reconstruct DISPLAY var if multi-head
        QByteArray display_name = qgetenv("DISPLAY");
        xcb_disconnect(c);
        c = nullptr;

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

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);

    // Disable the glib event loop integration, since it seems to be responsible
    // for several bug reports about high CPU usage (bug #239963)
    setenv("QT_NO_GLIB", "1", true);

    org::kde::KSMServerInterface ksmserver(QStringLiteral("org.kde.ksmserver"), QStringLiteral("/KSMServer"), QDBusConnection::sessionBus());
    ksmserver.suspendStartup(QStringLiteral(KWIN_NAME));
    KWin::Application a(argc, argv);

    a.setApplicationName(QStringLiteral(KWIN_NAME));
    a.setApplicationVersion(QStringLiteral(KWIN_VERSION_STRING));
    a.setApplicationDisplayName(i18n("KWin"));

    KAboutData aboutData(QStringLiteral(KWIN_NAME),          // The program name used internally
                         QString(),                          // The message catalog name. If null, program name is used instead
                         i18n("KWin"),                       // A displayable program name string
                         QStringLiteral(KWIN_VERSION_STRING), // The program version string
                         i18n(description),                  // Short description of what the app does
                         KAboutData::License_GPL,            // The license this code is released under
                         i18n("(c) 1999-2013, The KDE Developers"));   // Copyright Statement

    aboutData.addAuthor(i18n("Matthias Ettrich"), QString(), QStringLiteral("ettrich@kde.org"));
    aboutData.addAuthor(i18n("Cristian Tibirna"), QString(), QStringLiteral("tibirna@kde.org"));
    aboutData.addAuthor(i18n("Daniel M. Duley"),  QString(), QStringLiteral("mosfet@kde.org"));
    aboutData.addAuthor(i18n("Luboš Luňák"),      QString(), QStringLiteral("l.lunak@kde.org"));
    aboutData.addAuthor(i18n("Martin Gräßlin"),   i18n("Maintainer"), QStringLiteral("mgraesslin@kde.org"));

    QCommandLineOption lockOption(QStringLiteral("lock"), i18n("Disable configuration options"));
    QCommandLineOption replaceOption(QStringLiteral("replace"), i18n("Replace already-running ICCCM2.0-compliant window manager"));
    QCommandLineOption crashesOption(QStringLiteral("crashes"), i18n("Indicate that KWin has recently crashed n times"), QStringLiteral("n"));

    QCommandLineParser parser;
    parser.setApplicationDescription(i18n("KDE window manager"));
    parser.addVersionOption();
    parser.addHelpOption();
    parser.addOption(lockOption);
    parser.addOption(replaceOption);
    parser.addOption(crashesOption);

    parser.process(a);

    KWin::Application::setCrashCount(parser.value(crashesOption).toInt());
    a.setConfigLock(parser.isSet(lockOption));
    a.setReplace(parser.isSet(replaceOption));

    a.start();

    ksmserver.resumeStartup(QStringLiteral(KWIN_NAME));
#warning SessionManager needs porting
#if KWIN_QT5_PORTING
    KWin::SessionManager weAreIndeed;
#endif
    KWin::SessionSaveDoneHelper helper;
#warning insertCatalog needs porting
#if KWIN_QT5_PORTING
    KGlobal::locale()->insertCatalog("kwin_effects");
    KGlobal::locale()->insertCatalog("kwin_scripts");
    KGlobal::locale()->insertCatalog("kwin_scripting");
#endif

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
