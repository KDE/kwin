/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "compositor_interface.h"
#include "display.h"
#include "global_p.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class CompositorInterface::Private : public Global::Private
{
public:
    Private(CompositorInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createSurface(wl_client *client, wl_resource *resource, uint32_t id);
    void createRegion(wl_client *client, wl_resource *resource, uint32_t id);

    static void unbind(wl_resource *resource);
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void createRegionCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    CompositorInterface *q;
    static const struct wl_compositor_interface s_interface;
    static const quint32 s_version;
};

const quint32 CompositorInterface::Private::s_version = 4;

CompositorInterface::Private::Private(CompositorInterface *q, Display *d)
    : Global::Private(d, &wl_compositor_interface, s_version)
    , q(q)
{
}

#ifndef K_DOXYGEN
const struct wl_compositor_interface CompositorInterface::Private::s_interface = {
    createSurfaceCallback,
    createRegionCallback
};
#endif

CompositorInterface::CompositorInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

CompositorInterface::~CompositorInterface() = default;

void CompositorInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wl_compositor_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void CompositorInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

void CompositorInterface::Private::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->createSurface(client, resource, id);
}

void CompositorInterface::Private::createSurface(wl_client *client, wl_resource *resource, uint32_t id)
{
    SurfaceInterface *surface = new SurfaceInterface(q, resource);
    surface->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!surface->resource()) {
        wl_resource_post_no_memory(resource);
        delete surface;
        return;
    }
    emit q->surfaceCreated(surface);
}

void CompositorInterface::Private::createRegionCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->createRegion(client, resource, id);
}

void CompositorInterface::Private::createRegion(wl_client *client, wl_resource *resource, uint32_t id)
{
    RegionInterface *region = new RegionInterface(q, resource);
    region->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!region->resource()) {
        wl_resource_post_no_memory(resource);
        delete region;
        return;
    }
    emit q->regionCreated(region);
}

}
}
