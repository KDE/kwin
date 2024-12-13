/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "color_manager.h"

namespace KWin
{
namespace Wayland
{

void ColorManager::supportedFeature(void *data, wp_color_manager_v1 *manager, uint32_t feature)
{
    static_cast<ColorManager *>(data)->m_features.push_back(static_cast<wp_color_manager_v1_feature>(feature));
}

void ColorManager::supportedIntent(void *data, wp_color_manager_v1 *manager, uint32_t intent)
{
    // unused
}

void ColorManager::supportedPrimaries(void *data, wp_color_manager_v1 *manager, uint32_t primaries)
{
    static_cast<ColorManager *>(data)->m_primaries.push_back(static_cast<wp_color_manager_v1_primaries>(primaries));
}

void ColorManager::supportedTransferFunction(void *data, wp_color_manager_v1 *manager, uint32_t transferFunction)
{
    static_cast<ColorManager *>(data)->m_transferFunctions.push_back(static_cast<wp_color_manager_v1_transfer_function>(transferFunction));
}

void ColorManager::done(void *data, wp_color_manager_v1 *manager)
{
}

constexpr wp_color_manager_v1_listener ColorManager::s_listener{
    .supported_intent = supportedIntent,
    .supported_feature = supportedFeature,
    .supported_tf_named = supportedTransferFunction,
    .supported_primaries_named = supportedPrimaries,
    .done = done,
};

ColorManager::ColorManager(wp_color_manager_v1 *obj)
    : m_object(obj)
{
    wp_color_manager_v1_add_listener(obj, &s_listener, this);
}

ColorManager::~ColorManager()
{
    wp_color_manager_v1_destroy(m_object);
}

bool ColorManager::supportsFeature(wp_color_manager_v1_feature feature)
{
    return m_features.contains(feature);
}

bool ColorManager::supportsPrimaries(wp_color_manager_v1_primaries primaries)
{
    return m_primaries.contains(primaries);
}

bool ColorManager::supportsTransferFunction(wp_color_manager_v1_transfer_function tf)
{
    return m_transferFunctions.contains(tf);
}

wp_color_manager_v1 *ColorManager::object() const
{
    return m_object;
}

static std::optional<uint32_t> kwinPrimariesToProtoPrimaires(const Colorimetry &color)
{
    if (color == Colorimetry::BT709) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
    } else if (color == Colorimetry::PAL_M) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M;
    } else if (color == Colorimetry::PAL) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_PAL;
    } else if (color == Colorimetry::NTSC) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_NTSC;
    } else if (color == Colorimetry::GenericFilm) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM;
    } else if (color == Colorimetry::BT2020) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_BT2020;
    } else if (color == Colorimetry::CIEXYZ) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ;
    } else if (color == Colorimetry::DCIP3) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3;
    } else if (color == Colorimetry::DisplayP3) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3;
    } else if (color == Colorimetry::AdobeRGB) {
        return WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB;
    } else {
        return std::nullopt;
    }
}

static uint32_t kwinTFtoProtoTF(TransferFunction tf)
{
    switch (tf.type) {
    case TransferFunction::sRGB:
        return wp_color_manager_v1_transfer_function::WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
    case TransferFunction::linear:
        return wp_color_manager_v1_transfer_function::WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
    case TransferFunction::PerceptualQuantizer:
        return wp_color_manager_v1_transfer_function::WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
    case TransferFunction::gamma22:
        return wp_color_manager_v1_transfer_function::WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    case TransferFunction::BT1886:
        return wp_color_manager_v1_transfer_function::WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
    }
    Q_UNREACHABLE();
}

static constexpr double s_primariesUnit = 1.0 / 1'000'000.0;
static constexpr double s_minLuminanceUnit = 1.0 / 10'000.0;

