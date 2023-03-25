/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fractionalscale_v1_interface.h"

#include "display.h"
#include "fractionalscale_v1_interface_p.h"
#include "surface_interface_p.h"

#include <cmath>

static const int s_version = 1;

namespace KWaylandServer
{
class FractionalScaleManagerV1InterfacePrivate : public QtWaylandServer::wp_fractional_scale_manager_v1
{
protected:
    void wp_fractional_scale_manager_v1_destroy(Resource *resource) override;
    void wp_fractional_scale_manager_v1_get_fractional_scale(Resource *resource, uint32_t id, wl_resource *surface) override;
};

void FractionalScaleManagerV1InterfacePrivate::wp_fractional_scale_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FractionalScaleManagerV1InterfacePrivate::wp_fractional_scale_manager_v1_get_fractional_scale(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surface_resource);

    FractionalScaleV1Interface *scaleIface = FractionalScaleV1Interface::get(surface);
    if (scaleIface) {
        wl_resource_post_error(resource->handle, error_fractional_scale_exists, "the specified surface already has a fractional scale");
        return;
    }

    wl_resource *surfaceScalerResource = wl_resource_create(resource->client(), &wp_fractional_scale_v1_interface, resource->version(), id);

    new FractionalScaleV1Interface(surface, surfaceScalerResource);
}

FractionalScaleV1Interface::FractionalScaleV1Interface(SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::wp_fractional_scale_v1(resource)
    , surface(surface)
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->fractionalScaleExtension = this;
    setPreferredScale(surfacePrivate->preferredScale);
}

FractionalScaleV1Interface::~FractionalScaleV1Interface()
{
    if (surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
        surfacePrivate->fractionalScaleExtension = nullptr;
    }
}

FractionalScaleV1Interface *FractionalScaleV1Interface::get(SurfaceInterface *surface)
{
    return SurfaceInterfacePrivate::get(surface)->fractionalScaleExtension;
}

void FractionalScaleV1Interface::setPreferredScale(qreal scale)
{
    send_preferred_scale(std::round(scale * 120));
}

void FractionalScaleV1Interface::wp_fractional_scale_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FractionalScaleV1Interface::wp_fractional_scale_v1_destroy_resource(Resource *)
{
    delete this;
}

FractionalScaleManagerV1Interface::FractionalScaleManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new FractionalScaleManagerV1InterfacePrivate)
{
    d->init(*display, s_version);
}

FractionalScaleManagerV1Interface::~FractionalScaleManagerV1Interface()
{
}

} // namespace KWaylandServer
