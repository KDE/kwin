/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "colormanagement_v1.h"
#include "display.h"
#include "surface.h"
#include "surface_p.h"
#include "utils/resource.h"
#include "wayland/output.h"

namespace KWin
{

ColorManagerV1::ColorManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_color_manager_v1(*display, 1)
{
}

void ColorManagerV1::wp_color_manager_v1_bind_resource(Resource *resource)
{
    send_supported_feature(resource->handle, feature::feature_parametric);
    send_supported_feature(resource->handle, feature::feature_extended_target_volume);
    send_supported_feature(resource->handle, feature::feature_set_mastering_display_primaries);
    send_supported_feature(resource->handle, feature::feature_set_primaries);
    send_supported_feature(resource->handle, feature::feature_set_luminances);
    send_supported_feature(resource->handle, feature::feature_windows_scrgb);

    send_supported_primaries_named(resource->handle, primaries::primaries_srgb);
    send_supported_primaries_named(resource->handle, primaries::primaries_pal_m);
    send_supported_primaries_named(resource->handle, primaries::primaries_pal);
    send_supported_primaries_named(resource->handle, primaries::primaries_ntsc);
    send_supported_primaries_named(resource->handle, primaries::primaries_generic_film);
    send_supported_primaries_named(resource->handle, primaries::primaries_bt2020);
    send_supported_primaries_named(resource->handle, primaries::primaries_cie1931_xyz);
    send_supported_primaries_named(resource->handle, primaries::primaries_dci_p3);
    send_supported_primaries_named(resource->handle, primaries::primaries_display_p3);
    send_supported_primaries_named(resource->handle, primaries::primaries_adobe_rgb);

    send_supported_tf_named(resource->handle, transfer_function::transfer_function_gamma22);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_srgb);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_st2084_pq);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_ext_linear);

    send_supported_intent(resource->handle, render_intent::render_intent_perceptual);
    send_supported_intent(resource->handle, render_intent::render_intent_relative);
    send_supported_intent(resource->handle, render_intent::render_intent_absolute);
    send_supported_intent(resource->handle, render_intent::render_intent_relative_bpc);
    // TODO implement saturation intent

    send_done(resource->handle);
}

void ColorManagerV1::wp_color_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagerV1::wp_color_manager_v1_get_output(Resource *resource, uint32_t id, struct ::wl_resource *output)
{
    new ColorManagementOutputV1(resource->client(), id, resource->version(), OutputInterface::get(output)->handle());
}

void ColorManagerV1::wp_color_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    if (priv->frogColorManagement || priv->colorSurface) {
        wl_resource_post_error(resource->handle, error_surface_exists, "there's already a color management surface for this wl_surface");
        return;
    }
    priv->colorSurface = new ColorSurfaceV1(resource->client(), id, resource->version(), surf);
}

void ColorManagerV1::wp_color_manager_v1_get_surface_feedback(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    priv->colorFeedbackSurfaces.push_back(new ColorFeedbackSurfaceV1(resource->client(), id, resource->version(), surf));
}

void ColorManagerV1::wp_color_manager_v1_create_icc_creator(Resource *resource, uint32_t obj)
{
    wl_resource_post_error(resource->handle, error::error_unsupported_feature, "ICC profiles are not supported");
}

void ColorManagerV1::wp_color_manager_v1_create_parametric_creator(Resource *resource, uint32_t obj)
{
    new ColorParametricCreatorV1(resource->client(), obj, resource->version());
}

void ColorManagerV1::wp_color_manager_v1_create_windows_scrgb(Resource *resource, uint32_t image_description)
{
    const auto scrgb = ColorDescription{
        NamedColorimetry::BT709,
        TransferFunction(TransferFunction::linear, 0, 80),
        80,
        0,
        std::nullopt,
        std::nullopt,
        Colorimetry::fromName(NamedColorimetry::BT2020),
        Colorimetry::fromName(NamedColorimetry::BT709),
    };
    new ImageDescriptionV1(resource->client(), image_description, resource->version(), scrgb);
}

