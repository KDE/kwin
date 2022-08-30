/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "compositor_interface.h"
#include "display.h"
#include "region_interface_p.h"
#include "surface_interface.h"

#include <wayland-server-protocol.h>

namespace KWaylandServer
{

static const int s_version = 5;

class CompositorInterfacePrivate
{
public:
    CompositorInterfacePrivate(CompositorInterface *q, Display *display);
    ~CompositorInterfacePrivate();

    static CompositorInterfacePrivate *fromResource(wl_resource *resource);

    CompositorInterface *q;
    wl_global *global;
    Display *display;

    static void compositor_create_surface(wl_client *client, wl_resource *resource, uint32_t id);
    static void compositor_create_region(wl_client *client, wl_resource *resource, uint32_t id);

    static constexpr struct wl_compositor_interface implementation = {
        .create_surface = compositor_create_surface,
        .create_region = compositor_create_region,
    };
};

CompositorInterfacePrivate *CompositorInterfacePrivate::fromResource(wl_resource *resource)
{
    Q_ASSERT(wl_resource_instance_of(resource, &wl_compositor_interface, &implementation));
    return static_cast<CompositorInterfacePrivate *>(wl_resource_get_user_data(resource));
}

CompositorInterfacePrivate::CompositorInterfacePrivate(CompositorInterface *q, Display *display)
    : q(q)
    , display(display)
{
    global = wl_global_create(*display, &wl_compositor_interface, s_version, this, [](wl_client *client, void *data, uint32_t version, uint32_t id) {
        wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, version, id);
        wl_resource_set_implementation(resource, &implementation, data, nullptr);
    });
}

CompositorInterfacePrivate::~CompositorInterfacePrivate()
{
    if (global) {
        wl_global_destroy(global);
        global = nullptr;
    }
}

void CompositorInterfacePrivate::compositor_create_surface(wl_client *client, wl_resource *resource, uint32_t id)
{
    auto compositorPrivate = CompositorInterfacePrivate::fromResource(resource);
    auto surface = new SurfaceInterface(compositorPrivate->q, client, wl_resource_get_version(resource), id);
    Q_EMIT compositorPrivate->q->surfaceCreated(surface);
}

void CompositorInterfacePrivate::compositor_create_region(wl_client *client, wl_resource *resource, uint32_t id)
{
    new RegionInterface(client, wl_resource_get_version(resource), id);
}

CompositorInterface::CompositorInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new CompositorInterfacePrivate(this, display))
{
    connect(display, &Display::aboutToTerminate, this, [this]() {
        wl_global_destroy(d->global);
        d->global = nullptr;
    });
}

CompositorInterface::~CompositorInterface()
{
}

Display *CompositorInterface::display() const
{
    return d->display;
}

} // namespace KWaylandServer
