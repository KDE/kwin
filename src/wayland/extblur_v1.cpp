/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "extblur_v1.h"
#include "display.h"
#include "region_p.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

ExtBlurManagerV1::ExtBlurManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::ext_blur_manager_v1(*display, s_version)
{
}

void ExtBlurManagerV1::remove()
{
    globalRemove();
}

void ExtBlurManagerV1::ext_blur_manager_v1_destroy_global()
{
    delete this;
}

void ExtBlurManagerV1::ext_blur_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtBlurManagerV1::ext_blur_manager_v1_get_blur(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(surf);
    if (surfPriv->extBlur) {
        wl_resource_post_error(resource->handle, error_blur_exists, "surface already has an associated blur object");
        return;
    }
    surfPriv->extBlur = new ExtBlurSurfaceV1(surf, resource->client(), id);
}

ExtBlurSurfaceV1::ExtBlurSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : m_surface(surface)
{
}

ExtBlurSurfaceV1::~ExtBlurSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(m_surface);
        surfPriv->extBlur = nullptr;
        surfPriv->pending->blurRegion = QRegion{};
        surfPriv->pending->committed |= SurfaceState::Field::Blur;
    }
}

void ExtBlurSurfaceV1::ext_blur_surface_v1_destroy(Resource *resource)
{
    delete this;
}

void ExtBlurSurfaceV1::ext_blur_surface_v1_destroy_resource(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtBlurSurfaceV1::ext_blur_surface_v1_set_region(Resource *resource, wl_resource *region)
{
    if (m_surface) {
        return;
    }
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(m_surface);
    if (region) {
        const auto interface = RegionInterface::get(region);
        surfPriv->pending->blurRegion = interface->region();
    } else {
        surfPriv->pending->blurRegion = infiniteRegion();
    }
    surfPriv->pending->committed |= SurfaceState::Field::Blur;
}
}