wp_image_description_v1 *ColorManager::createImageDescription(const ColorDescription &color)
{
    const auto encode = [](double primary) {
        return std::round(primary / s_primariesUnit);
    };

    auto creator = wp_color_manager_v1_create_parametric_creator(object());
    if (auto primaries = kwinPrimariesToProtoPrimaires(color.containerColorimetry())) {
        wp_image_description_creator_params_v1_set_primaries_named(creator, *primaries);
    } else {
        const auto &c = color.containerColorimetry();
        wp_image_description_creator_params_v1_set_primaries(creator,
                                                             encode(c.red().toxy().x), encode(c.red().toxy().y),
                                                             encode(c.green().toxy().x), encode(c.green().toxy().y),
                                                             encode(c.blue().toxy().x), encode(c.blue().toxy().y),
                                                             encode(c.white().toxy().x), encode(c.white().toxy().y));
    }
    wp_image_description_creator_params_v1_set_tf_named(creator, kwinTFtoProtoTF(color.transferFunction()));
    if (supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)) {
        wp_image_description_creator_params_v1_set_luminances(creator, std::round(color.transferFunction().minLuminance / s_minLuminanceUnit), std::round(color.transferFunction().maxLuminance), std::round(color.referenceLuminance()));
    }
    if (supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES) && color.masteringColorimetry().has_value()) {
        const auto &c = *color.masteringColorimetry();
        wp_image_description_creator_params_v1_set_mastering_display_primaries(creator,
                                                                               encode(c.red().toxy().x), encode(c.red().toxy().y),
                                                                               encode(c.green().toxy().x), encode(c.green().toxy().y),
                                                                               encode(c.blue().toxy().x), encode(c.blue().toxy().y),
                                                                               encode(c.white().toxy().x), encode(c.white().toxy().y));
    }
    if (supportsFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES) && color.maxHdrLuminance()) {
        wp_image_description_creator_params_v1_set_mastering_luminance(creator, std::round(color.minLuminance() / s_minLuminanceUnit), std::round(*color.maxHdrLuminance()));
    }
    // TODO this assumes that the compositor immediately creates the image description
    // doesn't matter for KWin, but might be nice to fix for other compositors
    return wp_image_description_creator_params_v1_create(creator);
}

