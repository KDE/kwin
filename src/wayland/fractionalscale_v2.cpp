/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "fractionalscale_v2.h"

#include "display.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

FractionalScaleManagerV2::FractionalScaleManagerV2(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_fractional_scale_manager_v2(*display, s_version)
{
}

FractionalScaleManagerV2::~FractionalScaleManagerV2()
{
}

void FractionalScaleManagerV2::wp_fractional_scale_manager_v2_destroy_global()
{
    delete this;
}

void FractionalScaleManagerV2::wp_fractional_scale_manager_v2_get_fractional_scale(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    if (isGlobalRemoved()) {
        return;
    }
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPrivate = SurfaceInterfacePrivate::get(surf);
    if (surfPrivate->fractionalScaleV2) {
        wl_resource_post_error(resource->handle, error_fractional_scale_exists, "The surface already has a fractional_scale_v1 associated with it");
        return;
    }
    wl_resource *fractionalScaleResource = wl_resource_create(resource->client(), &wp_fractional_scale_v2_interface, s_version, id);
    if (!fractionalScaleResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    surfPrivate->fractionalScaleV2 = new FractionalScaleV2(surf, fractionalScaleResource);
}

FractionalScaleV2::FractionalScaleV2(SurfaceInterface *surface, wl_resource *resource)
    : QtWaylandServer::wp_fractional_scale_v2(resource)
    , m_surface(surface)
{
}

void FractionalScaleV2::setFractionalScale(double scale)
{
    if (SurfaceInterface *surf = m_surface) {
        SurfaceInterfacePrivate *surfPrivate = SurfaceInterfacePrivate::get(surf);
        if (surfPrivate->compositorToClientScale != scale) {
            send_scale_factor(resource()->handle, scale * (1UL << 24));
            surfPrivate->compositorToClientScale = scale;
        }
    }
}

void FractionalScaleV2::wp_fractional_scale_v2_set_scale_factor(Resource *resource, uint32_t scale_8_24)
{
    if (SurfaceInterface *surf = m_surface) {
        SurfaceInterfacePrivate::get(surf)->clientToCompositorScale = scale_8_24 / double(1UL << 24);
    }
}

void FractionalScaleV2::wp_fractional_scale_v2_destroy(Resource *resource)
{
    if (SurfaceInterface *surf = m_surface) {
        SurfaceInterfacePrivate *surfPrivate = SurfaceInterfacePrivate::get(surf);
        surfPrivate->clientToCompositorScale = 1;
        surfPrivate->fractionalScaleV2 = nullptr;
    }
    delete this;
}

}
