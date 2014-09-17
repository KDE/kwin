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
#include "compositor_interface.h"
#include "display.h"
#include "surface_interface.h"

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 3;

const struct wl_compositor_interface CompositorInterface::s_interface = {
    CompositorInterface::createSurfaceCallback,
    CompositorInterface::createRegionCallback
};

CompositorInterface::CompositorInterface(Display *display, QObject *parent)
    : QObject(parent)
    , m_display(display)
    , m_compositor(nullptr)
{
}

CompositorInterface::~CompositorInterface()
{
    destroy();
}

void CompositorInterface::create()
{
    Q_ASSERT(!m_compositor);
    m_compositor = wl_global_create(*m_display, &wl_compositor_interface, s_version, this, CompositorInterface::bind);
}

void CompositorInterface::destroy()
{
    if (!m_compositor) {
        return;
    }
    wl_global_destroy(m_compositor);
    m_compositor = nullptr;
}

void CompositorInterface::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    CompositorInterface *compositor = reinterpret_cast<CompositorInterface*>(data);
    compositor->bind(client, version, id);
}

void CompositorInterface::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &CompositorInterface::s_interface, this, CompositorInterface::unbind);
    // TODO: should we track?
}

void CompositorInterface::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

void CompositorInterface::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    CompositorInterface::cast(resource)->createSurface(client, resource, id);
}

void CompositorInterface::createSurface(wl_client *client, wl_resource *resource, uint32_t id)
{
    SurfaceInterface *surface = new SurfaceInterface(this);
    surface->create(client, wl_resource_get_version(resource), id);
    if (!surface->surface()) {
        wl_resource_post_no_memory(resource);
        delete surface;
        return;
    }
    emit surfaceCreated(surface);
}

void CompositorInterface::createRegionCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    CompositorInterface::cast(resource)->createRegion(client, resource, id);
}

void CompositorInterface::createRegion(wl_client *client, wl_resource *resource, uint32_t id)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(id)
    // TODO: implement
}

}
}
