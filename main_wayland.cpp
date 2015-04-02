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
#include "fb_backend.h"
#include "wayland_backend.h"
#include "wayland_server.h"
#include "xcbutils.h"
#include "x11windowed_backend.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
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
#include <QWindow>

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

static int startXServer(int waylandSocket, int wmFd);
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
    waylandServer()->createInternalConnection();

    // try creating the Wayland Backend
    createInput();
    createBackend();
    if (dynamic_cast<X11WindowedBackend*>(waylandServer()->backend())) {
        continueStartupWithScreens();
    }
}

void ApplicationWayland::createBackend()
{
    AbstractBackend *backend = nullptr;
    if (m_windowed) {
        if (!m_waylandDisplay.isEmpty()) {
            Wayland::WaylandBackend *b = new Wayland::WaylandBackend(m_waylandDisplay, this);
            connect(b, &Wayland::WaylandBackend::connectionFailed, this,
                [] () {
                    fputs(i18n("kwin_wayland: could not connect to Wayland Server, ensure WAYLAND_DISPLAY is set.\n").toLocal8Bit().constData(), stderr);
                    ::exit(1);
                }
            );
            connect(b, &Wayland::WaylandBackend::outputsChanged, this, &ApplicationWayland::continueStartupWithScreens);
            backend = b;
        }
        if (!backend && !m_x11Display.isEmpty()) {
            KWin::X11WindowedBackend *x11Backend = new KWin::X11WindowedBackend(m_x11Display, m_backendSize, this);
            if (x11Backend->isValid()) {
                backend = x11Backend;
                waylandServer()->seat()->setHasPointer(true);
                waylandServer()->seat()->setHasKeyboard(true);
            } else {
                delete x11Backend;
            }
        }
    }
    if (!m_framebuffer.isEmpty()) {
        FramebufferBackend *b = new FramebufferBackend(this);
        connect(b, &FramebufferBackend::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);
        b->setDevice(m_framebuffer);
        b->init();
        backend = b;
    }

    if (!backend) {
        std::cerr <<  "FATAL ERROR: could not create a backend, exiting now" << std::endl;
        ::exit(1);
    }
}

void ApplicationWayland::continueStartupWithScreens()
{
    if (Wayland::WaylandBackend *w = dynamic_cast<Wayland::WaylandBackend *>(waylandServer()->backend())) {
        disconnect(w, &Wayland::WaylandBackend::outputsChanged, this, &ApplicationWayland::continueStartupWithScreens);
    }
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

    const int fd = waylandServer()->createXWaylandConnection();
    if (fd == -1) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        exit(1);
        return;
    }

    m_xcbConnectionFd = sx[0];
    const int xDisplayPipe = startXServer(fd, sx[1]);
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

    // HACK: create a QWindow in a thread to force QtWayland to create the client buffer integration
    // this performs an eglInitialize which would block as it does a roundtrip to the Wayland server
    // in the main thread. By moving into a thread we get the initialize without hitting the problem
    // This needs to be done before creating the Workspace as from inside Workspace the dangerous code
    // gets hit in the main thread
    QFutureWatcher<void> *eglInitWatcher = new QFutureWatcher<void>(this);
    connect(eglInitWatcher, &QFutureWatcher<void>::finished, this,
        [this, eglInitWatcher] {
            eglInitWatcher->deleteLater();

            createWorkspace();

            Xcb::sync(); // Trigger possible errors, there's still a chance to abort

            notifyKSplash();
        }
    );
    eglInitWatcher->setFuture(QtConcurrent::run([] {
        QWindow w;
        w.setSurfaceType(QSurface::RasterGLSurface);
        w.create();
    }));
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
static int startXServer(int waylandSocket, int wmFd)
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

        int wlfd = dup(waylandSocket);
        if (wlfd < 0) {
            std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
            exit(20);
            return -1;
        }
        qputenv("WAYLAND_SOCKET", QByteArray::number(wlfd));
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
    // ensure that no thread takes SIGUSR
    sigset_t userSiganls;
    sigemptyset(&userSiganls);
    sigaddset(&userSiganls, SIGUSR1);
    sigaddset(&userSiganls, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &userSiganls, nullptr);

    // enforce wayland plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "wayland", true);
