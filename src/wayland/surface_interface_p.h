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
#ifndef WAYLAND_SERVER_SURFACE_INTERFACE_P_H
#define WAYLAND_SERVER_SURFACE_INTERFACE_P_H

#include "surface_interface.h"
#include "resource_p.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class SurfaceInterface::Private : public Resource::Private
{
public:
    struct State {
        QRegion damage = QRegion();
        QRegion opaque = QRegion();
        QRegion input = QRegion();
        bool inputIsSet = false;
        bool opaqueIsSet = false;
        bool inputIsInfinite = true;
        qint32 scale = 1;
        OutputInterface::Transform transform = OutputInterface::Transform::Normal;
        QList<wl_resource*> callbacks = QList<wl_resource*>();
        QPoint offset = QPoint();
        BufferInterface *buffer = nullptr;
        // stacking order: bottom (first) -> top (last)
        QList<QPointer<SubSurfaceInterface>> children;
    };
    Private(SurfaceInterface *q, CompositorInterface *c);
    ~Private();

    void destroy();

    void addChild(QPointer<SubSurfaceInterface> subsurface);
    void removeChild(QPointer<SubSurfaceInterface> subsurface);
    bool raiseChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling);
    bool lowerChild(QPointer<SubSurfaceInterface> subsurface, SurfaceInterface *sibling);

    State current;
    State pending;
    QPointer<SubSurfaceInterface> subSurface;

private:
    SurfaceInterface *q_func() {
        return reinterpret_cast<SurfaceInterface *>(q);
    }
    void commit();
    void damage(const QRect &rect);
    void setScale(qint32 scale);
    void setTransform(OutputInterface::Transform transform);
    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource *buffer, const QPoint &offset);
    void setOpaque(const QRegion &region);
    void setInput(const QRegion &region, bool isInfinite);

    static void destroyFrameCallback(wl_resource *r);

    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy);
    static void damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback);
    static void opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void commitCallback(wl_client *client, wl_resource *resource);
    // since version 2
    static void bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform);
    // since version 3
    static void bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale);

    static const struct wl_surface_interface s_interface;
};

}
}

#endif
