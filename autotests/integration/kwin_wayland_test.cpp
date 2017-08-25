/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "kwin_wayland_test.h"
#include "../../platform.h"
#include "../../composite.h"
#include "../../effects.h"
#include "../../wayland_server.h"
#include "../../workspace.h"
#include "../../xcbutils.h"

#include <KPluginMetaData>

#include <QAbstractEventDispatcher>
#include <QPluginLoader>
#include <QSocketNotifier>
#include <QStyle>
#include <QThread>
#include <QtConcurrentRun>

// system
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>

namespace KWin
{

static void readDisplay(int pipe);

WaylandTestApplication::WaylandTestApplication(OperationMode mode, int &argc, char **argv)
    : Application(mode, argc, argv)
{
    QStandardPaths::setTestModeEnabled(true);
#ifdef KWIN_BUILD_ACTIVITIES
    setUseKActivities(false);
#endif
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));
    initPlatform(KPluginMetaData(QStringLiteral("KWinWaylandVirtualBackend.so")));
    WaylandServer::create(this);
}

WaylandTestApplication::~WaylandTestApplication()
{
    kwinApp()->platform()->setOutputsEnabled(false);
    // need to unload all effects prior to destroying X connection as they might do X calls
    // also before destroy Workspace, as effects might call into Workspace
    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->unloadAllEffects();
    }
    destroyWorkspace();
    waylandServer()->dispatch();
    disconnect(m_xwaylandFailConnection);
    if (x11Connection()) {
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
        destroyAtoms();
        emit x11ConnectionAboutToBeDestroyed();
        xcb_disconnect(x11Connection());
        setX11Connection(nullptr);
    }
    if (m_xwaylandProcess) {
        m_xwaylandProcess->terminate();
        while (m_xwaylandProcess->state() != QProcess::NotRunning) {
            processEvents(QEventLoop::WaitForMoreEvents);
        }
        waylandServer()->destroyXWaylandConnection();
    }
    if (QStyle *s = style()) {
        s->unpolish(this);
    }
    waylandServer()->terminateClientConnections();
    destroyCompositor();
}

void WaylandTestApplication::performStartup()
{
    // first load options - done internally by a different thread
    createOptions();
    waylandServer()->createInternalConnection();

    // try creating the Wayland Backend
    createInput();
    createBackend();
}

void WaylandTestApplication::createBackend()
{
    Platform *platform = kwinApp()->platform();
    connect(platform, &Platform::screensQueried, this, &WaylandTestApplication::continueStartupWithScreens);
    connect(platform, &Platform::initFailed, this,
        [] () {
            std::cerr <<  "FATAL ERROR: backend failed to initialize, exiting now" << std::endl;
            ::exit(1);
        }
    );
    platform->init();
}

void WaylandTestApplication::continueStartupWithScreens()
{
    disconnect(kwinApp()->platform(), &Platform::screensQueried, this, &WaylandTestApplication::continueStartupWithScreens);
    createScreens();

    if (operationMode() == OperationModeWaylandOnly) {
        createCompositor();
        connect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithSceen);
        return;
    }
    createCompositor();
    connect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::startXwaylandServer);
}

void WaylandTestApplication::continueStartupWithSceen()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::continueStartupWithSceen);
    createWorkspace();
}

void WaylandTestApplication::continueStartupWithX()
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
            long result = 0;
            if (QThread::currentThread()->eventDispatcher()->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result)) {
                free(event);
                continue;
            }
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
        ::exit(1);
    }

    createWorkspace();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort
}

void WaylandTestApplication::createX11Connection()
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

void WaylandTestApplication::startXwaylandServer()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &WaylandTestApplication::startXwaylandServer);
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start Xwayland " << std::endl;
        exit(1);
        return;
    }
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        exit(1);
        return;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        exit(20);
        return;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        exit(1);
        return;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        exit(20);
        return;
    }

    m_xcbConnectionFd = sx[0];

    m_xwaylandProcess = new QProcess(kwinApp());
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments({QStringLiteral("-displayfd"),
                           QString::number(pipeFds[1]),
                           QStringLiteral("-rootless"),
                           QStringLiteral("-wm"),
                           QString::number(fd)});
    m_xwaylandFailConnection = connect(m_xwaylandProcess, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this,
        [] (QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                std::cerr << "FATAL ERROR: failed to start Xwayland" << std::endl;
            } else {
                std::cerr << "FATAL ERROR: Xwayland failed, going to exit now" << std::endl;
            }
            exit(1);
        }
    );
    const int xDisplayPipe = pipeFds[0];
    connect(m_xwaylandProcess, &QProcess::started, this,
        [this, xDisplayPipe] {
            QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &WaylandTestApplication::continueStartupWithX, Qt::QueuedConnection);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, &QFutureWatcher<void>::deleteLater, Qt::QueuedConnection);
            watcher->setFuture(QtConcurrent::run(readDisplay, xDisplayPipe));
        }
    );
    m_xwaylandProcess->start();
    close(pipeFds[1]);
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

}
