/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "colorrepresentation_v1.h"

#include "display.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

ColorRepresentationManagerV1::ColorRepresentationManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_color_representation_manager_v1(*display, s_version)
{
}

void ColorRepresentationManagerV1::wp_color_representation_manager_v1_bind_resource(Resource *resource)
{
    send_supported_alpha_mode(resource->handle, WP_COLOR_REPRESENTATION_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_IDENTITY, WP_COLOR_REPRESENTATION_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT601, WP_COLOR_REPRESENTATION_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT601, WP_COLOR_REPRESENTATION_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT2020, WP_COLOR_REPRESENTATION_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT2020, WP_COLOR_REPRESENTATION_V1_RANGE_FULL);
    // this means the sample is in the center of the pixel, which *should* be what we're doing
    // TODO support the other locations too?
    send_supported_chroma_location(resource->handle, WP_COLOR_REPRESENTATION_V1_CHROMA_LOCATION_TYPE_1);
}

void ColorRepresentationManagerV1::wp_color_representation_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorRepresentationManagerV1::wp_color_representation_manager_v1_create(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPrivate = SurfaceInterfacePrivate::get(surf);
    if (surfPrivate->colorRepresentation) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED, "Surface already has a color representation object");
        return;
    }
    surfPrivate->colorRepresentation = new ColorRepresentationV1(surf, resource->client(), id, resource->version());
}

ColorRepresentationV1::ColorRepresentationV1(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::wp_color_representation_v1(client, id, version)
{
}

ColorRepresentationV1::~ColorRepresentationV1()
{
    if (m_surface) {
        auto surfPrivate = SurfaceInterfacePrivate::get(m_surface);
        surfPrivate->colorRepresentation = nullptr;
        surfPrivate->pending->yuvCoefficients = YUVMatrixCoefficients::Identity;
        surfPrivate->pending->yuvCoefficientsIsSet = true;
    }
}

void ColorRepresentationV1::wp_color_representation_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorRepresentationV1::wp_color_representation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorRepresentationV1::wp_color_representation_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    if (alpha_mode != alpha_mode_premultiplied_electrical) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_INVALID_ALPHA_MODE, "unsupported alpha mode used");
        return;
    }
}

static const std::unordered_map<uint32_t, YUVMatrixCoefficients> s_protoToKWinMap = {
    {WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_IDENTITY, YUVMatrixCoefficients::Identity},
    {WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT601, YUVMatrixCoefficients::BT601},
    {WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT709, YUVMatrixCoefficients::BT709},
    {WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_BT2020, YUVMatrixCoefficients::BT2020},
};

void ColorRepresentationV1::wp_color_representation_v1_set_coefficients_and_range(Resource *resource, uint32_t coefficients, uint32_t range)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    auto it = s_protoToKWinMap.find(coefficients);
    if (it == s_protoToKWinMap.end()) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_UNSUPPORTED_COEFFICIENTS, "unsupported coefficients used");
        return;
    }
    // TODO support limited range rgb
    if (coefficients == WP_COLOR_REPRESENTATION_V1_COEFFICIENTS_IDENTITY && range != WP_COLOR_REPRESENTATION_V1_RANGE_FULL) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_UNSUPPORTED_COEFFICIENTS, "Limited range isn't supported with RGB");
        return;
    }
    auto surfPrivate = SurfaceInterfacePrivate::get(m_surface);
    surfPrivate->pending->yuvCoefficients = it->second;
    surfPrivate->pending->range = range == WP_COLOR_REPRESENTATION_V1_RANGE_LIMITED ? EncodingRange::Limited : EncodingRange::Full;
    surfPrivate->pending->yuvCoefficientsIsSet = true;
}

void ColorRepresentationV1::wp_color_representation_v1_set_chroma_location(Resource *resource, uint32_t chroma_location)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    if (chroma_location != WP_COLOR_REPRESENTATION_V1_CHROMA_LOCATION_TYPE_1) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_V1_ERROR_UNSUPPORTED_CHROMA_LOCATION, "Only chroma location 0 is supported");
        return;
    }
}

}