#if (QT_VERSION < QT_VERSION_CHECK(5, 4, 2))
    // TODO: remove warning once we depend on Qt 5.5
    qWarning() << "QtWayland 5.4.2 required, application might freeze if not present!";
#endif

    qputenv("WAYLAND_SOCKET", QByteArray::number(server->createQtConnection()));
    KWin::ApplicationWayland a(argc, argv);
    a.setupTranslator();

    server->setParent(&a);

    KWin::Application::createAboutData();

    QCommandLineOption xwaylandOption(QStringLiteral("xwayland"),
                                      i18n("Start a rootless Xwayland server."));
    QCommandLineOption waylandSocketOption(QStringList{QStringLiteral("s"), QStringLiteral("socket")},
                                           i18n("Name of the Wayland socket to listen on. If not set \"wayland-0\" is used."),
                                           QStringLiteral("socket"));
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
                                      i18n("Use a nested compositor in windowed mode."));
    QCommandLineOption framebufferOption(QStringLiteral("framebuffer"),
                                         i18n("Render to framebuffer."));
    QCommandLineOption framebufferDeviceOption(QStringLiteral("fb-device"),
                                               i18n("The framebuffer device to render to."),
                                               QStringLiteral("fbdev"));
    framebufferDeviceOption.setDefaultValue(QStringLiteral("/dev/fb0"));
    QCommandLineOption x11DisplayOption(QStringLiteral("x11-display"),
                                        i18n("The X11 Display to use in windowed mode on platform X11."),
                                        QStringLiteral("display"));
    QCommandLineOption waylandDisplayOption(QStringLiteral("wayland-display"),
                                            i18n("The Wayland Display to use in windowed mode on platform Wayland."),
                                            QStringLiteral("display"));
    QCommandLineOption widthOption(QStringLiteral("width"),
                                   i18n("The width for windowed mode. Default width is 1024."),
                                   QStringLiteral("width"));
    widthOption.setDefaultValue(QString::number(1024));
    QCommandLineOption heightOption(QStringLiteral("height"),
                                    i18n("The height for windowed mode. Default height is 768."),
                                    QStringLiteral("height"));
    heightOption.setDefaultValue(QString::number(768));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(xwaylandOption);
    parser.addOption(waylandSocketOption);
    parser.addOption(windowedOption);
    parser.addOption(x11DisplayOption);
    parser.addOption(waylandDisplayOption);
    parser.addOption(framebufferOption);
    parser.addOption(framebufferDeviceOption);
    parser.addOption(widthOption);
    parser.addOption(heightOption);
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

    if (parser.isSet(windowedOption) && parser.isSet(framebufferOption)) {
        std::cerr << "FATAL ERROR Cannot have both --windowed and --framebuffer" << std::endl;
        return 1;
    }

    a.setWindowed(parser.isSet(windowedOption));
    if (parser.isSet(windowedOption)) {
        bool ok = false;
        const int width = parser.value(widthOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for width" << std::endl;
            return 1;
        }
        const int height = parser.value(heightOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for height" << std::endl;
            return 1;
        }
        a.setBackendSize(QSize(width, height));
        if (parser.isSet(x11DisplayOption)) {
            a.setX11Display(parser.value(x11DisplayOption).toUtf8());
        } else if (!parser.isSet(waylandDisplayOption)) {
            a.setX11Display(qgetenv("DISPLAY"));
        }
        if (parser.isSet(waylandDisplayOption)) {
            a.setWaylandDisplay(parser.value(waylandDisplayOption).toUtf8());
        } else if (!parser.isSet(x11DisplayOption)) {
            a.setWaylandDisplay(qgetenv("WAYLAND_DISPLAY"));
        }
    }
    if (parser.isSet(framebufferOption)) {
        a.setFramebuffer(parser.value(framebufferDeviceOption));
    }

    a.setStartXwayland(parser.isSet(xwaylandOption));
    a.start();

    return a.exec();
}
