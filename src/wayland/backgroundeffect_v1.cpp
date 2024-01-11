/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "backgroundeffect_v1.h"
#include "display.h"
#include "region_p.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

ExtBackgroundEffectManagerV1::ExtBackgroundEffectManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::ext_background_effect_manager_v1(*display, s_version)
{
    m_removeTimer.setSingleShot(true);
    m_removeTimer.setInterval(100);
    connect(&m_removeTimer, &QTimer::timeout, this, &ExtBackgroundEffectManagerV1::sendCapabilities);
}

void ExtBackgroundEffectManagerV1::addBlurCapability()
{
    if (!m_supportsBlur) {
        m_supportsBlur = true;
        m_removeTimer.stop();
        sendCapabilities();
    }
}

void ExtBackgroundEffectManagerV1::removeBlurCapability()
{
    if (m_supportsBlur) {
        m_supportsBlur = false;
        // compress the capability removal
        // so that reloading effects doesn't make clients do stupid things
        m_removeTimer.start();
    }
}

void ExtBackgroundEffectManagerV1::sendCapabilities()
{
    const auto resources = resourceMap();
    for (const auto res : resources) {
        send_capabilities(res->handle, m_supportsBlur ? capability_blur : 0);
    }
}

void ExtBackgroundEffectManagerV1::ext_background_effect_manager_v1_bind_resource(Resource *resource)
{
    send_capabilities(resource->handle, m_supportsBlur ? capability_blur : 0);
}

void ExtBackgroundEffectManagerV1::ext_background_effect_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtBackgroundEffectManagerV1::ext_background_effect_manager_v1_get_background_effect(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(surf);
    if (surfPriv->extBackgroundeffect) {
        wl_resource_post_error(resource->handle, error_background_effect_exists, "surface already has an associated blur object");
        return;
    }
    surfPriv->extBackgroundeffect = new ExtBackgroundEffectSurfaceV1(surf, resource->client(), id, resource->version());
}

ExtBackgroundEffectSurfaceV1::ExtBackgroundEffectSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::ext_background_effect_surface_v1(client, id, version)
    , m_surface(surface)
{
}

ExtBackgroundEffectSurfaceV1::~ExtBackgroundEffectSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(m_surface);
        surfPriv->extBackgroundeffect = nullptr;
        surfPriv->pending->blurRegion = Region{};
        surfPriv->pending->committed |= SurfaceState::Field::Blur;
    }
}

void ExtBackgroundEffectSurfaceV1::ext_background_effect_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ExtBackgroundEffectSurfaceV1::ext_background_effect_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtBackgroundEffectSurfaceV1::ext_background_effect_surface_v1_set_blur_region(Resource *resource, wl_resource *region)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, error_surface_destroyed, "tried to set blur region on destroyed surface");
        return;
    }
    SurfaceInterfacePrivate *surfPriv = SurfaceInterfacePrivate::get(m_surface);
    if (region) {
        const auto interface = RegionInterface::get(region);
        surfPriv->pending->blurRegion = interface->region();
    } else {
        surfPriv->pending->blurRegion = Region{};
    }
    surfPriv->pending->committed |= SurfaceState::Field::Blur;
}
}
