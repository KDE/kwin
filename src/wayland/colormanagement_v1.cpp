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
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_st2084_pq);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_ext_linear);
    send_supported_tf_named(resource->handle, transfer_function::transfer_function_bt1886);

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
    new ColorManagementOutputV1(resource->client(), id, resource->version(), OutputInterface::get(output));
}

void ColorManagerV1::wp_color_manager_v1_get_surface(Resource *resource, uint32_t id, struct ::wl_resource *surface)
{
    const auto surf = SurfaceInterface::get(surface);
    const auto priv = SurfaceInterfacePrivate::get(surf);
    if (priv->colorSurface) {
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
    const auto scrgb = std::make_shared<ColorDescription>(ColorDescription{
        Colorimetry::BT709,
        TransferFunction(TransferFunction::linear, 0, 80),
        203,
        0,
        std::nullopt,
        std::nullopt,
        Colorimetry::BT2020,
        Colorimetry::BT709,
    });
    ImageDescriptionV1::createReady(resource->client(), image_description, resource->version(), scrgb);
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

void ColorFeedbackSurfaceV1::setPreferredColorDescription(const std::shared_ptr<ColorDescription> &descr)
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
    ImageDescriptionV1::createReady(resource->client(), image_description, resource->version(), m_preferred);
}

void ColorFeedbackSurfaceV1::wp_color_management_surface_feedback_v1_get_preferred_parametric(Resource *resource, uint32_t image_description)
{
    ImageDescriptionV1::createReady(resource->client(), image_description, resource->version(), m_preferred);
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
        priv->pending->committed |= SurfaceState::Field::ColorDescription;
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
    case QtWaylandServer::wp_color_manager_v1::render_intent::render_intent_absolute:
        return RenderingIntent::RelativeColorimetric;
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
        wl_resource_post_error(resource->handle, WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_INERT, "the associated surface was already destroyed");
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
    priv->pending->committed |= SurfaceState::Field::ColorDescription;
}

void ColorSurfaceV1::wp_color_management_surface_v1_unset_image_description(Resource *resource)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_INERT, "the associated surface was already destroyed");
        return;
    }
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->colorDescription = ColorDescription::sRGB;
    priv->pending->committed |= SurfaceState::Field::ColorDescription;
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
        ImageDescriptionV1::createReady(resource->client(), image_description, resource->version(), std::make_shared<ColorDescription>(ColorDescription{
                                                                                                        *m_colorimetry,
                                                                                                        func,
                                                                                                        referenceLuminance,
                                                                                                        m_minMasteringLuminance.value_or(func.minLuminance),
                                                                                                        maxFrameAverageLuminance,
                                                                                                        maxHdrLuminance.value_or(func.maxLuminance),
                                                                                                        m_masteringColorimetry.value_or(*m_colorimetry),
                                                                                                        Colorimetry::BT709,
                                                                                                    }));
    } else {
        ImageDescriptionV1::createFailed(resource->client(), image_description, resource->version(), WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, QStringLiteral("The provided image description failed to verify as usable"));
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
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
        m_transferFunctionType = TransferFunction::gamma22;
        return;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
        m_transferFunctionType = TransferFunction::PerceptualQuantizer;
        return;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR:
        m_transferFunctionType = TransferFunction::linear;
        return;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886:
        m_transferFunctionType = TransferFunction::BT1886;
        return;
        // intentionally not supported - it's confusing and
        // deprecated in version 2 of the protocol
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
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
        m_colorimetry = Colorimetry::BT709;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal_m:
        m_colorimetry = Colorimetry::PAL_M;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal:
        m_colorimetry = Colorimetry::PAL;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_ntsc:
        m_colorimetry = Colorimetry::NTSC;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_generic_film:
        m_colorimetry = Colorimetry::GenericFilm;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_bt2020:
        m_colorimetry = Colorimetry::BT2020;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_cie1931_xyz:
        m_colorimetry = Colorimetry::CIEXYZ;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_dci_p3:
        m_colorimetry = Colorimetry::DCIP3;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_display_p3:
        m_colorimetry = Colorimetry::DisplayP3;
        return;
    case QtWaylandServer::wp_color_manager_v1::primaries::primaries_adobe_rgb:
        m_colorimetry = Colorimetry::AdobeRGB;
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

ImageDescriptionV1 *ImageDescriptionV1::createReady(wl_client *client, uint32_t id, uint32_t version, const std::shared_ptr<ColorDescription> &colorDescription)
{
    auto description = new ImageDescriptionV1(client, id, version, colorDescription);
    description->send_ready(s_idCounter++);
    return description;
}

ImageDescriptionV1 *ImageDescriptionV1::createFailed(wl_client *client, uint32_t id, uint32_t version, wp_image_description_v1_cause error, const QString &message)
{
    auto description = new ImageDescriptionV1(client, id, version, std::nullopt);
    description->send_failed(error, message);
    return description;
}

ImageDescriptionV1::ImageDescriptionV1(wl_client *client, uint32_t id, uint32_t version, const std::optional<std::shared_ptr<ColorDescription>> &color)
    : QtWaylandServer::wp_image_description_v1(client, id, version)
    , m_description(color)
{
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
    case TransferFunction::linear:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
    case TransferFunction::PerceptualQuantizer:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
        // not "correct", but the piece-wise TF shouldn't be used anyways
    case TransferFunction::sRGB:
    case TransferFunction::gamma22:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    case TransferFunction::BT1886:
        return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
    }
    Q_UNREACHABLE();
}

static bool compareXY(double left, double right)
{
    return std::round(left / s_primaryUnit) == std::round(right / s_primaryUnit);
}

