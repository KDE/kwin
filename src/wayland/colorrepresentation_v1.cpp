/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "colorrepresentation_v1.h"

#include "display.h"
#include "surface_p.h"
#include "utils/drm_format_helper.h"

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
    send_supported_alpha_mode(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED);
    send_supported_coefficients_and_ranges(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020, WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL);
    send_done(resource->handle);
}

void ColorRepresentationManagerV1::wp_color_representation_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorRepresentationManagerV1::wp_color_representation_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *surfPrivate = SurfaceInterfacePrivate::get(surf);
    if (surfPrivate->colorRepresentation) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_MANAGER_V1_ERROR_SURFACE_EXISTS, "Surface already has a color representation object");
        return;
    }
    surfPrivate->colorRepresentation = new ColorRepresentationSurfaceV1(surf, resource->client(), id, resource->version());
}

ColorRepresentationSurfaceV1::ColorRepresentationSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::wp_color_representation_surface_v1(client, id, version)
    , m_surface(surface)
{
}

ColorRepresentationSurfaceV1::~ColorRepresentationSurfaceV1()
{
    if (m_surface) {
        auto surfPrivate = SurfaceInterfacePrivate::get(m_surface);
        surfPrivate->colorRepresentation = nullptr;
        surfPrivate->pending->yuvCoefficients = YUVMatrixCoefficients::Identity;
        surfPrivate->pending->committed |= SurfaceState::Field::YuvCoefficients;
    }
}

void ColorRepresentationSurfaceV1::wp_color_representation_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorRepresentationSurfaceV1::wp_color_representation_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorRepresentationSurfaceV1::wp_color_representation_surface_v1_set_alpha_mode(Resource *resource, uint32_t alpha_mode)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    if (alpha_mode != alpha_mode_premultiplied_electrical) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_ALPHA_MODE, "unsupported alpha mode used");
        return;
    }
}

static const std::unordered_map<uint32_t, YUVMatrixCoefficients> s_protoToKWinMap = {
    {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY, YUVMatrixCoefficients::Identity},
    {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601, YUVMatrixCoefficients::BT601},
    {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709, YUVMatrixCoefficients::BT709},
    {WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020, YUVMatrixCoefficients::BT2020},
};

void ColorRepresentationSurfaceV1::wp_color_representation_surface_v1_set_coefficients_and_range(Resource *resource, uint32_t coefficients, uint32_t range)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    auto it = s_protoToKWinMap.find(coefficients);
    if (it == s_protoToKWinMap.end()) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_COEFFICIENTS, "unsupported coefficients used");
        return;
    }
    // TODO support limited range rgb
    if (coefficients == WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY && range != WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_COEFFICIENTS, "Limited range isn't supported with RGB");
        return;
    }
    auto surfPrivate = SurfaceInterfacePrivate::get(m_surface);
    surfPrivate->pending->yuvCoefficients = it->second;
    surfPrivate->pending->range = range == WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL ? EncodingRange::Full : EncodingRange::Limited;
    surfPrivate->pending->committed |= SurfaceState::Field::YuvCoefficients;
}

void ColorRepresentationSurfaceV1::wp_color_representation_surface_v1_set_chroma_location(Resource *resource, uint32_t chroma_location)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT, "The related surface was destroyed");
        return;
    }
    // TODO route this through to the core, and use it when possible
}

bool ColorRepresentationSurfaceV1::maybeEmitProtocolErrors()
{
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    if (!priv->pending->buffer) {
        return false;
    }
    bool yuv = false;
    if (auto attrs = priv->pending->buffer->dmabufAttributes()) {
        // this assumes that we only support YUV formats we have conversions for
        // if we ever change that, we should add a "isYUV" flag in FormatInfo
        const auto info = FormatInfo::get(attrs->format);
        yuv = info && info->yuvConversion();
    } else {
        // we (currently?) don't support YUV formats with SHM
        // and single pixel buffer doesn't support them either
        yuv = false;
    }
    const bool match = yuv == (priv->pending->yuvCoefficients != YUVMatrixCoefficients::Identity);
    if (match) {
        return false;
    }
    if (yuv) {
        wl_resource_post_error(resource()->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_PIXEL_FORMAT,
                               "The buffer has a YCbCr format, but color representation is identity!");
    } else {
        wl_resource_post_error(resource()->handle, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_PIXEL_FORMAT,
                               "The buffer has a RGB format, but color representation is not identity!");
    }
    return true;
}
}