// it doesn't currently handle this as expected, see https://github.com/llvm/llvm-project/issues/138300
// clang-format off
constexpr wp_image_description_info_v1_listener ImageDescriptionListener::s_listener = {
    .done = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        const auto tfType = listener->m_transferFunctionType.value_or(TransferFunction::gamma22);
        const auto luminances = listener->m_transferFunctionLuminances.value_or(Luminances{
            .min = TransferFunction::defaultMinLuminanceFor(tfType),
            .max = TransferFunction::defaultMaxLuminanceFor(tfType),
            .reference = TransferFunction::defaultReferenceLuminanceFor(tfType),
        });
        Q_EMIT listener->done(ColorDescription{
            listener->m_colorimetry,
            TransferFunction(tfType, luminances.min, luminances.max),
            luminances.reference,
            listener->m_minMasteringLuminance.value_or(luminances.min),
            listener->m_maxFall.value_or(luminances.max),
            listener->m_maxCll.value_or(listener->m_maxMasteringLuminance.value_or(luminances.max)),
        });
        delete listener;
    },
    .icc_file = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, int32_t icc, uint32_t icc_size) {
        // intentionally ignored
    },
    .primaries = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        listener->m_colorimetry = Colorimetry{
            xy{r_x * s_primariesUnit, r_y * s_primariesUnit},
            xy{g_x * s_primariesUnit, g_y * s_primariesUnit},
            xy{b_x * s_primariesUnit, b_y * s_primariesUnit},
            xy{w_x * s_primariesUnit, w_y * s_primariesUnit},
        };
    },
    .primaries_named = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t primaries) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        switch (primaries) {
        case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB:
            listener->m_colorimetry = Colorimetry::BT709;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M:
            listener->m_colorimetry = Colorimetry::PAL_M;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_PAL:
            listener->m_colorimetry = Colorimetry::PAL;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_NTSC:
            listener->m_colorimetry = Colorimetry::NTSC;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM:
            listener->m_colorimetry = Colorimetry::GenericFilm;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020:
            listener->m_colorimetry = Colorimetry::BT2020;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ:
            listener->m_colorimetry = Colorimetry::CIEXYZ;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3:
            listener->m_colorimetry = Colorimetry::DCIP3;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3:
            listener->m_colorimetry = Colorimetry::DisplayP3;
            return;
        case WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB:
            listener->m_colorimetry = Colorimetry::AdobeRGB;
            return;
        }
    },
    .tf_power = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t eexp) {
        // not currently supported
    },
    .tf_named = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t tf) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        switch (tf) {
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
            listener->m_transferFunctionType = TransferFunction::gamma22;
            break;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
            listener->m_transferFunctionType = TransferFunction::PerceptualQuantizer;
            break;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR:
            listener->m_transferFunctionType = TransferFunction::linear;
            break;
        case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886:
            listener->m_transferFunctionType = TransferFunction::BT1886;
            break;
        }
    },
    .luminances = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        listener->m_transferFunctionLuminances = {
            .min = double(min_lum * s_minLuminanceUnit),
            .max = double(max_lum),
            .reference = double(reference_lum),
        };
    },
    .target_primaries = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        listener->m_masteringColorimetry = Colorimetry{
            xy{r_x / s_primariesUnit, r_y / s_primariesUnit},
            xy{g_x / s_primariesUnit, g_y / s_primariesUnit},
            xy{b_x / s_primariesUnit, b_y / s_primariesUnit},
            xy{w_x / s_primariesUnit, w_y / s_primariesUnit},
        };
    },
    .target_luminance = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t min_lum, uint32_t max_lum) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        if (min_lum != 0) {
            listener->m_minMasteringLuminance = min_lum * s_minLuminanceUnit;
        }
        if (max_lum != 0) {
            listener->m_maxMasteringLuminance = max_lum;
        }
    },
    .target_max_cll = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t max_cll) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        if (max_cll != 0) {
            listener->m_maxCll = max_cll;
        }
    },
    .target_max_fall = [](void *data, wp_image_description_info_v1 *wp_image_description_info_v1, uint32_t max_fall) {
        const auto listener = static_cast<ImageDescriptionListener *>(data);
        if (max_fall != 0) {
            listener->m_maxFall = max_fall;
        }
    },
};

ImageDescriptionListener::ImageDescriptionListener(wp_image_description_info_v1 *info)
{
    wp_image_description_info_v1_add_listener(info, &s_listener, this);
}

constexpr wp_color_management_surface_feedback_v1_listener ColorSurfaceFeedback::s_listener = {
    .preferred_changed = [](void *data, wp_color_management_surface_feedback_v1 *wp_color_management_surface_feedback_v1, uint32_t identity) {
        const auto feedback = static_cast<ColorSurfaceFeedback *>(data);
        wp_image_description_v1 *img = wp_color_management_surface_feedback_v1_get_preferred_parametric(wp_color_management_surface_feedback_v1);
        auto listener = new ImageDescriptionListener(wp_image_description_v1_get_information(img));
        connect(listener, &ImageDescriptionListener::done, feedback, &ColorSurfaceFeedback::setPreferredColor);
        wp_image_description_v1_destroy(img);
    },
};
// clang-format on

ColorSurfaceFeedback::ColorSurfaceFeedback(wp_color_management_surface_feedback_v1 *feedback)
    : m_feedback(feedback)
{
    wp_color_management_surface_feedback_v1_add_listener(m_feedback, &s_listener, this);
}

ColorSurfaceFeedback::~ColorSurfaceFeedback()
{
    wp_color_management_surface_feedback_v1_destroy(m_feedback);
}

void ColorSurfaceFeedback::setPreferredColor(const ColorDescription &color)
{
    if (color != m_preferred) {
        m_preferred = color;
        Q_EMIT preferredColorChanged();
    }
}

const ColorDescription &ColorSurfaceFeedback::preferredColor() const
{
    return m_preferred;
}
}
}
