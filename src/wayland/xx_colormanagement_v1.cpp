/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xx_colormanagement_v1.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"
#include "utils/resource.h"
#include "wayland/output.h"

namespace KWin
{

XXColorManagerV1::XXColorManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xx_color_manager_v1(*display, 1)
{
}

void XXColorManagerV1::xx_color_manager_v1_bind_resource(Resource *resource)
{
    send_supported_feature(resource->handle, feature::feature_parametric);
    send_supported_feature(resource->handle, feature::feature_extended_target_volume);
    send_supported_feature(resource->handle, feature::feature_set_mastering_display_primaries);
    send_supported_feature(resource->handle, feature::feature_set_primaries);

    send_supported_primaries_named(resource->handle, primaries::primaries_srgb);
    send_supported_primaries_named(resource->handle, primaries::primaries_bt2020);

    send_supported_tf_named(resource->handle, transfer_function::transfer_function_bt709);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_gamma22);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_srgb);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_st2084_pq);
    // TODO scRGB?

    send_supported_intent(resource->handle, render_intent::render_intent_perceptual);
    send_supported_intent(resource->handle, render_intent::render_intent_relative);
    // TODO implement the other rendering intents
}

void XXColorManagerV1::xx_color_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorManagerV1::xx_color_manager_v1_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output)
{
    new XXColorManagementOutputV1(resource->client(), id, resource->version(), OutputInterface::get(output)->handle());
}

void XXColorManagerV1::xx_color_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    if (priv->frogColorManagement || priv->xxColorSurface) {
        wl_resource_post_error(resource->handle, 0, "there's already a color management surface for this wl_surface");
        return;
    }
    priv->xxColorSurface = new XXColorSurfaceV1(resource->client(), id, resource->version(), surf);
}

void XXColorManagerV1::xx_color_manager_v1_new_icc_creator(Resource *resource, uint32_t obj)
{
    wl_resource_post_error(resource->handle, error::error_unsupported_feature, "ICC profiles are not supported");
}

void XXColorManagerV1::xx_color_manager_v1_new_parametric_creator(Resource *resource, uint32_t obj)
{
    new XXColorParametricCreatorV1(resource->client(), obj, resource->version());
}

XXColorSurfaceV1::XXColorSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::xx_color_management_surface_v1(client, id, version)
    , m_surface(surface)
    , m_preferred(SurfaceInterfacePrivate::get(surface)->preferredColorDescription.value_or(ColorDescription::sRGB))
{
}

XXColorSurfaceV1::~XXColorSurfaceV1()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->colorDescription = ColorDescription::sRGB;
        priv->pending->colorDescriptionIsSet = true;
        priv->xxColorSurface = nullptr;
    }
}

void XXColorSurfaceV1::setPreferredColorDescription(const ColorDescription &descr)
{
    if (m_preferred != descr) {
        m_preferred = descr;
        send_preferred_changed(resource()->handle);
    }
}

void XXColorSurfaceV1::xx_color_management_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorSurfaceV1::xx_color_management_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorSurfaceV1::xx_color_management_surface_v1_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = XXImageDescriptionV1::get(image_description)->description();
    priv->pending->colorDescriptionIsSet = true;
    // TODO render_intent
}

void XXColorSurfaceV1::xx_color_management_surface_v1_set_default_image_description(Resource *resource)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ColorDescription::sRGB;
    priv->pending->colorDescriptionIsSet = true;
}

void XXColorSurfaceV1::xx_color_management_surface_v1_get_preferred(Resource *resource, uint32_t image_description)
{
    new XXImageDescriptionV1(resource->client(), image_description, resource->version(), m_preferred);
}

