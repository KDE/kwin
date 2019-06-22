/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2014 Martin Gräßlin <mgraesslin@kde.org>
Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
#include "xwayland.h"
#include "databridge.h"

#include "wayland_server.h"
#include "main_wayland.h"
#include "utils.h"

#include <KLocalizedString>
#include <KSelectionOwner>

#include "xcbutils.h"

#include <QAbstractEventDispatcher>
#include <QtConcurrentRun>
#include <QFile>
#include <QFutureWatcher>
#include <QProcess>
#include <QSocketNotifier>
#include <QThread>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#endif

#include <sys/socket.h>
#include <iostream>

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

namespace KWin {
namespace Xwl
{

Xwayland *s_self = nullptr;
Xwayland* Xwayland::self()
{
    return s_self;
}

Xwayland::Xwayland(ApplicationWaylandAbstract *app, QObject *parent)
    : XwaylandInterface(parent),
      m_app(app)
{
    s_self = this;
}

Xwayland::~Xwayland()
{
    disconnect(m_xwaylandFailConnection);
    if (m_app->x11Connection()) {
        Xcb::setInputFocus(XCB_INPUT_FOCUS_POINTER_ROOT);
        m_app->destroyAtoms();
        Q_EMIT m_app->x11ConnectionAboutToBeDestroyed();
        xcb_disconnect(m_app->x11Connection());
        m_app->setX11Connection(nullptr);
    }

    if (m_xwaylandProcess) {
        m_xwaylandProcess->terminate();
        while (m_xwaylandProcess->state() != QProcess::NotRunning) {
            m_app->processEvents(QEventLoop::WaitForMoreEvents);
        }
        waylandServer()->destroyXWaylandConnection();
    }
    s_self = nullptr;
}

void Xwayland::init()
{
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start Xwayland " << std::endl;
        Q_EMIT criticalError(1);
        return;
    }
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        Q_EMIT criticalError(1);
        return;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket to open XCB connection" << std::endl;
        Q_EMIT criticalError(20);
        return;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        Q_EMIT criticalError(1);
        return;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        std::cerr << "FATAL ERROR: failed to open socket for Xwayland" << std::endl;
        Q_EMIT criticalError(20);
        return;
    }

    m_xcbConnectionFd = sx[0];

    m_xwaylandProcess = new Process(this);
    m_xwaylandProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    QProcessEnvironment env = m_app->processStartupEnvironment();
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    env.insert("EGL_PLATFORM", QByteArrayLiteral("DRM"));
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments({QStringLiteral("-displayfd"),
                           QString::number(pipeFds[1]),
                           QStringLiteral("-rootless"),
                           QStringLiteral("-wm"),
                           QString::number(fd)});
    m_xwaylandFailConnection = connect(m_xwaylandProcess, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error), this,
        [this] (QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                std::cerr << "FATAL ERROR: failed to start Xwayland" << std::endl;
            } else {
                std::cerr << "FATAL ERROR: Xwayland failed, going to exit now" << std::endl;
            }
            Q_EMIT criticalError(1);
        }
    );
    const int xDisplayPipe = pipeFds[0];
    connect(m_xwaylandProcess, &QProcess::started, this,
        [this, xDisplayPipe] {
            QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, this, &Xwayland::continueStartupWithX, Qt::QueuedConnection);
            QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, &QFutureWatcher<void>::deleteLater, Qt::QueuedConnection);
            watcher->setFuture(QtConcurrent::run(readDisplay, xDisplayPipe));
        }
    );
    m_xwaylandProcess->start();
    close(pipeFds[1]);
}

void Xwayland::prepareDestroy()
{
    delete m_dataBridge;
    m_dataBridge = nullptr;
}

void Xwayland::createX11Connection()
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
        Q_EMIT criticalError(1);
        return;
    }

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(c));
    m_xcbScreen = iter.data;
    Q_ASSERT(m_xcbScreen);

    m_app->setX11Connection(c);
    // we don't support X11 multi-head in Wayland
    m_app->setX11ScreenNumber(screenNumber);
    m_app->setX11RootWindow(defaultScreen()->root);
}

void Xwayland::continueStartupWithX()
{
    createX11Connection();
    auto *xcbConn = m_app->x11Connection();
    if (!xcbConn) {
        // about to quit
        Q_EMIT criticalError(1);
        return;
    }
    QSocketNotifier *notifier = new QSocketNotifier(xcb_get_file_descriptor(xcbConn), QSocketNotifier::Read, this);
    auto processXcbEvents = [this, xcbConn] {
        while (auto event = xcb_poll_for_event(xcbConn)) {
            if (m_dataBridge->filterEvent(event)) {
                free(event);
                continue;
            }
            long result = 0;
            QThread::currentThread()->eventDispatcher()->filterNativeEvent(QByteArrayLiteral("xcb_generic_event_t"), event, &result);
            free(event);
        }
        xcb_flush(xcbConn);
    };
    connect(notifier, &QSocketNotifier::activated, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QThread::currentThread()->eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);

    xcb_prefetch_extension_data(xcbConn, &xcb_xfixes_id);
    m_xfixes = xcb_get_extension_data(xcbConn, &xcb_xfixes_id);

    // create selection owner for WM_S0 - magic X display number expected by XWayland
    KSelectionOwner owner("WM_S0", xcbConn, m_app->x11RootWindow());
    owner.claim(true);

    m_app->createAtoms();
    m_app->setupEventFilters();

    m_dataBridge = new DataBridge;

    // Check  whether another windowmanager is running
    const uint32_t maskValues[] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    ScopedCPointer<xcb_generic_error_t> redirectCheck(xcb_request_check(connection(),
                                                                        xcb_change_window_attributes_checked(connection(),
                                                                                                             rootWindow(),
                                                                                                             XCB_CW_EVENT_MASK,
                                                                                                             maskValues)));
    if (!redirectCheck.isNull()) {
        fputs(i18n("kwin_wayland: an X11 window manager is running on the X11 Display.\n").toLocal8Bit().constData(), stderr);
        Q_EMIT criticalError(1);
        return;
    }

    auto env = m_app->processStartupEnvironment();
    env.insert(QStringLiteral("DISPLAY"), QString::fromUtf8(qgetenv("DISPLAY")));
    m_app->setProcessStartupEnvironment(env);

    emit initialized();

    Xcb::sync(); // Trigger possible errors, there's still a chance to abort
}

DragEventReply Xwayland::dragMoveFilter(Toplevel *target, QPoint pos)
{
    if (!m_dataBridge) {
        return DragEventReply::Wayland;
    }
    return m_dataBridge->dragMoveFilter(target, pos);
}

}
}
