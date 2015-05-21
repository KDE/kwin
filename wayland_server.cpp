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
#include "wayland_server.h"
#include "abstract_backend.h"
#include "composite.h"
#include "screens.h"
#include "shell_client.h"
#include "workspace.h"

// Client
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>
// Server
#include <KWayland/Server/compositor_interface.h>
#include <KWayland/Server/datadevicemanager_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>

// Qt
#include <QWindow>

// system
#include <sys/types.h>
#include <sys/socket.h>

using namespace KWayland::Server;

namespace KWin
{

KWIN_SINGLETON_FACTORY(WaylandServer)

WaylandServer::WaylandServer(QObject *parent)
    : QObject(parent)
{
}

WaylandServer::~WaylandServer() = default;

void WaylandServer::init(const QByteArray &socketName)
{
    m_display = new KWayland::Server::Display(this);
    if (!socketName.isNull() && !socketName.isEmpty()) {
        m_display->setSocketName(QString::fromUtf8(socketName));
    }
    m_display->start();
    m_compositor = m_display->createCompositor(m_display);
    m_compositor->create();
    connect(m_compositor, &CompositorInterface::surfaceCreated, this,
        [this] (SurfaceInterface *surface) {
            // check whether we have a Toplevel with the Surface's id
            Workspace *ws = Workspace::self();
            if (!ws) {
                // it's possible that a Surface gets created before Workspace is created
                return;
            }
            if (surface->client() != xWaylandConnection()) {
                // setting surface is only relevat for Xwayland clients
                return;
            }
            auto check = [surface] (const Toplevel *t) {
                return t->surfaceId() == surface->id();
            };
            if (Toplevel *t = ws->findToplevel(check)) {
                t->setSurface(surface);
            }
        }
    );
    m_shell = m_display->createShell(m_display);
    m_shell->create();
    connect(m_shell, &ShellInterface::surfaceCreated, this,
        [this] (ShellSurfaceInterface *surface) {
            if (!Workspace::self()) {
                // it's possible that a Surface gets created before Workspace is created
                return;
            }
            if (surface->client() == m_xwaylandConnection) {
                // skip Xwayland clients, those are created using standard X11 way
                return;
            }
            if (surface->client() == m_qtConnection) {
                // one of Qt's windows
                if (m_dummyWindowSurface && (m_dummyWindowSurface->id() == surface->surface()->id())) {
                    fakeDummyQtWindowInput();
                    return;
                }
                // HACK: in order to get Qt to not block for frame rendered, we immediatelly emit the
                // frameRendered once we get a new damage event.
                auto s = surface->surface();
                connect(s, &SurfaceInterface::damaged, this, [this, s] {
                    s->frameRendered(0);
                    m_qtConnection->flush();
                });
            }
            auto client = new ShellClient(surface);
            if (auto c = Compositor::self()) {
                connect(client, &Toplevel::needsRepaint, c, &Compositor::scheduleRepaint);
            }
            m_clients << client;
            emit shellClientAdded(client);
        }
    );
    m_display->createShm();
    m_seat = m_display->createSeat(m_display);
    m_seat->create();
    m_display->createDataDeviceManager(m_display)->create();
}

void WaylandServer::initOutputs()
{
    if (m_backend && m_backend->handlesOutputs()) {
        return;
    }
    Screens *s = screens();
    Q_ASSERT(s);
    for (int i = 0; i < s->count(); ++i) {
        OutputInterface *output = m_display->createOutput(m_display);
        output->setPhysicalSize(s->size(i) / 3.8);
        output->addMode(s->size(i));
        output->create();
    }
}

int WaylandServer::createXWaylandConnection()
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return -1;
    }
    m_xwaylandConnection = m_display->createClient(sx[0]);
    connect(m_xwaylandConnection, &KWayland::Server::ClientConnection::disconnected, this,
        [] {
            qFatal("Xwayland Connection died");
        }
    );
    return sx[1];
}

int WaylandServer::createQtConnection()
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return -1;
    }
    m_qtConnection = m_display->createClient(sx[0]);
    return sx[1];
}