ColorFeedbackSurfaceV1::ColorFeedbackSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::wp_color_management_surface_feedback_v1(client, id, version)
    , m_surface(surface)
    , m_preferred(SurfaceInterfacePrivate::get(surface)->preferredColorDescription.value_or(ColorDescription::sRGB))
{
}

ColorFeedbackSurfaceV1::~ColorFeedbackSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->colorFeedbackSurfaces.removeOne(this);
    }
}

void ColorFeedbackSurfaceV1::setPreferredColorDescription(const ColorDescription &descr)
{
    if (m_preferred != descr) {
        m_preferred = descr;
        send_preferred_changed(resource()->handle, ImageDescriptionV1::s_idCounter++);
    }
}

void ColorFeedbackSurfaceV1::wp_color_management_surface_feedback_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorFeedbackSurfaceV1::wp_color_management_surface_feedback_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorFeedbackSurfaceV1::wp_color_management_surface_feedback_v1_get_preferred(Resource *resource, uint32_t image_description)
{
    new ImageDescriptionV1(resource->client(), image_description, resource->version(), m_preferred);
}

void ColorFeedbackSurfaceV1::wp_color_management_surface_feedback_v1_get_preferred_parametric(Resource *resource, uint32_t image_description)
{
    new ImageDescriptionV1(resource->client(), image_description, resource->version(), m_preferred);
}

ColorSurfaceV1::ColorSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::wp_color_management_surface_v1(client, id, version)
    , m_surface(surface)
{
}

ColorSurfaceV1::~ColorSurfaceV1()
{
    if (m_surface) {
        const auto priv = SurfaceInterfacePrivate::get(m_surface);
        priv->pending->colorDescription = ColorDescription::sRGB;
        priv->pending->colorDescriptionIsSet = true;
        priv->colorSurface = nullptr;
    }
}

void ColorSurfaceV1::wp_color_management_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorSurfaceV1::wp_color_management_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static std::optional<RenderingIntent> waylandToKwinIntent(uint32_t intent)
{
    switch (intent) {
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_perceptual:
        return RenderingIntent::Perceptual;
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_relative:
        return RenderingIntent::RelativeColorimetric;
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_absolute:
        return RenderingIntent::AbsoluteColorimetric;
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_relative_bpc:
        return RenderingIntent::RelativeColorimetricWithBPC;
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_saturation:
        return std::nullopt;
    }
    return std::nullopt;
}

