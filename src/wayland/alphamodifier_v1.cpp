/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "alphamodifier_v1.h"

#include "display.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

AlphaModifierManagerV1::AlphaModifierManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_alpha_modifier_v1(*display, s_version)
{
}

void AlphaModifierManagerV1::wp_alpha_modifier_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AlphaModifierManagerV1::wp_alpha_modifier_v1_get_surface(Resource *resource, uint32_t id, ::wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *priv = SurfaceInterfacePrivate::get(surf);
    if (priv->alphaModifier) {
        wl_resource_post_error(surface, error_already_constructed, "wl_surface already has an alpha modifier surface");
        return;
    }
    new AlphaModifierSurfaceV1(resource->client(), id, resource->version(), surf);
}

AlphaModifierSurfaceV1::AlphaModifierSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::wp_alpha_modifier_surface_v1(client, id, version)
    , m_surface(surface)
{
}

AlphaModifierSurfaceV1::~AlphaModifierSurfaceV1()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->alphaMultiplier = 1;
        priv->pending->alphaMultiplierIsSet = true;
    }
}

void AlphaModifierSurfaceV1::wp_alpha_modifier_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void AlphaModifierSurfaceV1::wp_alpha_modifier_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AlphaModifierSurfaceV1::wp_alpha_modifier_surface_v1_set_multiplier(Resource *resource, uint32_t factor)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, error_no_surface, "wl_surface was destroyed before a set_multiplier request");
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->alphaMultiplier = factor / double(std::numeric_limits<uint32_t>::max());
    priv->pending->alphaMultiplierIsSet = true;
}

}
