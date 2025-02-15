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
    }
    Q_UNREACHABLE();
}

static constexpr double s_primariesUnit = 1.0 / 1'000'000.0;

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
        wp_image_description_creator_params_v1_set_luminances(creator, std::round(10'000 * color.transferFunction().minLuminance), std::round(color.transferFunction().maxLuminance), std::round(color.referenceLuminance()));
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
        wp_image_description_creator_params_v1_set_mastering_luminance(creator, std::round(10'000 * color.minLuminance()), std::round(*color.maxHdrLuminance()));
    }
    // TODO this assumes that the compositor immediately creates the image description
    // doesn't matter for KWin, but might be nice to fix for other compositors
    return wp_image_description_creator_params_v1_create(creator);
}

}
}