static bool compareXY(xy left, xy right)
{
    return compareXY(left.x, right.x) && compareXY(left.y, right.y);
}

static bool comparePrimaries(const Colorimetry &left, const Colorimetry &right)
{
    // this can't just use Colorimetry::operator==
    // as that has a different resolution from the Wayland protocol
    return compareXY(left.red().toxy(), right.red().toxy())
        && compareXY(left.green().toxy(), right.green().toxy())
        && compareXY(left.blue().toxy(), right.blue().toxy())
        && compareXY(left.white().toxy(), right.white().toxy());
}

static std::optional<uint32_t> kwinPrimariesToProtoPrimaires(const Colorimetry &primaries)
{
    if (comparePrimaries(primaries, Colorimetry::BT709)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_srgb;
    } else if (comparePrimaries(primaries, Colorimetry::PAL_M)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal_m;
    } else if (comparePrimaries(primaries, Colorimetry::PAL)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_pal;
    } else if (comparePrimaries(primaries, Colorimetry::NTSC)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_ntsc;
    } else if (comparePrimaries(primaries, Colorimetry::GenericFilm)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_generic_film;
    } else if (comparePrimaries(primaries, Colorimetry::BT2020)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_bt2020;
    } else if (comparePrimaries(primaries, Colorimetry::CIEXYZ)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_cie1931_xyz;
    } else if (comparePrimaries(primaries, Colorimetry::DCIP3)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_dci_p3;
    } else if (comparePrimaries(primaries, Colorimetry::DisplayP3)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_display_p3;
    } else if (comparePrimaries(primaries, Colorimetry::AdobeRGB)) {
        return QtWaylandServer::wp_color_manager_v1::primaries::primaries_adobe_rgb;
    } else {
        return std::nullopt;
    }
}

void ImageDescriptionV1::wp_image_description_v1_get_information(Resource *qtResource, uint32_t information)
{
    if (!m_description.has_value()) {
        wl_resource_post_error(qtResource->handle, error_no_information, "Can't request information from a failed image description");
        return;
    }
    const auto &color = *m_description;
    auto resource = wl_resource_create(qtResource->client(), &wp_image_description_info_v1_interface, qtResource->version(), information);
    const auto round = [](float f) {
        return std::clamp(std::round(f / s_primaryUnit), 0.0, 1.0 / s_primaryUnit);
    };
    const xyY containerRed = color->containerColorimetry().red().toxyY();
    const xyY containerGreen = color->containerColorimetry().green().toxyY();
    const xyY containerBlue = color->containerColorimetry().blue().toxyY();
    const xyY containerWhite = color->containerColorimetry().white().toxyY();
    wp_image_description_info_v1_send_primaries(resource,
                                                round(containerRed.x), round(containerRed.y),
                                                round(containerGreen.x), round(containerGreen.y),
                                                round(containerBlue.x), round(containerBlue.y),
                                                round(containerWhite.x), round(containerWhite.y));
    if (auto name = kwinPrimariesToProtoPrimaires(color->containerColorimetry())) {
        wp_image_description_info_v1_send_primaries_named(resource, *name);
    }
    const xyY masterRed = color->masteringColorimetry().red().toxyY();
    const xyY masterGreen = color->masteringColorimetry().green().toxyY();
    const xyY masterBlue = color->masteringColorimetry().blue().toxyY();
    const xyY masterWhite = color->masteringColorimetry().white().toxyY();
    wp_image_description_info_v1_send_target_primaries(resource,
                                                       round(masterRed.x), round(masterRed.y),
                                                       round(masterGreen.x), round(masterGreen.y),
                                                       round(masterBlue.x), round(masterBlue.y),
                                                       round(masterWhite.x), round(masterWhite.y));
    if (auto maxfall = color->maxAverageLuminance()) {
        wp_image_description_info_v1_send_target_max_fall(resource, std::round(*maxfall));
    }
    if (auto maxcll = color->maxHdrLuminance()) {
        wp_image_description_info_v1_send_target_max_cll(resource, std::round(*maxcll));
    }
    wp_image_description_info_v1_send_luminances(resource, std::round(color->transferFunction().minLuminance / s_minLuminanceUnit), std::round(color->transferFunction().maxLuminance), std::round(color->referenceLuminance()));
    wp_image_description_info_v1_send_target_luminance(resource, std::round(color->minLuminance() / s_minLuminanceUnit), std::round(color->maxHdrLuminance().value_or(800)));
    wp_image_description_info_v1_send_tf_named(resource, kwinTFtoProtoTF(color->transferFunction()));
    wp_image_description_info_v1_send_done(resource);
    wl_resource_destroy(resource);
}

const std::optional<std::shared_ptr<ColorDescription>> &ImageDescriptionV1::description() const
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

ColorManagementOutputV1::ColorManagementOutputV1(wl_client *client, uint32_t id, uint32_t version, OutputInterface *output)
    : QtWaylandServer::wp_color_management_output_v1(client, id, version)
    , m_output(output)
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    connect(output->handle(), &Output::colorDescriptionChanged, this, &ColorManagementOutputV1::colorDescriptionChanged);
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
    if (!m_output || m_output->isRemoved()) {
        ImageDescriptionV1::createFailed(resource->client(), image_description, resource->version(), WP_IMAGE_DESCRIPTION_V1_CAUSE_NO_OUTPUT, QStringLiteral("wl_output has been removed"));
    } else {
        ImageDescriptionV1::createReady(resource->client(), image_description, resource->version(), m_output->handle()->colorDescription());
    }
}

void ColorManagementOutputV1::colorDescriptionChanged()
{
    if (!m_output || m_output->isRemoved()) {
        return;
    }

    send_image_description_changed();
}

}

#include "moc_colormanagement_v1.cpp"
