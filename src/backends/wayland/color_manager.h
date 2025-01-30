/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorspace.h"
#include "wayland-xx-color-management-v4-client-protocol.h"

#include <QList>

namespace KWin
{
namespace Wayland
{

class ColorManager
{
public:
    explicit ColorManager(xx_color_manager_v4 *obj);
    ~ColorManager();

    bool supportsFeature(xx_color_manager_v4_feature feature);
    bool supportsPrimaries(xx_color_manager_v4_primaries primaries);
    bool supportsTransferFunction(xx_color_manager_v4_transfer_function tf);

    xx_color_manager_v4 *object() const;
    xx_image_description_v4 *createImageDescription(const ColorDescription &color);

private:
    static const xx_color_manager_v4_listener s_listener;
    static void supportedFeature(void *data, xx_color_manager_v4 *manager, uint32_t feature);
    static void supportedIntent(void *data, xx_color_manager_v4 *manager, uint32_t intent);
    static void supportedPrimaries(void *data, xx_color_manager_v4 *manager, uint32_t primaries);
    static void supportedTransferFunction(void *data, xx_color_manager_v4 *manager, uint32_t transferFunction);

    xx_color_manager_v4 *const m_object;
    QList<xx_color_manager_v4_feature> m_features;
    QList<xx_color_manager_v4_primaries> m_primaries;
    QList<xx_color_manager_v4_transfer_function> m_transferFunctions;
};

}
}
