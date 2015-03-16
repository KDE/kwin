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
#include "shell_client.h"
#include "deleted.h"
#include "wayland_server.h"

#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>

using namespace KWayland::Server;

namespace KWin
{

ShellClient::ShellClient(ShellSurfaceInterface *surface)
    : Toplevel()
    , m_shellSurface(surface)
{
    setSurface(surface->surface());
    setupCompositing();
    if (surface->surface()->buffer()) {
        setReadyForPainting();
        m_clientSize = surface->surface()->buffer()->size();
    } else {
        ready_for_painting = false;
    }
    setGeometry(QRect(QPoint(0, 0), m_clientSize));

    connect(surface->surface(), &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = m_shellSurface->surface()->buffer()->size();
            setGeometry(QRect(QPoint(0, 0), m_clientSize));
        }
    );
    connect(surface, &ShellSurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    connect(surface->surface(), &SurfaceInterface::unmapped, this, &ShellClient::destroyClient);
}

ShellClient::~ShellClient() = default;

void ShellClient::destroyClient()
{
    Deleted *del = Deleted::create(this);
    emit windowClosed(this, del);
    waylandServer()->removeClient(this);

    del->unrefWindow();
    m_shellSurface = nullptr;
    deleteClient(this);
}

void ShellClient::deleteClient(ShellClient *c)
{
    delete c;
}

QStringList ShellClient::activities() const
{
    // TODO: implement
    return QStringList();
}

QPoint ShellClient::clientPos() const
{
    return QPoint(0, 0);
}

QSize ShellClient::clientSize() const
{
    // TODO: connect for changes
    return m_clientSize;
}

void ShellClient::debug(QDebug &stream) const
{
    // TODO: implement
    Q_UNUSED(stream)
}

int ShellClient::desktop() const
{
    // TODO: implement
    return -1;
}

Layer ShellClient::layer() const
{
    // TODO: implement
    return KWin::NormalLayer;
}

bool ShellClient::shouldUnredirect() const
{
    // TODO: unredirect for fullscreen
    return false;
}

QRect ShellClient::transparentRect() const
{
    // TODO: implement
    return QRect();
}

NET::WindowType ShellClient::windowType(bool direct, int supported_types) const
{
    // TODO: implement
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return NET::Normal;
}

double ShellClient::opacity() const
{
    return 1.0;
}

void ShellClient::setOpacity(double opacity)
{
    Q_UNUSED(opacity)
}

void ShellClient::addDamage(const QRegion &damage)
{
    setReadyForPainting();
    if (m_shellSurface->surface()->buffer()->size().isValid()) {
        m_clientSize = m_shellSurface->surface()->buffer()->size();
        setGeometry(QRect(QPoint(0, 0), m_clientSize));
    }
    Toplevel::addDamage(damage);
}

void ShellClient::setGeometry(const QRect &rect)
{
    if (geom == rect) {
        return;
    }
    const QRect old = geom;
    geom = rect;
    emit geometryChanged();
    emit geometryShapeChanged(this, old);
}

QByteArray ShellClient::windowRole() const
{
    return QByteArray();
}

}
