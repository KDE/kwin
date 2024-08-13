/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "xx_colormanagement_v4.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"
#include "utils/resource.h"
#include "wayland/output.h"

namespace KWin
{

XXColorManagerV4::XXColorManagerV4(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xx_color_manager_v4(*display, 1)
{
}

void XXColorManagerV4::xx_color_manager_v4_bind_resource(Resource *resource)
{
    send_supported_feature(resource->handle, feature::feature_parametric);
    send_supported_feature(resource->handle, feature::feature_extended_target_volume);
    send_supported_feature(resource->handle, feature::feature_set_mastering_display_primaries);
    send_supported_feature(resource->handle, feature::feature_set_primaries);
    send_supported_feature(resource->handle, feature::feature_set_luminances);

    send_supported_primaries_named(resource->handle, primaries::primaries_srgb);
    send_supported_primaries_named(resource->handle, primaries::primaries_bt2020);

    send_supported_tf_named(resource->handle, transfer_function::transfer_function_gamma22);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_srgb);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_st2084_pq);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_linear);

    send_supported_intent(resource->handle, render_intent::render_intent_perceptual);
    send_supported_intent(resource->handle, render_intent::render_intent_relative);
    send_supported_intent(resource->handle, render_intent::render_intent_absolute);
    // TODO implement saturation and relative bpc intents
}

void XXColorManagerV4::xx_color_manager_v4_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorManagerV4::xx_color_manager_v4_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output)
{
    new XXColorManagementOutputV4(resource->client(), id, resource->version(), OutputInterface::get(output)->handle());
}

void XXColorManagerV4::xx_color_manager_v4_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    if (priv->frogColorManagement || priv->xxColorSurface) {
        wl_resource_post_error(resource->handle, 0, "there's already a color management surface for this wl_surface");
        return;
    }
    priv->xxColorSurface = new XXColorSurfaceV4(resource->client(), id, resource->version(), surf);
}

void XXColorManagerV4::xx_color_manager_v4_get_feedback_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    priv->xxColorFeedbacks.push_back(new XXColorFeedbackSurfaceV4(resource->client(), id, resource->version(), surf));
}

void XXColorManagerV4::xx_color_manager_v4_new_icc_creator(Resource *resource, uint32_t obj)
{
    wl_resource_post_error(resource->handle, error::error_unsupported_feature, "ICC profiles are not supported");
}

void XXColorManagerV4::xx_color_manager_v4_new_parametric_creator(Resource *resource, uint32_t obj)
{
    new XXColorParametricCreatorV4(resource->client(), obj, resource->version());
}

XXColorFeedbackSurfaceV4::XXColorFeedbackSurfaceV4(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::xx_color_management_feedback_surface_v4(client, id, version)
    , m_surface(surface)
    , m_preferred(SurfaceInterfacePrivate::get(surface)->preferredColorDescription.value_or(ColorDescription::sRGB))
{
}

XXColorFeedbackSurfaceV4::~XXColorFeedbackSurfaceV4()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->xxColorFeedbacks.removeOne(this);
    }
}

void XXColorFeedbackSurfaceV4::setPreferredColorDescription(const ColorDescription &descr)
{
    if (m_preferred != descr) {
        m_preferred = descr;
        send_preferred_changed(resource()->handle);
    }
}

void XXColorFeedbackSurfaceV4::xx_color_management_feedback_surface_v4_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorFeedbackSurfaceV4::xx_color_management_feedback_surface_v4_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorFeedbackSurfaceV4::xx_color_management_feedback_surface_v4_get_preferred(Resource *resource, uint32_t image_description)
{
    new XXImageDescriptionV4(resource->client(), image_description, resource->version(), m_preferred);
}

XXColorSurfaceV4::XXColorSurfaceV4(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::xx_color_management_surface_v4(client, id, version)
    , m_surface(surface)
{
}

XXColorSurfaceV4::~XXColorSurfaceV4()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->colorDescription = ColorDescription::sRGB;
        priv->pending->colorDescriptionIsSet = true;
        priv->xxColorSurface = nullptr;
    }
}

void XXColorSurfaceV4::xx_color_management_surface_v4_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorSurfaceV4::xx_color_management_surface_v4_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static std::optional<RenderingIntent> waylandToKwinIntent(uint32_t intent)
{
    switch (intent) {
    case QtWaylandServer::xx_color_manager_v4::render_intent::render_intent_perceptual:
        return RenderingIntent::Perceptual;
    case QtWaylandServer::xx_color_manager_v4::render_intent::render_intent_relative:
        return RenderingIntent::RelativeColorimetric;
    case QtWaylandServer::xx_color_manager_v4::render_intent::render_intent_absolute:
        return RenderingIntent::AbsoluteColorimetric;
    case QtWaylandServer::xx_color_manager_v4::render_intent::render_intent_saturation:
    case QtWaylandServer::xx_color_manager_v4::render_intent::render_intent_relative_bpc:
        return std::nullopt;
    }
    return std::nullopt;
}