XXColorParametricCreatorV1::XXColorParametricCreatorV1(wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::xx_image_description_creator_params_v1(client, id, version)
{
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description)
{
    if (!m_colorimetry || !m_transferFunction) {
        wl_resource_post_error(resource->handle, error::error_incomplete_set, "colorimetry or transfer function missing");
        return;
    }
    if (m_transferFunction != NamedTransferFunction::PerceptualQuantizer && (m_maxFrameAverageBrightness || m_maxPeakBrightness)) {
        wl_resource_post_error(resource->handle, error::error_inconsistent_set, "max_cll and max_fall must only be set with the PQ transfer function");
        return;
    }
    new XXImageDescriptionV1(resource->client(), image_description, resource->version(), ColorDescription(*m_colorimetry, *m_transferFunction, 100, 0, m_maxFrameAverageBrightness.value_or(100), m_maxPeakBrightness.value_or(100), 0));
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_tf_named(Resource *resource, uint32_t tf)
{
    if (m_transferFunction) {
        wl_resource_post_error(resource->handle, error::error_already_set, "transfer function is already set");
        return;
    }
    switch (tf) {
    case XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
    case XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT709:
    case XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
        m_transferFunction = NamedTransferFunction::gamma22;
        return;
    case XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
        m_transferFunction = NamedTransferFunction::PerceptualQuantizer;
        return;
    default:
        // TODO add more transfer functions
        wl_resource_post_error(resource->handle, error::error_invalid_tf, "unsupported named transfer function");
    }
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp)
{
    wl_resource_post_error(resource->handle, error::error_invalid_tf, "power transfer functions are not supported");
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_primaries_named(Resource *resource, uint32_t primaries)
{
    if (m_colorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "primaries are already set");
        return;
    }
    switch (primaries) {
    case XX_COLOR_MANAGER_V1_PRIMARIES_SRGB:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT709);
        return;
    case XX_COLOR_MANAGER_V1_PRIMARIES_BT2020:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT2020);
        return;
    default:
        // TODO add more named primaries
        wl_resource_post_error(resource->handle, error::error_invalid_primaries, "unsupported named primaries");
    }
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y)
{
    if (m_colorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "primaries are already set");
        return;
    }
    if (w_x == 0 || w_y == 0) {
        wl_resource_post_error(resource->handle, error::error_invalid_primaries, "whitepoint must not be zero");
        return;
    }
    m_colorimetry = Colorimetry{
        QVector2D(r_x / 10'000.0, r_y / 10'000.0),
        QVector2D(g_x / 10'000.0, g_y / 10'000.0),
        QVector2D(b_x / 10'000.0, b_y / 10'000.0),
        QVector2D(w_x / 10'000.0, w_y / 10'000.0)};
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, uint32_t r_x, uint32_t r_y, uint32_t g_x, uint32_t g_y, uint32_t b_x, uint32_t b_y, uint32_t w_x, uint32_t w_y)
{
    // ignored (at least for now)
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum)
{
    // ignored (at least for now)
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t max_cll)
{
    m_maxPeakBrightness = max_cll;
}

void XXColorParametricCreatorV1::xx_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t max_fall)
{
    m_maxFrameAverageBrightness = max_fall;
}

XXImageDescriptionV1::XXImageDescriptionV1(wl_client *client, uint32_t id, uint32_t version, const ColorDescription &color)
    : QtWaylandServer::xx_image_description_v1(client, id, version)
    , m_description(color)
{
    // there's no need to track image description identities, as our descriptions are very lightweight
    static uint32_t s_identity = 1;
    send_ready(resource()->handle, s_identity++);
}

void XXImageDescriptionV1::xx_image_description_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XXImageDescriptionV1::xx_image_description_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static uint32_t kwinTFtoProtoTF(NamedTransferFunction tf)
{
    switch (tf) {
    case NamedTransferFunction::sRGB:
        return xx_color_manager_v1_transfer_function::XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
    case NamedTransferFunction::linear:
        return xx_color_manager_v1_transfer_function::XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LINEAR;
    case NamedTransferFunction::PerceptualQuantizer:
        return xx_color_manager_v1_transfer_function::XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
    case NamedTransferFunction::scRGB:
        return xx_color_manager_v1_transfer_function::XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LINEAR;
    case NamedTransferFunction::gamma22:
        return xx_color_manager_v1_transfer_function::XX_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    }
    Q_UNREACHABLE();
}

void XXImageDescriptionV1::xx_image_description_v1_get_information(Resource *qtResource, uint32_t information)
{
    auto resource = wl_resource_create(qtResource->client(), &xx_image_description_info_v1_interface, qtResource->version(), information);
    const auto c = m_description.colorimetry();
    const auto round = [](float f) {
        return std::clamp(std::round(f * 10'000.0), 0.0, 1.0);
    };
    xx_image_description_info_v1_send_primaries(resource,
                                                round(c.red().x()), round(c.red().y()),
                                                round(c.green().x()), round(c.green().y()),
                                                round(c.blue().x()), round(c.blue().y()),
                                                round(c.white().x()), round(c.white().y()));
    xx_image_description_info_v1_send_tf_named(resource, kwinTFtoProtoTF(m_description.transferFunction()));
    xx_image_description_info_v1_send_done(resource);
    wl_resource_destroy(resource);
}

const ColorDescription &XXImageDescriptionV1::description() const
{
    return m_description;
}

XXImageDescriptionV1 *XXImageDescriptionV1::get(wl_resource *resource)
{
    if (auto resourceContainer = Resource::fromResource(resource)) {
        return static_cast<XXImageDescriptionV1 *>(resourceContainer->object());
    } else {
        return nullptr;
    }
}

XXColorManagementOutputV1::XXColorManagementOutputV1(wl_client *client, uint32_t id, uint32_t version, Output *output)
    : QtWaylandServer::xx_color_management_output_v1(client, id, version)
    , m_output(output)
    , m_colorDescription(output->colorDescription())
{
    connect(output, &Output::colorDescriptionChanged, this, &XXColorManagementOutputV1::colorDescriptionChanged);
}

void XXColorManagementOutputV1::xx_color_management_output_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorManagementOutputV1::xx_color_management_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorManagementOutputV1::xx_color_management_output_v1_get_image_description(Resource *resource, uint32_t image_description)
{
    new XXImageDescriptionV1(resource->client(), image_description, resource->version(), m_colorDescription);
}

void XXColorManagementOutputV1::colorDescriptionChanged()
{
    m_colorDescription = m_output->colorDescription();
    send_image_description_changed();
}

}

#include "moc_xx_colormanagement_v1.cpp"
