/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "compositor_interface.h"
#include "display.h"
#include "region_interface_p.h"
#include "surface_interface.h"

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
static const int s_version = 5;

class CompositorInterfacePrivate : public QtWaylandServer::wl_compositor
{
public:
    CompositorInterfacePrivate(CompositorInterface *q, Display *display);

    CompositorInterface *q;
    Display *display;

protected:
    void compositor_create_surface(Resource *resource, uint32_t id) override;
    void compositor_create_region(Resource *resource, uint32_t id) override;
};

CompositorInterfacePrivate::CompositorInterfacePrivate(CompositorInterface *q, Display *display)
    : QtWaylandServer::wl_compositor(*display, s_version)
    , q(q)
    , display(display)
{
}

void CompositorInterfacePrivate::compositor_create_surface(Resource *resource, uint32_t id)
{
    wl_resource *surfaceResource = wl_resource_create(resource->client(), &wl_surface_interface, resource->version(), id);
    if (!surfaceResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    Q_EMIT q->surfaceCreated(new SurfaceInterface(q, surfaceResource));
}

void CompositorInterfacePrivate::compositor_create_region(Resource *resource, uint32_t id)
{
    wl_resource *regionResource = wl_resource_create(resource->client(), &wl_region_interface, resource->version(), id);
    if (!regionResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    new RegionInterface(regionResource);
}

CompositorInterface::CompositorInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new CompositorInterfacePrivate(this, display))
{
}

CompositorInterface::~CompositorInterface()
{
}

Display *CompositorInterface::display() const
{
    return d->display;
}

} // namespace KWaylandServer
