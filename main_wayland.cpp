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
#include "wayland_server.h"
#include "xcbutils.h"

// KWayland
#include <KWayland/Server/display.h>
// KDE
#include <KLocalizedString>
// Qt
#include <qplatformdefs.h>
#include <QCommandLineParser>
#include <QFile>

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
 * Starts the Xwayland-Server.
 * The new process is started by forking into it.
 **/
static int startXServer(const QByteArray &waylandSocket)
{
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start Xwayland " << std::endl;
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process - should be turned into X-Server
        // writes to pipe, closes read side
        close(pipeFds[0]);
        char fdbuf[16];
        sprintf(fdbuf, "%d", pipeFds[1]);
        qputenv("WAYLAND_DISPLAY", waylandSocket.isEmpty() ? QByteArrayLiteral("wayland-0") : waylandSocket);
        execlp("Xwayland", "Xwayland", "-displayfd", fdbuf, "-rootless", (char *)0);
        close(pipeFds[1]);
        exit(20);
    }
    // parent process - this is KWin
    // reads from pipe, closes write side
    close(pipeFds[1]);
    return pipeFds[0];
}

static void readDisplay(int pipe)
{
    QFile readPipe;
    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server" << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() -1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

} // namespace

extern "C"
KWIN_EXPORT int kdemain(int argc, char * argv[])
{
    // process command line arguments to figure out whether we have to start Xwayland and the Wayland socket
    bool startXwayland = false;
    QByteArray waylandSocket;
    for (int i = 1; i < argc; ++i) {
        QByteArray arg = QByteArray::fromRawData(argv[i], qstrlen(argv[i]));
        if (arg == "--xwayland") {
            startXwayland = true;
        }
        if (arg == "--socket" || arg == "-s") {
            if (++i < argc) {
                waylandSocket = QByteArray::fromRawData(argv[i], qstrlen(argv[i]));
            }
            continue;
        }
        if (arg.startsWith("--socket=")) {
            waylandSocket = arg.mid(9);
        }
    }

    KWin::WaylandServer *server = KWin::WaylandServer::create(nullptr);
    server->init(waylandSocket);

    if (startXwayland) {
        const int xDisplayPipe = KWin::startXServer(waylandSocket);
        fd_set rfds;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        do {
            server->display()->dispatchEvents(1000);
            FD_ZERO(&rfds);
            FD_SET(xDisplayPipe, &rfds);
        } while (select(xDisplayPipe + 1, &rfds, NULL, NULL, &tv) == 0);
        KWin::readDisplay(xDisplayPipe);
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

    server->setParent(&a);
    server->display()->startLoop();

    KWin::Application::createAboutData();

    QCommandLineOption xwaylandOption(QStringLiteral("xwayland"),
                                      i18n("Start a rootless Xwayland server."));
    QCommandLineOption waylandSocketOption(QStringList{QStringLiteral("s"), QStringLiteral("socket")},
                                           i18n("Name of the Wayland socket to listen on. If not set \"wayland-0\" is used."),
                                           QStringLiteral("socket"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(xwaylandOption);
    parser.addOption(waylandSocketOption);
#if HAVE_INPUT
    QCommandLineOption libinputOption(QStringLiteral("libinput"),
                                      i18n("Enable libinput support for input events processing. Note: never use in a nested session."));
    parser.addOption(libinputOption);
#endif

    parser.process(a);
    a.processCommandLine(&parser);

#if HAVE_INPUT
    KWin::Application::setUseLibinput(parser.isSet(libinputOption));
#endif

    if (parser.isSet(xwaylandOption)) {
        a.setOperationMode(KWin::Application::OperationModeXwayland);

        // create selection owner for WM_S0 - magic X display number expected by XWayland
        KSelectionOwner owner("WM_S0");
        owner.claim(true);
    }

    // perform sanity checks
    // TODO: remove those two
    if (a.platformName().toLower() != QLatin1String("xcb")) {
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

    return a.exec();
}
