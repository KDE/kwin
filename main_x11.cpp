/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "main_x11.h"
#include <config-kwin.h>
// kwin
#include "sm.h"
#include "xcbutils.h"

// KDE
#include <KLocalizedString>
// Qt
#include <qplatformdefs.h>
#include <QCommandLineParser>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

namespace KWin
{

static void sighandler(int)
{
    QApplication::exit();
}

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
        screen_P = QX11Info::appScreen();
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

//************************************
// ApplicationX11
//************************************

ApplicationX11::ApplicationX11(int &argc, char **argv)
    : Application(OperationModeX11, argc, argv)
    , owner()
    , m_replace(false)
{
    setX11Connection(QX11Info::connection());
    setX11RootWindow(QX11Info::appRootWindow());
}

ApplicationX11::~ApplicationX11()
{
    destroyWorkspace();
    if (!owner.isNull() && owner->ownerWindow() != XCB_WINDOW_NONE)   // If there was no --replace (no new WM)
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
}

void ApplicationX11::setReplace(bool replace)
{
    m_replace = replace;
}

void ApplicationX11::lostSelection()
{
    sendPostedEvents();
    destroyWorkspace();
    // Remove windowmanager privileges
    Xcb::selectInput(rootWindow(), XCB_EVENT_MASK_PROPERTY_CHANGE);
    quit();
}

void ApplicationX11::performStartup()
{
    if (Application::x11ScreenNumber() == -1) {
        Application::setX11ScreenNumber(QX11Info::appScreen());
    }

    // QSessionManager for some reason triggers a very early commitDataRequest
    // and updates the key - before we create the workspace and load the session
    // data -> store and pass to the workspace constructor
    m_originalSessionKey = sessionKey();

    owner.reset(new KWinSelectionOwner(Application::x11ScreenNumber()));
    connect(owner.data(), &KSelectionOwner::failedToClaimOwnership, []{
        fputs(i18n("kwin: unable to claim manager selection, another wm running? (try using --replace)\n").toLocal8Bit().constData(), stderr);
        ::exit(1);
    });
    connect(owner.data(), SIGNAL(lostOwnership()), SLOT(lostSelection()));
    connect(owner.data(), &KSelectionOwner::claimedOwnership, [this]{
        setupEventFilters();
        // first load options - done internally by a different thread
        createOptions();

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

        createInput();
        createWorkspace();

        Xcb::sync(); // Trigger possible errors, there's still a chance to abort

        notifyKSplash();
    });
    // we need to do an XSync here, otherwise the QPA might crash us later on
    Xcb::sync();
    owner->claim(m_replace, true);

    createAtoms();
}

} // namespace

extern "C"
KWIN_EXPORT int kdemain(int argc, char * argv[])
{
    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();

    int primaryScreen = 0;
    xcb_connection_t *c = xcb_connect(nullptr, &primaryScreen);
    if (!c || xcb_connection_has_error(c)) {
        fprintf(stderr, "%s: FATAL ERROR while trying to open display %s\n",
                argv[0], qgetenv("DISPLAY").constData());
        exit(1);
    }

    const int number_of_screens = xcb_setup_roots_length(xcb_get_setup(c));
    xcb_disconnect(c);
    c = nullptr;

    // multi head
    auto isMultiHead = []() -> bool {
        QByteArray multiHead = qgetenv("KDE_MULTIHEAD");
        if (!multiHead.isEmpty()) {
            return (multiHead.toLower() == "true");
        }
        return true;
    };
    if (number_of_screens != 1 && isMultiHead()) {
        KWin::Application::setX11MultiHead(true);
        KWin::Application::setX11ScreenNumber(primaryScreen);
        int pos; // Temporarily needed to reconstruct DISPLAY var if multi-head
        QByteArray display_name = qgetenv("DISPLAY");

        if ((pos = display_name.lastIndexOf('.')) != -1)
            display_name.remove(pos, 10);   // 10 is enough to be sure we removed ".s"

        QString envir;
        for (int i = 0; i < number_of_screens; i++) {
            // If execution doesn't pass by here, then kwin
            // acts exactly as previously
            if (i != KWin::Application::x11ScreenNumber() && fork() == 0) {
                KWin::Application::setX11ScreenNumber(i);
                QByteArray dBusSuffix = qgetenv("KWIN_DBUS_SERVICE_SUFFIX");
                if (!dBusSuffix.isNull()) {
                    dBusSuffix.append(".");
                }
                dBusSuffix.append(QByteArrayLiteral("head-")).append(QByteArray::number(i));
                qputenv("KWIN_DBUS_SERVICE_SUFFIX", dBusSuffix);
                // Break here because we are the child process, we don't
                // want to fork() anymore
                break;
            }
        }
        // In the next statement, display_name shouldn't contain a screen
        // number. If it had it, it was removed at the "pos" check
        envir.sprintf("DISPLAY=%s.%d", display_name.data(), KWin::Application::x11ScreenNumber());

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

    // enforce xcb plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "xcb", true);

    qunsetenv("QT_DEVICE_PIXEL_RATIO");

    KWin::ApplicationX11 a(argc, argv);
    a.setupTranslator();

    KWin::Application::createAboutData();

    QCommandLineOption replaceOption(QStringLiteral("replace"), i18n("Replace already-running ICCCM2.0-compliant window manager"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(replaceOption);

    parser.process(a);
    a.processCommandLine(&parser);
    a.setReplace(parser.isSet(replaceOption));

    // perform sanity checks
    if (a.platformName().toLower() != QStringLiteral("xcb")) {
        fprintf(stderr, "%s: FATAL ERROR expecting platform xcb but got platform %s\n",
                argv[0], qPrintable(a.platformName()));
        exit(1);
    }
    if (!KWin::display()) {
        fprintf(stderr, "%s: FATAL ERROR KWin requires Xlib support in the xcb plugin. Do not configure Qt with -no-xcb-xlib\n",
                argv[0]);
        exit(1);
    }

    a.start();

    KWin::SessionSaveDoneHelper helper;
    Q_UNUSED(helper); // The sessionsavedonehelper opens a side channel to the smserver,
                      // listens for events and talks to it, so it needs to be created.
    return a.exec();
}
