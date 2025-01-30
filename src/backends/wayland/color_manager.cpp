/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "color_manager.h"

namespace KWin
{
namespace Wayland
{

void ColorManager::supportedFeature(void *data, xx_color_manager_v4 *manager, uint32_t feature)
{
    static_cast<ColorManager *>(data)->m_features.push_back(static_cast<xx_color_manager_v4_feature>(feature));
}

void ColorManager::supportedIntent(void *data, xx_color_manager_v4 *manager, uint32_t intent)
{
    // unused
}

void ColorManager::supportedPrimaries(void *data, xx_color_manager_v4 *manager, uint32_t primaries)
{
    static_cast<ColorManager *>(data)->m_primaries.push_back(static_cast<xx_color_manager_v4_primaries>(primaries));
}

void ColorManager::supportedTransferFunction(void *data, xx_color_manager_v4 *manager, uint32_t transferFunction)
{
    static_cast<ColorManager *>(data)->m_transferFunctions.push_back(static_cast<xx_color_manager_v4_transfer_function>(transferFunction));
}

constexpr xx_color_manager_v4_listener ColorManager::s_listener{
    .supported_intent = supportedIntent,
    .supported_feature = supportedFeature,
    .supported_tf_named = supportedTransferFunction,
    .supported_primaries_named = supportedPrimaries,
};

ColorManager::ColorManager(xx_color_manager_v4 *obj)
    : m_object(obj)
{
    xx_color_manager_v4_add_listener(obj, &s_listener, this);
}

ColorManager::~ColorManager()
{
    xx_color_manager_v4_destroy(m_object);
}

bool ColorManager::supportsFeature(xx_color_manager_v4_feature feature)
{
    return m_features.contains(feature);
}

bool ColorManager::supportsPrimaries(xx_color_manager_v4_primaries primaries)
{
    return m_primaries.contains(primaries);
}

bool ColorManager::supportsTransferFunction(xx_color_manager_v4_transfer_function tf)
{
    return m_transferFunctions.contains(tf);
}

xx_color_manager_v4 *ColorManager::object() const
{
    return m_object;
}

static std::optional<uint32_t> kwinPrimariesToProtoPrimaires(const Colorimetry &color)
{
    const auto primaries = color.name();
    if (!primaries) {
        return std::nullopt;
    }
    switch (*primaries) {
    case NamedColorimetry::BT709:
        return XX_COLOR_MANAGER_V4_PRIMARIES_SRGB;
    case NamedColorimetry::PAL_M:
        return XX_COLOR_MANAGER_V4_PRIMARIES_PAL_M;
    case NamedColorimetry::PAL:
        return XX_COLOR_MANAGER_V4_PRIMARIES_PAL;
    case NamedColorimetry::NTSC:
        return XX_COLOR_MANAGER_V4_PRIMARIES_NTSC;
    case NamedColorimetry::GenericFilm:
        return XX_COLOR_MANAGER_V4_PRIMARIES_GENERIC_FILM;
    case NamedColorimetry::BT2020:
        return XX_COLOR_MANAGER_V4_PRIMARIES_BT2020;
    case NamedColorimetry::CIEXYZ:
        return XX_COLOR_MANAGER_V4_PRIMARIES_CIE1931_XYZ;
    case NamedColorimetry::DCIP3:
        return XX_COLOR_MANAGER_V4_PRIMARIES_DCI_P3;
    case NamedColorimetry::DisplayP3:
        return XX_COLOR_MANAGER_V4_PRIMARIES_DISPLAY_P3;
    case NamedColorimetry::AdobeRGB:
        return XX_COLOR_MANAGER_V4_PRIMARIES_ADOBE_RGB;
    case NamedColorimetry::CIEXYZD50:
        return std::nullopt;
    }
    Q_UNREACHABLE();
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

static constexpr double s_primariesUnit = 1.0 / 10'000.0;

xx_image_description_v4 *ColorManager::createImageDescription(const ColorDescription &color)
{
    const auto encode = [](double primary) {
        return std::round(primary / s_primariesUnit);
    };

    auto creator = xx_color_manager_v4_new_parametric_creator(object());
    if (auto primaries = kwinPrimariesToProtoPrimaires(color.containerColorimetry())) {
        xx_image_description_creator_params_v4_set_primaries_named(creator, *primaries);
    } else {
        const auto &c = color.containerColorimetry();
        xx_image_description_creator_params_v4_set_primaries(creator,
                                                             encode(c.red().toxy().x), encode(c.red().toxy().y),
                                                             encode(c.green().toxy().x), encode(c.green().toxy().y),
                                                             encode(c.blue().toxy().x), encode(c.blue().toxy().y),
                                                             encode(c.white().toxy().x), encode(c.white().toxy().y));
    }
    xx_image_description_creator_params_v4_set_tf_named(creator, kwinTFtoProtoTF(color.transferFunction()));
    if (supportsFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_LUMINANCES)) {
        xx_image_description_creator_params_v4_set_luminances(creator, std::round(10'000 * color.transferFunction().minLuminance), std::round(color.transferFunction().maxLuminance), std::round(color.referenceLuminance()));
    }
    if (supportsFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES) && color.masteringColorimetry().has_value()) {
        const auto &c = *color.masteringColorimetry();
        xx_image_description_creator_params_v4_set_mastering_display_primaries(creator,
                                                                               encode(c.red().toxy().x), encode(c.red().toxy().y),
                                                                               encode(c.green().toxy().x), encode(c.green().toxy().y),
                                                                               encode(c.blue().toxy().x), encode(c.blue().toxy().y),
                                                                               encode(c.white().toxy().x), encode(c.white().toxy().y));
    }
    if (supportsFeature(XX_COLOR_MANAGER_V4_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES) && color.maxHdrLuminance()) {
        xx_image_description_creator_params_v4_set_mastering_luminance(creator, std::round(10'000 * color.minLuminance()), std::round(*color.maxHdrLuminance()));
    }
    // TODO this assumes that the compositor immediately creates the image description
    // doesn't matter for KWin, but might be nice to fix for other compositors
    return xx_image_description_creator_params_v4_create(creator);
}

}
}
