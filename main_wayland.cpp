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
#include "workspace.h"
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
#include <QtConcurrentRun>
#include <QFile>
#include <QFutureWatcher>
#include <QtCore/private/qeventdispatcher_unix_p.h>
#include <QSocketNotifier>
#include <QThread>
#include <QDebug>

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

static int startXServer(const QByteArray &waylandSocket, int wmFd);
static void readDisplay(int pipe);

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
    if (x11Connection()) {
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
        xcb_disconnect(x11Connection());
    }
}

void ApplicationWayland::performStartup()
{
    setOperationMode(m_startXWayland ? OperationModeXwayland : OperationModeWaylandAndX11);
    // first load options - done internally by a different thread
    createOptions();

    // try creating the Wayland Backend
    createInput();
    Wayland::WaylandBackend *backend = Wayland::WaylandBackend::create();
    connect(backend, &Wayland::WaylandBackend::connectionFailed, this,
        [] () {
            fputs(i18n("kwin_wayland: could not connect to Wayland Server, ensure WAYLAND_DISPLAY is set.\n").toLocal8Bit().constData(), stderr);
            ::exit(1);
        }
    );
    connect(backend, &Wayland::WaylandBackend::outputsChanged, this, &ApplicationWayland::continueStartupWithScreens);
}

void ApplicationWayland::continueStartupWithScreens()
{
    disconnect(Wayland::WaylandBackend::self(), &Wayland::WaylandBackend::outputsChanged, this, &ApplicationWayland::continueStartupWithScreens);
    createScreens();
    waylandServer()->initOutputs();

    if (!m_startXWayland) {
        continueStartupWithX();
        return;
    }
    createCompositor();

    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        exit(1);
        return;
    }

    m_xcbConnectionFd = sx[0];
    const int xDisplayPipe = startXServer(WaylandServer::self()->display()->socketName().toUtf8(), sx[1]);
    QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &ApplicationWayland::continueStartupWithX, Qt::QueuedConnection);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, &QFutureWatcher<void>::deleteLater, Qt::QueuedConnection);
    watcher->setFuture(QtConcurrent::run(readDisplay, xDisplayPipe));
}

void ApplicationWayland::continueStartupWithX()
{
    createX11Connection();
    xcb_connection_t *c = x11Connection();
    if (!c) {
        // about to quit
        return;
    }
    QSocketNotifier *notifier = new QSocketNotifier(xcb_get_file_descriptor(c), QSocketNotifier::Read, this);
    auto processXcbEvents = [this, c] {
        while (auto event = xcb_poll_for_event(c)) {
            updateX11Time(event);
            if (Workspace::self()) {
                Workspace::self()->workspaceEvent(event);
            }
            free(event);
        }
        xcb_flush(c);
    };
    connect(notifier, &QSocketNotifier::activated, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    KSelectionOwner owner("WM_S0", c, x11RootWindow());
    owner.claim(true);

    createAtoms();

    setupEventFilters();

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

    createWorkspace();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort

    notifyKSplash();
}

void ApplicationWayland::createX11Connection()
{
    int screenNumber = 0;
    xcb_connection_t *c = nullptr;
    if (m_xcbConnectionFd == -1) {
        c = xcb_connect(nullptr, &screenNumber);
    } else {
        c = xcb_connect_to_fd(m_xcbConnectionFd, nullptr);
    }
    if (int error = xcb_connection_has_error(c)) {
        std::cerr << "FATAL ERROR: Creating connection to XServer failed: " << error << std::endl;
        exit(1);
        return;
    }
    setX11Connection(c);
    // we don't support X11 multi-head in Wayland
    setX11ScreenNumber(screenNumber);
    setX11RootWindow(defaultScreen()->root);
}

/**
 * Starts the Xwayland-Server.
 * The new process is started by forking into it.
 **/
static int startXServer(const QByteArray &waylandSocket, int wmFd)
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
        char wmfdbuf[16];
        int fd = dup(wmFd);
        if (fd < 0) {
            std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
            exit(20);
            return -1;
        }
        sprintf(wmfdbuf, "%d", fd);
        qputenv("WAYLAND_DISPLAY", waylandSocket.isEmpty() ? QByteArrayLiteral("wayland-0") : waylandSocket);
        execlp("Xwayland", "Xwayland", "-displayfd", fdbuf, "-rootless", "-wm", wmfdbuf, (char *)0);
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
    QByteArray waylandSocket;
    for (int i = 1; i < argc; ++i) {
        QByteArray arg = QByteArray::fromRawData(argv[i], qstrlen(argv[i]));
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

    // set our own event dispatcher to be able to dispatch events before the event loop is started
    QAbstractEventDispatcher *eventDispatcher = new QEventDispatcherUNIX();
    QCoreApplication::setEventDispatcher(eventDispatcher);
    KWin::WaylandServer *server = KWin::WaylandServer::create(nullptr);
    server->init(waylandSocket);

    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();
    KWin::Application::setupLoggingCategoryFilters();

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);

    // we want QtWayland to connect to our Wayland display, but the WaylandBackend to the existing Wayland backend
    // so fiddling around with the env variables.
    const QByteArray systemDisplay = qgetenv("WAYLAND_DISPLAY");
    qputenv("WAYLAND_DISPLAY", waylandSocket.isEmpty() ? QByteArrayLiteral("wayland-0") : waylandSocket);
    KWin::ApplicationWayland a(argc, argv);
    qputenv("WAYLAND_DISPLAY", systemDisplay);
    a.setupTranslator();

    server->setParent(&a);

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

    a.setStartXwayland(parser.isSet(xwaylandOption));
    a.start();

    return a.exec();
}
