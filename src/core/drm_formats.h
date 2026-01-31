/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023-2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QHash>
#include <QList>
#include <QString>
#include <flat_set>
#include <unordered_map>
#include <vector>

#include <epoxy/gl.h>
#include <libdrm/drm_fourcc.h>
#include <optional>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace KWin
{

using ModifierList = std::flat_set<uint64_t, std::less<uint64_t>, std::vector<uint64_t>>;
// to be able to pass the modifier list to gbm (as a pointer), it must be contiguous
static_assert(std::contiguous_iterator<decltype(ModifierList{}.begin())>);
using FormatModifierMap = QHash<uint32_t, ModifierList>;

KWIN_EXPORT ModifierList intersect(const ModifierList &one, const ModifierList &two);

struct YuvFormat
{
    uint32_t format = DRM_FORMAT_YUYV;
    uint32_t widthDivisor = 1;
    uint32_t heightDivisor = 1;
};
struct YuvConversion
{
    QList<struct YuvFormat> plane = {};
};

struct KWIN_EXPORT FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;
    VkFormat vulkanFormat;
    bool floatingPoint;

    static const QHash<uint32_t, YuvConversion> s_drmConversions;
    static const std::unordered_map<uint32_t, FormatInfo> s_knownFormats;

    std::optional<YuvConversion> yuvConversion() const
    {
        const auto it = s_drmConversions.find(drmFormat);
        return it != s_drmConversions.end() ? *it : std::optional<YuvConversion>{};
    }

    static std::optional<FormatInfo> get(uint32_t drmFormat);
    static QString drmFormatName(uint32_t format);
};
}