void ColorSurfaceV1::wp_color_management_surface_v1_set_image_description(Resource *resource, struct ::wl_resource *image_description, uint32_t render_intent)
{
    if (!m_surface) {
        return;
    }
    const std::optional<RenderingIntent> intent = waylandToKwinIntent(render_intent);
    if (!intent) {
        wl_resource_post_error(resource->handle, WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE, "rendering intent is not supported");
        return;
    }
    const auto description = ImageDescriptionV1::get(image_description);
    if (!description->description().has_value()) {
        wl_resource_post_error(resource->handle, error_image_description, "failed image description can't be used");
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = *ImageDescriptionV1::get(image_description)->description();
    priv->pending->renderingIntent = *intent;
    priv->pending->colorDescriptionIsSet = true;
}

void ColorSurfaceV1::wp_color_management_surface_v1_unset_image_description(Resource *resource)
{
    if (!m_surface) {
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ColorDescription::sRGB;
    priv->pending->colorDescriptionIsSet = true;
}

ColorParametricCreatorV1::ColorParametricCreatorV1(wl_client *client, uint32_t id, uint32_t version)
    : QtWaylandServer::wp_image_description_creator_params_v1(client, id, version)
{
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_create(Resource *resource, uint32_t image_description)
{
    if (!m_colorimetry || !m_transferFunctionType) {
        wl_resource_post_error(resource->handle, error::error_incomplete_set, "colorimetry or transfer function missing");
        return;
    }

    TransferFunction func{*m_transferFunctionType};
    double referenceLuminance = TransferFunction::defaultReferenceLuminanceFor(func.type);
    if (m_transferFunctionLuminances) {
        // PQ is special cased to require (max - min) == 10'000
        if (m_transferFunctionType == TransferFunction::PerceptualQuantizer) {
            func.minLuminance = m_transferFunctionLuminances->min;
            func.maxLuminance = m_transferFunctionLuminances->min + 10'000;
        } else {
            func.minLuminance = m_transferFunctionLuminances->min;
            func.maxLuminance = m_transferFunctionLuminances->max;
        }
        referenceLuminance = m_transferFunctionLuminances->reference;
    }

    std::optional<double> maxFrameAverageLuminance = m_maxFall ? m_maxFall : m_maxMasteringLuminance;
    std::optional<double> maxHdrLuminance = m_maxCll ? m_maxCll : m_maxMasteringLuminance;
    // some applications provide truly nonsensical luminance values, like 10 million nits.
    // this is just a basic sanity check to not make use of that and instead assume HGIG values
    // which are fine for most HDR games and videos
    const bool hasSaneMetadata = m_maxFall <= 10'000 && m_maxCll <= 10'000 && m_maxMasteringLuminance <= 10'000;
    if (!hasSaneMetadata) {
        maxFrameAverageLuminance = 600;
        maxHdrLuminance = 1'000;
        m_minMasteringLuminance = func.minLuminance;
    }
    if (Colorimetry::isValid(m_colorimetry->red().toxy(), m_colorimetry->green().toxy(), m_colorimetry->blue().toxy(), m_colorimetry->white().toxy())) {
        new ImageDescriptionV1(resource->client(), image_description, resource->version(), ColorDescription(*m_colorimetry, func, referenceLuminance, m_minMasteringLuminance.value_or(func.minLuminance), maxFrameAverageLuminance, maxHdrLuminance, m_masteringColorimetry, Colorimetry::fromName(NamedColorimetry::BT709)));
    } else {
        new ImageDescriptionV1(resource->client(), image_description, resource->version(), std::nullopt);
    }
    wl_resource_destroy(resource->handle);
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_tf_named(Resource *resource, uint32_t tf)
{
    if (m_transferFunctionType) {
        wl_resource_post_error(resource->handle, error::error_already_set, "transfer function is already set");
        return;
    }
    switch (tf) {
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
        m_transferFunctionType = TransferFunction::gamma22;
        return;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
        m_transferFunctionType = TransferFunction::PerceptualQuantizer;
        return;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR:
        m_transferFunctionType = TransferFunction::linear;
        return;
    default:
        // TODO add more transfer functions
        wl_resource_post_error(resource->handle, error::error_invalid_tf, "unsupported named transfer function");
    }
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_tf_power(Resource *resource, uint32_t eexp)
{
    wl_resource_post_error(resource->handle, error::error_invalid_tf, "power transfer functions are not supported");
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_primaries_named(Resource *resource, uint32_t primaries)
{
    if (m_colorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "primaries are already set");
        return;
    }
    switch (primaries) {
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_srgb:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT709);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal_m:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::PAL_M);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::PAL);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_ntsc:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::NTSC);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_generic_film:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::GenericFilm);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_bt2020:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::BT2020);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_cie1931_xyz:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::CIEXYZ);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_dci_p3:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::DCIP3);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_display_p3:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::DisplayP3);
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_adobe_rgb:
        m_colorimetry = Colorimetry::fromName(NamedColorimetry::AdobeRGB);
        return;
    default:
        wl_resource_post_error(resource->handle, error::error_invalid_primaries_named, "unsupported named primaries");
    }
}

constexpr double s_primaryUnit = 1.0 / 1'000'000.0;

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y)
{
    if (m_colorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "primaries are already set");
        return;
    }
    m_colorimetry = Colorimetry{
        xy{r_x * s_primaryUnit, r_y * s_primaryUnit},
        xy{g_x * s_primaryUnit, g_y * s_primaryUnit},
        xy{b_x * s_primaryUnit, b_y * s_primaryUnit},
        xy{w_x * s_primaryUnit, w_y * s_primaryUnit},
    };
}

