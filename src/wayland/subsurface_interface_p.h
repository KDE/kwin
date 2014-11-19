/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef WAYLAND_SERVER_SUBSURFACE_INTERFACE_P_H
#define WAYLAND_SERVER_SUBSURFACE_INTERFACE_P_H

#include "subcompositor_interface.h"
#include "resource_p.h"
// Qt
#include <QPoint>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class SubSurfaceInterface::Private : public Resource::Private
{
public:
    Private(SubSurfaceInterface *q, SubCompositorInterface *compositor);
    ~Private();

    using Resource::Private::create;
    void create(ClientConnection *client, quint32 version, quint32 id, SurfaceInterface *surface, SurfaceInterface *parent);
    void commit();

    QPoint pos = QPoint(0, 0);
    QPoint scheduledPos = QPoint();
    bool scheduledPosChange = false;
    Mode mode = Mode::Synchronized;
    QPointer<SurfaceInterface> surface;
    QPointer<SurfaceInterface> parent;

private:
    SubSurfaceInterface *q_func() {
        return reinterpret_cast<SubSurfaceInterface *>(q);
    }
    void setMode(Mode mode);
    void setPosition(const QPoint &pos);
    void placeAbove(SurfaceInterface *sibling);
    void placeBelow(SurfaceInterface *sibling);

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void setPositionCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y);
    static void placeAboveCallback(wl_client *client, wl_resource *resource, wl_resource *sibling);
    static void placeBelowCallback(wl_client *client, wl_resource *resource, wl_resource *sibling);
    static void setSyncCallback(wl_client *client, wl_resource *resource);
    static void setDeSyncCallback(wl_client *client, wl_resource *resource);

    static const struct wl_subsurface_interface s_interface;
};

}
}

#endif
