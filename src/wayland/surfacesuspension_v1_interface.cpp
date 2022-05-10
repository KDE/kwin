/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "surfacesuspension_v1_interface.h"
#include "display.h"
#include "surface_interface_p.h"
#include "surfacesuspension_v1_interface_p.h"

namespace KWaylandServer
{

static const int s_version = 1;

class SurfaceSuspensionManagerV1InterfacePrivate : public QtWaylandServer::wp_surface_suspension_manager_v1
{
public:
    explicit SurfaceSuspensionManagerV1InterfacePrivate(Display *display);

protected:
    void wp_surface_suspension_manager_v1_destroy(Resource *resource) override;
    void wp_surface_suspension_manager_v1_get_surface_suspension(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource) override;
};

SurfaceSuspensionManagerV1InterfacePrivate::SurfaceSuspensionManagerV1InterfacePrivate(Display *display)
    : QtWaylandServer::wp_surface_suspension_manager_v1(*display, s_version)
{
}

void SurfaceSuspensionManagerV1InterfacePrivate::wp_surface_suspension_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SurfaceSuspensionManagerV1InterfacePrivate::wp_surface_suspension_manager_v1_get_surface_suspension(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);
    SurfaceSuspensionV1Interface *suspension = SurfaceSuspensionV1Interface::get(surface);

    suspension->add(resource->client(), id, resource->version());
}

SurfaceSuspensionManagerV1Interface::SurfaceSuspensionManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new SurfaceSuspensionManagerV1InterfacePrivate(display))
{
}

SurfaceSuspensionManagerV1Interface::~SurfaceSuspensionManagerV1Interface()
{
}

SurfaceSuspensionV1Interface::SurfaceSuspensionV1Interface(SurfaceInterface *surface)
    : m_surface(surface)
{
}

bool SurfaceSuspensionV1Interface::isSuspended() const
{
    return m_suspended;
}

void SurfaceSuspensionV1Interface::setSuspended(bool suspended)
{
    if (m_suspended == suspended) {
        return;
    }

    m_suspended = suspended;

    const auto resources = resourceMap();
    for (Resource *resource : resources) {
        if (suspended) {
            send_suspended(resource->handle);
        } else {
            send_resumed(resource->handle);
        }
    }
}

SurfaceSuspensionV1Interface *SurfaceSuspensionV1Interface::get(SurfaceInterface *surface)
{
    return SurfaceInterfacePrivate::get(surface)->suspension.data();
}

void SurfaceSuspensionV1Interface::wp_surface_suspension_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

} // namespace KWaylandServer