constexpr double s_minLuminanceUnit = 1.0 / 10'000.0;

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_luminances(Resource *resource, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum)
{
    if (max_lum < min_lum * s_minLuminanceUnit) {
        wl_resource_post_error(resource->handle, error::error_invalid_luminance, "max. luminance is lower than the min. luminance");
        return;
    }
    if (reference_lum < min_lum * s_minLuminanceUnit) {
        wl_resource_post_error(resource->handle, error::error_invalid_luminance, "reference luminance is lower than the min. luminance");
        return;
    }
    m_transferFunctionLuminances = Luminances{
        .min = min_lum * s_minLuminanceUnit,
        .max = double(max_lum),
        .reference = double(reference_lum),
    };
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_mastering_display_primaries(Resource *resource, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y)
{
    if (m_masteringColorimetry) {
        wl_resource_post_error(resource->handle, error::error_already_set, "mastering display primaries are already set");
        return;
    }
    m_masteringColorimetry = Colorimetry{
        xy{r_x * s_primaryUnit, r_y * s_primaryUnit},
        xy{g_x * s_primaryUnit, g_y * s_primaryUnit},
        xy{b_x * s_primaryUnit, b_y * s_primaryUnit},
        xy{w_x * s_primaryUnit, w_y * s_primaryUnit},
    };
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_mastering_luminance(Resource *resource, uint32_t min_lum, uint32_t max_lum)
{
    if (m_minMasteringLuminance) {
        wl_resource_post_error(resource->handle, error::error_already_set, "mastering luminance is already set");
        return;
    }
    if (min_lum * s_minLuminanceUnit >= max_lum) {
        wl_resource_post_error(resource->handle, error::error_invalid_luminance, "min_lum can't be higher or equal to max_lum");
        return;
    }
    m_minMasteringLuminance = min_lum * s_minLuminanceUnit;
    if (max_lum > 0) {
        m_maxMasteringLuminance = max_lum;
    }
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_max_cll(Resource *resource, uint32_t max_cll)
{
    if (m_maxCll) {
        wl_resource_post_error(resource->handle, error::error_already_set, "max_cll is already set");
        return;
    }
    if (max_cll > 0) {
        m_maxCll = max_cll;
    }
}

void ColorParametricCreatorV1::wp_image_description_creator_params_v1_set_max_fall(Resource *resource, uint32_t max_fall)
{
    if (m_maxFall) {
        wl_resource_post_error(resource->handle, error::error_already_set, "max_fall is already set");
        return;
    }
    if (max_fall) {
        m_maxFall = max_fall;
    }
}

uint64_t ImageDescriptionV1::s_idCounter = 1;

ImageDescriptionV1::ImageDescriptionV1(wl_client *client, uint32_t id, uint32_t version, const std::optional<ColorDescription> &color)
    : QtWaylandServer::wp_image_description_v1(client, id, version)
    , m_description(color)
{
    if (color.has_value()) {
        send_ready(resource()->handle, s_idCounter++);
    } else {
        send_failed(resource()->handle, cause::cause_unsupported, "The provided image description failed to verify as usable");
    }
}

void ImageDescriptionV1::wp_image_description_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ImageDescriptionV1::wp_image_description_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

static uint32_t kwinTFtoProtoTF(TransferFunction tf)
{
    switch (tf.type) {
    case TransferFunction::sRGB:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
    case TransferFunction::linear:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
    case TransferFunction::PerceptualQuantizer:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
    case TransferFunction::gamma22:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    }
    Q_UNREACHABLE();
}

static uint32_t kwinPrimariesToProtoPrimaires(NamedColorimetry primaries)
{
    switch (primaries) {
    case NamedColorimetry::BT709:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_srgb;
    case NamedColorimetry::PAL_M:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal_m;
    case NamedColorimetry::PAL:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal;
    case NamedColorimetry::NTSC:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_ntsc;
    case NamedColorimetry::GenericFilm:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_generic_film;
    case NamedColorimetry::BT2020:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_bt2020;
    case NamedColorimetry::CIEXYZ:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_cie1931_xyz;
    case NamedColorimetry::DCIP3:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_dci_p3;
    case NamedColorimetry::DisplayP3:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_display_p3;
    case NamedColorimetry::AdobeRGB:
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_adobe_rgb;
    }
    Q_UNREACHABLE();
}

void ImageDescriptionV1::wp_image_description_v1_get_information(Resource *qtResource, uint32_t information)
{
    if (!m_description.has_value()) {
        wl_resource_post_error(qtResource->handle, error_no_information, "Can't request information from a failed image description");
        return;
    }
    auto resource = wl_resource_create(qtResource->client(), &wp_image_description_info_v1_interface, qtResource->version(), information);
    const auto round = [](float f) {
        return std::clamp(std::round(f / s_primaryUnit), 0.0, 1.0 / s_primaryUnit);
    };
    const xyY containerRed = m_description->containerColorimetry().red().toxyY();
    const xyY containerGreen = m_description->containerColorimetry().green().toxyY();
    const xyY containerBlue = m_description->containerColorimetry().blue().toxyY();
    const xyY containerWhite = m_description->containerColorimetry().white().toxyY();
    wp_image_description_info_v1_send_primaries(resource,
                                                round(containerRed.x), round(containerRed.y),
                                                round(containerGreen.x), round(containerGreen.y),
                                                round(containerBlue.x), round(containerBlue.y),
                                                round(containerWhite.x), round(containerWhite.y));
    if (auto name = m_description->containerColorimetry().name()) {
        wp_image_description_info_v1_send_primaries_named(resource, kwinPrimariesToProtoPrimaires(*name));
    }
    if (auto m = m_description->masteringColorimetry()) {
        const xyY masterRed = m->red().toxyY();
        const xyY masterGreen = m->green().toxyY();
        const xyY masterBlue = m->blue().toxyY();
        const xyY masterWhite = m->white().toxyY();
        wp_image_description_info_v1_send_target_primaries(resource,
                                                           round(masterRed.x), round(masterRed.y),
                                                           round(masterGreen.x), round(masterGreen.y),
                                                           round(masterBlue.x), round(masterBlue.y),
                                                           round(masterWhite.x), round(masterWhite.y));
    }
    if (auto maxfall = m_description->maxAverageLuminance()) {
        wp_image_description_info_v1_send_target_max_fall(resource, *maxfall);
    }
    if (auto maxcll = m_description->maxHdrLuminance()) {
        wp_image_description_info_v1_send_target_max_cll(resource, *maxcll);
    }
    wp_image_description_info_v1_send_luminances(resource, std::round(m_description->transferFunction().minLuminance / s_minLuminanceUnit), std::round(m_description->transferFunction().maxLuminance), std::round(m_description->referenceLuminance()));
    wp_image_description_info_v1_send_target_luminance(resource, m_description->minLuminance(), m_description->maxHdrLuminance().value_or(800));
    wp_image_description_info_v1_send_tf_named(resource, kwinTFtoProtoTF(m_description->transferFunction()));
    wp_image_description_info_v1_send_done(resource);
    wl_resource_destroy(resource);
}

const std::optional<ColorDescription> &ImageDescriptionV1::description() const
{
    return m_description;
}

ImageDescriptionV1 *ImageDescriptionV1::get(wl_resource *resource)
{
    if (auto resourceContainer = Resource::fromResource(resource)) {
        return static_cast<ImageDescriptionV1 *>(resourceContainer->object());
    } else {
        return nullptr;
    }
}

ColorManagementOutputV1::ColorManagementOutputV1(wl_client *client, uint32_t id, uint32_t version, Output *output)
    : QtWaylandServer::wp_color_management_output_v1(client, id, version)
    , m_output(output)
    , m_colorDescription(output->colorDescription())
{
    connect(output, &Output::colorDescriptionChanged, this, &ColorManagementOutputV1::colorDescriptionChanged);
}

void ColorManagementOutputV1::wp_color_management_output_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void ColorManagementOutputV1::wp_color_management_output_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ColorManagementOutputV1::wp_color_management_output_v1_get_image_description(Resource *resource, uint32_t image_description)
{
    new ImageDescriptionV1(resource->client(), image_description, resource->version(), m_colorDescription);
}

void ColorManagementOutputV1::colorDescriptionChanged()
{
    m_colorDescription = m_output->colorDescription();
    send_image_description_changed();
}

}

#include "moc_colormanagement_v1.cpp"
