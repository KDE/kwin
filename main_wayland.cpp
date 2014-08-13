/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

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
#include "main_wayland.h"
#include <config-kwin.h>
// kwin
#include "wayland_backend.h"
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

#include <iostream>

namespace KWin
{

static void sighandler(int)
{
    QApplication::exit();
}

//************************************
// ApplicationWayland
//************************************

ApplicationWayland::ApplicationWayland(int &argc, char **argv)
    : Application(OperationModeWaylandAndX11, argc, argv)
{
}

ApplicationWayland::~ApplicationWayland()
{
    destroyWorkspace();
    delete Wayland::WaylandBackend::self();
    // TODO: only if we support X11
    Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
}

void ApplicationWayland::performStartup()
{
    // we don't support X11 multi-head in Wayland
    Application::setX11ScreenNumber(0);

    // we need to do an XSync here, otherwise the QPA might crash us later on
    // TODO: remove
    Xcb::sync();

    createAtoms();

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
        fputs(i18n("kwin_wayland: an X11 window manager is running on the X11 Display.\n").toLocal8Bit().constData(), stderr);
        ::exit(1);
    }

    // try creating the Wayland Backend
    Wayland::WaylandBackend *backend = Wayland::WaylandBackend::create();
    connect(backend, &Wayland::WaylandBackend::connectionFailed, this,
        [] () {
            fputs(i18n("kwin_wayland: could not connect to Wayland Server, ensure WAYLAND_DISPLAY is set.\n").toLocal8Bit().constData(), stderr);
            ::exit(1);
        }
    );

    createWorkspace();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort

    notifyKSplash();
}

/**
 * Starts the X-Server with binary name @p process on @p display.
 * The new process is started by forking into it.
 **/
static void startXServer(const QByteArray &process, const QByteArray &display)
{
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start X Server "
                  << process.constData()
                  << " with arguments "
                  << display.constData()
                  << std::endl;
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process - should be turned into X-Server
        // writes to pipe, closes read side
        close(pipeFds[0]);
        char fdbuf[16];
        sprintf(fdbuf, "%d", pipeFds[1]);
        execlp(process.constData(), process.constData(), "-displayfd", fdbuf, display.constData(), (char *)0);
        close(pipeFds[1]);
        exit(20);
    }
    // parent process - this is KWin
    // reads from pipe, closes write side
    close(pipeFds[1]);

    QFile readPipe;
    if (!readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server "
                  << process.constData()
                  << " with arguments "
                  << display.constData()
                  << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() -1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData();

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipeFds[0]);
}

} // namespace

extern "C"
KWIN_EXPORT int kdemain(int argc, char * argv[])
{
    // process command line arguments to figure out whether we have to start an X-Server
    bool startXephyr = false;
    bool startXvfb = false;
    bool startXwayland = false;
    QByteArray xDisplay;
    QByteArray xServer;
    for (int i = 1; i < argc; ++i) {
        QByteArray arg = argv[i];
        if (arg == "-x" || arg == "--start-X-Server") {
            if (++i < argc) {
                xServer = argv[i];
            }
            startXephyr = (xServer == QStringLiteral("xephyr"));
            startXvfb = (xServer == QStringLiteral("xvfb"));
            startXwayland = (xServer == QStringLiteral("xwayland"));
            if (!startXephyr && !startXvfb && !startXwayland) {
                fprintf(stderr, "%s: FATAL ERROR unknown X-Server %s specified to start\n",
                        argv[0], qPrintable(xServer));
                exit(1);
            }
            continue;
        }
        if (arg == "--display") {
            if (++i < argc) {
                xDisplay = argv[i];
            }
        }
    }

    if (startXephyr) {
        KWin::startXServer(QByteArrayLiteral("Xephyr"), xDisplay);
    }
    if (startXvfb) {
        KWin::startXServer(QByteArrayLiteral("Xvfb"), xDisplay);
    }
    if (startXwayland) {
        KWin::startXServer(QByteArrayLiteral("Xwayland"), xDisplay);
    }

    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();
    KWin::Application::setupLoggingCategoryFilters();

    // TODO: check whether we have a wayland connection

    // Disable the glib event loop integration, since it seems to be responsible
    // for several bug reports about high CPU usage (bug #239963)
    setenv("QT_NO_GLIB", "1", true);

    // enforce xcb plugin, unfortunately command line switch has precedence
    // TODO: ensure it's not xcb once we support the Wayland QPA
    setenv("QT_QPA_PLATFORM", "xcb", true);

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);

    KWin::ApplicationWayland a(argc, argv);
    a.setupTranslator();

    KWin::Application::createAboutData();

    QCommandLineOption startXServerOption(QStringList({QStringLiteral("x"), QStringLiteral("x-server")}),
                                          i18n("Start a nested X Server."),
                                          QStringLiteral("xephyr|xvfb|xwayland"));
    QCommandLineOption x11DisplayOption(QStringLiteral("display"),
                                        i18n("The X11 Display to connect to, required if option x-server is used."),
                                        QStringLiteral("display"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(startXServerOption);
    parser.addOption(x11DisplayOption);

    parser.process(a);
    a.processCommandLine(&parser);

    // perform sanity checks
    // TODO: remove those two
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

    // TODO: is this still needed?
    a.registerDBusService();

    return a.exec();
}