void WaylandServer::createInternalConnection()
{
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return;
    }
    m_internalConnection.server = m_display->createClient(sx[0]);
    using namespace KWayland::Client;
    m_internalConnection.client = new ConnectionThread(this);
    m_internalConnection.client->setSocketFd(sx[1]);
    connect(m_internalConnection.client, &ConnectionThread::connected, this,
        [this] {
            Registry *registry = new Registry(m_internalConnection.client);
            registry->create(m_internalConnection.client);
            connect(registry, &Registry::shmAnnounced, this,
                [this, registry] (quint32 name, quint32 version) {
                    m_internalConnection.shm = registry->createShmPool(name, version, m_internalConnection.client);
                }
            );
            registry->setup();
        }
    );
    m_internalConnection.client->initConnection();
}

void WaylandServer::installBackend(AbstractBackend *backend)
{
    Q_ASSERT(!m_backend);
    m_backend = backend;
}

void WaylandServer::uninstallBackend(AbstractBackend *backend)
{
    Q_ASSERT(m_backend == backend);
    m_backend = nullptr;
}

void WaylandServer::removeClient(ShellClient *c)
{
    m_clients.removeAll(c);
    emit shellClientRemoved(c);
}

void WaylandServer::createDummyQtWindow()
{
    if (m_dummyWindow) {
        return;
    }
    m_dummyWindow.reset(new QWindow());
    m_dummyWindow->setSurfaceType(QSurface::RasterSurface);
    m_dummyWindow->show();
    m_dummyWindowSurface = KWayland::Client::Surface::fromWindow(m_dummyWindow.data());
}

void WaylandServer::fakeDummyQtWindowInput()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    // we need to fake Qt into believing it has got any seat events
    // this is done only when receiving either a key press or button.
    // we simulate by sending a button press and release
    auto surface = KWayland::Server::SurfaceInterface::get(m_dummyWindowSurface->id(), m_qtConnection);
    if (!surface) {
        return;
    }
    const auto oldSeatSurface = m_seat->focusedPointerSurface();
    const auto oldPos = m_seat->focusedPointerSurfacePosition();
    m_seat->setFocusedPointerSurface(surface, QPoint(0, 0));
    m_seat->setPointerPos(QPointF(0, 0));
    m_seat->pointerButtonPressed(Qt::LeftButton);
    m_seat->pointerButtonReleased(Qt::LeftButton);
    m_qtConnection->flush();
    m_dummyWindow->hide();
    m_seat->setFocusedPointerSurface(oldSeatSurface, oldPos);
#endif
}

void WaylandServer::dispatch()
{
    if (!m_display) {
        return;
    }
    if (!m_qtClientConnection) {
        if (m_qtConnection && QGuiApplication::instance()) {
            m_qtClientConnection = KWayland::Client::ConnectionThread::fromApplication(this);
        }
    }
    if (m_qtClientConnection) {
        m_qtClientConnection->flush();
    }
    m_display->dispatchEvents(0);
}

ShellClient *WaylandServer::findClient(quint32 id) const
{
    if (id == 0) {
        return nullptr;
    }
    auto it = std::find_if(m_clients.constBegin(), m_clients.constEnd(),
        [id] (ShellClient *c) {
            return c->windowId() == id;
        }
    );
    if (it == m_clients.constEnd()) {
        return nullptr;
    }
    return *it;
}

quint32 WaylandServer::createWindowId(SurfaceInterface *surface)
{
    auto it = m_clientIds.constFind(surface->client());
    quint16 clientId = 0;
    if (it != m_clientIds.constEnd()) {
        clientId = it.value();
    } else {
        clientId = createClientId(surface->client());
    }
    Q_ASSERT(clientId != 0);
    quint32 id = clientId;
    // TODO: this does not prevent that two surfaces of same client get same id
    id = (id << 16) | (surface->id() & 0xFFFF);
    if (findClient(id)) {
        qCWarning(KWIN_CORE) << "Invalid client windowId generated:" << id;
        return 0;
    }
    return id;
}

quint16 WaylandServer::createClientId(ClientConnection *c)
{
    auto ids = m_clientIds.values().toSet();
    quint16 id = 1;
    if (!ids.isEmpty()) {
        for (quint16 i = ids.count() + 1; i >= 1 ; i--) {
            if (!ids.contains(i)) {
                id = i;
                break;
            }
        }
    }
    Q_ASSERT(!ids.contains(id));
    m_clientIds.insert(c, id);
    connect(c, &ClientConnection::disconnected, this,
        [this] (ClientConnection *c) {
            m_clientIds.remove(c);
        }
    );
    return id;
}

}