void XXColorSurfaceV4::xx_color_management_surface_v4_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent)
{
    if (!m_surface) {
        return;
    }
    const std::optional<RenderingIntent> intent = waylandToKwinIntent(render_intent);
    if (!intent) {
        wl_resource_post_error(resource->handle, XX_COLOR_MANAGER_V4_ERROR_UNSUPPORTED_FEATURE, "rendering intent is not supported");
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = XXImageDescriptionV4::get(image_description)->description();
    priv->pending->renderingIntent = *intent;
    priv->pending->colorDescriptionIsSet = true;
}

void XXColorSurfaceV4::xx_color_management_surface_v4_unset_image_description(Resource *resource)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ColorDescription::sRGB;
    priv->pending->colorDescriptionIsSet = true;
}

XXColorParametricCreatorV4::XXColorParametricCreatorV4(wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::xx_image_description_creator_params_v4(client, id, version)
{
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_create(Resource *resource, uint32_t image_description)
{
    if (!m_colorimetry || !m_transferFunctionType) {
        wl_resource_post_error(resource->handle, error::error_incomplete_set, "colorimetry or transfer function missing");
        return;
    }
    if (m_transferFunctionType != TransferFunction::PerceptualQuantizer && (m_maxCll || m_maxFall)) {
        wl_resource_post_error(resource->handle, error::error_inconsistent_set, "max_cll and max_fall must only be set with the PQ transfer function");
        return;
    }
    const std::optional<double> maxFrameAverageLuminance = m_maxFall ? m_maxFall : m_maxMasteringLuminance;
    const std::optional<double> maxHdrLuminance = m_maxCll ? m_maxCll : m_maxMasteringLuminance;
    TransferFunction func{*m_transferFunctionType};
    double referenceLuminance = TransferFunction::defaultReferenceLuminanceFor(func.type);
    if (m_transferFunctionLuminances) {
        func.minLuminance = m_transferFunctionLuminances->min;
        func.maxLuminance = m_transferFunctionLuminances->max;
        referenceLuminance = m_transferFunctionLuminances->reference;
    }
    new XXImageDescriptionV4(resource->client(), image_description, resource->version(), ColorDescription(*m_colorimetry, func, referenceLuminance, m_minMasteringLuminance.value_or(0), maxFrameAverageLuminance, maxHdrLuminance, m_masteringColorimetry, Colorimetry::fromName(NamedColorimetry::BT709)));
    wl_resource_destroy(resource->handle);
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_tf_named(Resource *resource, uint32_t tf)
{
    if (m_transferFunctionType) {
        wl_resource_post_error(resource->handle, error::error_already_set, "transfer function is already set");
        return;
    }
    switch (tf) {
    case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB:
    case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22:
        m_transferFunctionType = TransferFunction::gamma22;
        return;
    case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ:
        m_transferFunctionType = TransferFunction::PerceptualQuantizer;
        return;
    case XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR:
        m_transferFunctionType = TransferFunction::linear;
        return;
    default:
        // TODO add more transfer functions
        wl_resource_post_error(resource->handle, error::error_invalid_tf, "unsupported named transfer function");
    }
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_tf_power(Resource *resource, uint32_t eexp)
{
    wl_resource_post_error(resource->handle, error::error_invalid_tf, "power transfer functions are not supported");
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_primaries_named(Resource *resource, uint32_t primaries)
{
    if (m_colorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "primaries are already set");
        return;
    }
    switch (primaries) {
    case XX_COLOR_MANAGER_V4_PRIMARIES_SRGB:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT709);
        return;
    case XX_COLOR_MANAGER_V4_PRIMARIES_BT2020:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT2020);
        return;
    default:
        // TODO add more named primaries
        wl_resource_post_error(resource->handle, error::error_invalid_primaries, "unsupported named primaries");
    }
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y)
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
        QVector2D(w_x / 10'000.0, w_y / 10'000.0),
    };
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_luminances(Resource *resource, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum)
{
    m_transferFunctionLuminances = Luminances{
        .min = min_lum / 10'000.0,
        .max = double(max_lum),
        .reference = double(reference_lum),
    };
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_mastering_display_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y)
{
    if (m_masteringColorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "mastering display primaries are already set");
        return;
    }
    m_masteringColorimetry = Colorimetry{
        QVector2D(r_x / 10'000.0, r_y / 10'000.0),
        QVector2D(g_x / 10'000.0, g_y / 10'000.0),
        QVector2D(b_x / 10'000.0, b_y / 10'000.0),
        QVector2D(w_x / 10'000.0, w_y / 10'000.0),
    };
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum)
{
    if (m_minMasteringLuminance) {
        wl_resource_post_error(resource->handle, error::error_already_set, "mastering luminance is already set");
        return;
    }
    m_minMasteringLuminance = min_lum / 10'000.0;
    if (max_lum > 0) {
        m_maxMasteringLuminance = max_lum;
    }
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_max_cll(Resource *resource, uint32_t max_cll)
{
    if (m_maxCll) {
        wl_resource_post_error(resource->handle, error::error_already_set, "max_cll is already set");
        return;
    }
    if (max_cll > 0) {
        m_maxCll = max_cll;
    }
}

void XXColorParametricCreatorV4::xx_image_description_creator_params_v4_set_max_fall(Resource *resource, uint32_t max_fall)
{
    if (m_maxFall) {
        wl_resource_post_error(resource->handle, error::error_already_set, "max_fall is already set");
        return;
    }
    if (max_fall) {
        m_maxFall = max_fall;
    }
}

XXImageDescriptionV4::XXImageDescriptionV4(wl_client *client, uint32_t id, uint32_t version, const ColorDescription &color)
    : QtWaylandServer::xx_image_description_v4(client, id, version)
    , m_description(color)
{
    // there's no need to track image description identities, as our descriptions are very lightweight
    static uint32_t s_identity = 1;
    send_ready(resource()->handle, s_identity++);
}

void XXImageDescriptionV4::xx_image_description_v4_destroy_resource(Resource *resource)
{
    delete this;
}

void XXImageDescriptionV4::xx_image_description_v4_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static uint32_t kwinTFtoProtoTF(TransferFunction tf)
{
    switch (tf.type) {
    case TransferFunction::sRGB:
        return xx_color_manager_v4_transfer_function::XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_SRGB;
    case TransferFunction::linear:
        return xx_color_manager_v4_transfer_function::XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_LINEAR;
    case TransferFunction::PerceptualQuantizer:
        return xx_color_manager_v4_transfer_function::XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_ST2084_PQ;
    case TransferFunction::gamma22:
        return xx_color_manager_v4_transfer_function::XX_COLOR_MANAGER_V4_TRANSFER_FUNCTION_GAMMA22;
    }
    Q_UNREACHABLE();
}

void XXImageDescriptionV4::xx_image_description_v4_get_information(Resource *qtResource, uint32_t information)
{
    auto resource = wl_resource_create(qtResource->client(), &xx_image_description_info_v4_interface, qtResource->version(), information);
    const auto c = m_description.containerColorimetry();
    const auto round = [](float f) {
        return std::clamp(std::round(f * 10'000.0), 0.0, 10'000.0);
    };
    xx_image_description_info_v4_send_primaries(resource,
                                                round(c.red().x()), round(c.red().y()),
                                                round(c.green().x()), round(c.green().y()),
                                                round(c.blue().x()), round(c.blue().y()),
                                                round(c.white().x()), round(c.white().y()));
    xx_image_description_info_v4_send_luminances(resource, std::round(m_description.transferFunction().minLuminance * 10'000), std::round(m_description.transferFunction().maxLuminance), std::round(m_description.referenceLuminance()));
    xx_image_description_info_v4_send_target_luminance(resource, m_description.minLuminance(), m_description.maxHdrLuminance().value_or(800));
    xx_image_description_info_v4_send_tf_named(resource, kwinTFtoProtoTF(m_description.transferFunction()));
    xx_image_description_info_v4_send_done(resource);
    wl_resource_destroy(resource);
}

const ColorDescription &XXImageDescriptionV4::description() const
{
    return m_description;
}

XXImageDescriptionV4 *XXImageDescriptionV4::get(wl_resource *resource)
{
    if (auto resourceContainer = Resource::fromResource(resource)) {
        return static_cast<XXImageDescriptionV4 *>(resourceContainer->object());
    } else {
        return nullptr;
    }
}

XXColorManagementOutputV4::XXColorManagementOutputV4(wl_client *client, uint32_t id, uint32_t version, Output *output)
    : QtWaylandServer::xx_color_management_output_v4(client, id, version)
    , m_output(output)
    , m_colorDescription(output->colorDescription())
{
    connect(output, &Output::colorDescriptionChanged, this, &XXColorManagementOutputV4::colorDescriptionChanged);
}

void XXColorManagementOutputV4::xx_color_management_output_v4_destroy_resource(Resource *resource)
{
    delete this;
}

void XXColorManagementOutputV4::xx_color_management_output_v4_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XXColorManagementOutputV4::xx_color_management_output_v4_get_image_description(Resource *resource, uint32_t image_description)
{
    new XXImageDescriptionV4(resource->client(), image_description, resource->version(), m_colorDescription);
}

void XXColorManagementOutputV4::colorDescriptionChanged()
{
    m_colorDescription = m_output->colorDescription();
    send_image_description_changed();
}

}

#include "moc_xx_colormanagement_v4.cpp"
