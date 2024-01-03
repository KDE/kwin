/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "frog_colormanagement_v1.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

FrogColorManagementV1::FrogColorManagementV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::frog_color_management_factory_v1(*display, s_version)
{
}

FrogColorManagementV1::~FrogColorManagementV1()
{
}

void FrogColorManagementV1::frog_color_management_factory_v1_get_color_managed_surface(Resource *resource, wl_resource *surface, uint32_t callback)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate::get(surf)->frogColorManagement = new FrogColorManagementSurfaceV1(surf, resource->client(), callback);
}

void FrogColorManagementV1::frog_color_management_factory_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

FrogColorManagementSurfaceV1::FrogColorManagementSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : QtWaylandServer::frog_color_managed_surface(client, id, s_version)
    , m_surface(surface)
{
}

FrogColorManagementSurfaceV1::~FrogColorManagementSurfaceV1()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->colorDescription = ColorDescription::sRGBf();
        priv->pending->colorDescriptionIsSet = true;
        priv->frogColorManagement = nullptr;
    }
}

static QtWaylandServer::frog_color_managed_surface::transfer_function kwinToFrogTransferFunction(NamedTransferFunction tf)
{
    switch (tf) {
    case NamedTransferFunction::sRGB:
        return QtWaylandServer::frog_color_managed_surface::transfer_function_srgb;
    case NamedTransferFunction::gamma22:
        return QtWaylandServer::frog_color_managed_surface::transfer_function_gamma_22;
    case NamedTransferFunction::PerceptualQuantizer:
        return QtWaylandServer::frog_color_managed_surface::transfer_function_st2084_pq;
    case NamedTransferFunction::scRGB:
        return QtWaylandServer::frog_color_managed_surface::transfer_function_scrgb_linear;
    case NamedTransferFunction::linear:
        return QtWaylandServer::frog_color_managed_surface::transfer_function_scrgb_linear;
    }
    return QtWaylandServer::frog_color_managed_surface::transfer_function_undefined;
}

uint16_t encodePrimary(float primary)
{
    return uint16_t(std::clamp<float>(std::round(primary / 0.00002), 0, 0xC350));
}

void FrogColorManagementSurfaceV1::setPreferredColorDescription(const ColorDescription &colorDescription)
{
    const auto &color = colorDescription.colorimetry();
    send_preferred_metadata(kwinToFrogTransferFunction(colorDescription.transferFunction()),
                            encodePrimary(color.red().x()), encodePrimary(color.red().y()),
                            encodePrimary(color.green().x()), encodePrimary(color.green().y()),
                            encodePrimary(color.blue().x()), encodePrimary(color.blue().y()),
                            encodePrimary(color.white().x()), encodePrimary(color.white().y()),
                            std::round(colorDescription.maxHdrHighlightBrightness()),
                            std::round(colorDescription.minHdrBrightness() / 0.0001),
                            std::round(colorDescription.maxFrameAverageBrightness()));
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_set_known_transfer_function(Resource *resource, uint32_t transfer_function)
{
    switch (transfer_function) {
    case transfer_function_undefined:
    case transfer_function_srgb:
    case transfer_function_gamma_22:
        m_transferFunction = NamedTransferFunction::gamma22;
        break;
    case transfer_function_st2084_pq:
        m_transferFunction = NamedTransferFunction::PerceptualQuantizer;
        break;
    case transfer_function_scrgb_linear:
        m_transferFunction = NamedTransferFunction::scRGB;
        break;
    }
    updateColorDescription();
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_set_known_container_color_volume(Resource *resource, uint32_t primaries)
{
    switch (primaries) {
    case primaries_undefined:
    case primaries_rec709:
        m_colorimetry = NamedColorimetry::BT709;
        break;
    case primaries_rec2020:
        m_colorimetry = NamedColorimetry::BT2020;
        break;
    }
    updateColorDescription();
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_set_render_intent(Resource *resource, uint32_t render_intent)
{
    // there's only perceptual right now, so this can be ignored
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_set_hdr_metadata(Resource *resource,
                                                                               uint32_t mastering_display_primary_red_x, uint32_t mastering_display_primary_red_y,
                                                                               uint32_t mastering_display_primary_green_x, uint32_t mastering_display_primary_green_y,
                                                                               uint32_t mastering_display_primary_blue_x, uint32_t mastering_display_primary_blue_y,
                                                                               uint32_t mastering_white_point_x, uint32_t mastering_white_point_y,
                                                                               uint32_t max_display_mastering_luminance, uint32_t min_display_mastering_luminance,
                                                                               uint32_t max_cll, uint32_t max_fall)
{
    m_maxPeakBrightness = max_cll;
    m_maxFrameAverageBrightness = max_fall;
    updateColorDescription();
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void FrogColorManagementSurfaceV1::frog_color_managed_surface_destroy_resource(Resource *resource)
{
    delete this;
}

void FrogColorManagementSurfaceV1::updateColorDescription()
{
    if (m_surface) {
        // TODO make brightness values optional in ColorDescription
        SurfaceInterfacePrivate *priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->colorDescription = ColorDescription(m_colorimetry, m_transferFunction, 0, 0, m_maxFrameAverageBrightness, m_maxPeakBrightness, 0);
        priv->pending->colorDescriptionIsSet = true;
    }
}

}

#include "moc_frog_colormanagement_v1.cpp"
