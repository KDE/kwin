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

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <QThread>

using namespace KWayland::Client;

namespace KWin
{
namespace Test
{

static struct {
    ConnectionThread *connection = nullptr;
    EventQueue *queue = nullptr;
    Compositor *compositor = nullptr;
    ServerSideDecorationManager *decoration = nullptr;
    Shell *shell = nullptr;
    ShmPool *shm = nullptr;
    Seat *seat = nullptr;
    PlasmaShell *plasmaShell = nullptr;
    PlasmaWindowManagement *windowManagement = nullptr;
    QThread *thread = nullptr;
} s_waylandConnection;

bool setupWaylandConnection(const QString &socketName, AdditionalWaylandInterfaces flags)
{
    if (s_waylandConnection.connection) {
        return false;
    }
    // setup connection
    s_waylandConnection.connection = new ConnectionThread;
    QSignalSpy connectedSpy(s_waylandConnection.connection, &ConnectionThread::connected);
    if (!connectedSpy.isValid()) {
        return false;
    }
    s_waylandConnection.connection->setSocketName(socketName);

    s_waylandConnection.thread = new QThread(kwinApp());
    s_waylandConnection.connection->moveToThread(s_waylandConnection.thread);
    s_waylandConnection.thread->start();

    s_waylandConnection.connection->initConnection();
    if (!connectedSpy.wait()) {
        return false;
    }

    s_waylandConnection.queue = new EventQueue;
    s_waylandConnection.queue->setup(s_waylandConnection.connection);
    if (!s_waylandConnection.queue->isValid()) {
        return false;
    }

    Registry registry;
    registry.setEventQueue(s_waylandConnection.queue);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    if (!allAnnounced.isValid()) {
        return false;
    }
    registry.create(s_waylandConnection.connection);
    if (!registry.isValid()) {
        return false;
    }
    registry.setup();
    if (!allAnnounced.wait()) {
        return false;
    }

    s_waylandConnection.compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version);
    if (!s_waylandConnection.compositor->isValid()) {
        return false;
    }
    s_waylandConnection.shm = registry.createShmPool(registry.interface(Registry::Interface::Shm).name, registry.interface(Registry::Interface::Shm).version);
    if (!s_waylandConnection.shm->isValid()) {
        return false;
    }
    s_waylandConnection.shell = registry.createShell(registry.interface(Registry::Interface::Shell).name, registry.interface(Registry::Interface::Shell).version);
    if (!s_waylandConnection.shell->isValid()) {
        return false;
    }
    if (flags.testFlag(AdditionalWaylandInterface::Seat)) {
        s_waylandConnection.seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version);
        if (!s_waylandConnection.seat->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::Decoration)) {
        s_waylandConnection.decoration = registry.createServerSideDecorationManager(registry.interface(Registry::Interface::ServerSideDecorationManager).name,
                                                                                    registry.interface(Registry::Interface::ServerSideDecorationManager).version);
        if (!s_waylandConnection.decoration->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::PlasmaShell)) {
        s_waylandConnection.plasmaShell = registry.createPlasmaShell(registry.interface(Registry::Interface::PlasmaShell).name,
                                                                     registry.interface(Registry::Interface::PlasmaShell).version);
        if (!s_waylandConnection.plasmaShell->isValid()) {
            return false;
        }
    }
    if (flags.testFlag(AdditionalWaylandInterface::WindowManagement)) {
        s_waylandConnection.windowManagement = registry.createPlasmaWindowManagement(registry.interface(Registry::Interface::PlasmaWindowManagement).name,
                                                                                     registry.interface(Registry::Interface::PlasmaWindowManagement).version);
        if (!s_waylandConnection.windowManagement->isValid()) {
            return false;
        }
    }

    return true;
}

void destroyWaylandConnection()
{
    delete s_waylandConnection.compositor;
    s_waylandConnection.compositor = nullptr;
    delete s_waylandConnection.windowManagement;
    s_waylandConnection.windowManagement = nullptr;
    delete s_waylandConnection.plasmaShell;
    s_waylandConnection.plasmaShell = nullptr;
    delete s_waylandConnection.decoration;
    s_waylandConnection.decoration = nullptr;
    delete s_waylandConnection.decoration;
    s_waylandConnection.decoration = nullptr;
    delete s_waylandConnection.seat;
    s_waylandConnection.seat = nullptr;
    delete s_waylandConnection.shell;
    s_waylandConnection.shell = nullptr;
    delete s_waylandConnection.shm;
    s_waylandConnection.shm = nullptr;
    delete s_waylandConnection.queue;
    s_waylandConnection.queue = nullptr;
    if (s_waylandConnection.thread) {
        QSignalSpy spy(s_waylandConnection.connection, &QObject::destroyed);
        s_waylandConnection.connection->deleteLater();
        QVERIFY(spy.wait());
        s_waylandConnection.thread->quit();
        s_waylandConnection.thread->wait();
        delete s_waylandConnection.thread;
        s_waylandConnection.thread = nullptr;
        s_waylandConnection.connection = nullptr;
    }
}

ConnectionThread *waylandConnection()
{
    return s_waylandConnection.connection;
}

Compositor *waylandCompositor()
{
    return s_waylandConnection.compositor;
}

Shell *waylandShell()
{
    return s_waylandConnection.shell;
}

ShmPool *waylandShmPool()
{
    return s_waylandConnection.shm;
}

Seat *waylandSeat()
{
    return s_waylandConnection.seat;
}

ServerSideDecorationManager *waylandServerSideDecoration()
{
    return s_waylandConnection.decoration;
}

PlasmaShell *waylandPlasmaShell()
{
    return s_waylandConnection.plasmaShell;
}

PlasmaWindowManagement *waylandWindowManagement()
{
    return s_waylandConnection.windowManagement;
}

bool waitForWaylandPointer()
{
    if (!s_waylandConnection.seat) {
        return false;
    }
    QSignalSpy hasPointerSpy(s_waylandConnection.seat, &Seat::hasPointerChanged);
    if (!hasPointerSpy.isValid()) {
        return false;
    }
    return hasPointerSpy.wait();
}

bool waitForWaylandTouch()
{
    if (!s_waylandConnection.seat) {
        return false;
    }
    QSignalSpy hasTouchSpy(s_waylandConnection.seat, &Seat::hasTouchChanged);
    if (!hasTouchSpy.isValid()) {
        return false;
    }
    return hasTouchSpy.wait();
}

void render(Surface *surface, const QSize &size, const QColor &color, const QImage::Format &format)
{
    QImage img(size, format);
    img.fill(color);
    surface->attachBuffer(s_waylandConnection.shm->createBuffer(img));
    surface->damage(QRect(QPoint(0, 0), size));
    surface->commit(Surface::CommitFlag::None);
}

void flushWaylandConnection()
{
    if (s_waylandConnection.connection) {
        s_waylandConnection.connection->flush();
    }
}

Surface *createSurface(QObject *parent)
{
    if (!s_waylandConnection.compositor) {
        return nullptr;
    }
    auto s = s_waylandConnection.compositor->createSurface(parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    return s;
}

ShellSurface *createShellSurface(Surface *surface, QObject *parent)
{
    if (!s_waylandConnection.shell) {
        return nullptr;
    }
    auto s = s_waylandConnection.shell->createSurface(surface, parent);
    if (!s->isValid()) {
        delete s;
        return nullptr;
    }
    return s;
}

}
}
