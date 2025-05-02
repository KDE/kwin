/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorspace.h"
#include "wayland-color-management-v1-client-protocol.h"

#include <QList>
#include <QObject>

namespace KWin
{
namespace Wayland
{

class ColorManager
{
public:
    explicit ColorManager(wp_color_manager_v1 *obj);
    ~ColorManager();

    bool supportsFeature(wp_color_manager_v1_feature feature);
    bool supportsPrimaries(wp_color_manager_v1_primaries primaries);
    bool supportsTransferFunction(wp_color_manager_v1_transfer_function tf);

    wp_color_manager_v1 *object() const;
    wp_image_description_v1 *createImageDescription(const ColorDescription &color);

private:
    static const wp_color_manager_v1_listener s_listener;
    static void supportedFeature(void *data, wp_color_manager_v1 *manager, uint32_t feature);
    static void supportedIntent(void *data, wp_color_manager_v1 *manager, uint32_t intent);
    static void supportedPrimaries(void *data, wp_color_manager_v1 *manager, uint32_t primaries);
    static void supportedTransferFunction(void *data, wp_color_manager_v1 *manager, uint32_t transferFunction);
    static void done(void *data, wp_color_manager_v1 *manager);

    wp_color_manager_v1 *const m_object;
    QList<wp_color_manager_v1_feature> m_features;
    QList<wp_color_manager_v1_primaries> m_primaries;
    QList<wp_color_manager_v1_transfer_function> m_transferFunctions;
};

class ImageDescriptionListener : public QObject
{
    Q_OBJECT
public:
    explicit ImageDescriptionListener(wp_image_description_info_v1 *info);

Q_SIGNALS:
    void done(const ColorDescription &descr);

private:
    static const wp_image_description_info_v1_listener s_listener;

    Colorimetry m_colorimetry = Colorimetry::BT709;
    std::optional<TransferFunction::Type> m_transferFunctionType;
    struct Luminances
    {
        double min;
        double max;
        double reference;
    };
    std::optional<Luminances> m_transferFunctionLuminances;

    // mastering display information
    std::optional<Colorimetry> m_masteringColorimetry;
    std::optional<double> m_minMasteringLuminance;
    std::optional<double> m_maxMasteringLuminance;
    std::optional<double> m_maxCll;
    std::optional<double> m_maxFall;
};

class ColorSurfaceFeedback : public QObject
{
    Q_OBJECT
public:
    explicit ColorSurfaceFeedback(wp_color_management_surface_feedback_v1 *);
    ~ColorSurfaceFeedback();

    const ColorDescription &preferredColor() const;

Q_SIGNALS:
    void preferredColorChanged();

private:
    static const wp_color_management_surface_feedback_v1_listener s_listener;
    void setPreferredColor(const ColorDescription &color);

    wp_color_management_surface_feedback_v1 *const m_feedback;
    ColorDescription m_preferred = ColorDescription::sRGB;
};
}
}
