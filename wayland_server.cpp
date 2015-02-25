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
#include "screens.h"
#include "toplevel.h"
#include "workspace.h"

#include <KWayland/Server/compositor_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>

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
    m_display->createShm();
    m_seat = m_display->createSeat(m_display);
    m_seat->create();
}

void WaylandServer::initOutputs()
{
    Screens *s = screens();
    Q_ASSERT(s);
    for (int i = 0; i < s->count(); ++i) {
        OutputInterface *output = m_display->createOutput(m_display);
        // TODO: fixme
        output->setPhysicalSize(QSize(269, 168));
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
    return sx[1];
}

}
