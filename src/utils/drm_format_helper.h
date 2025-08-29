/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "vulkan/vulkan_include.h"

#include <QHash>
#include <QList>
#include <QString>

#include <array>
#include <epoxy/gl.h>
#include <libdrm/drm_fourcc.h>
#include <optional>
#include <stdint.h>

namespace KWin
{

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

static const QHash<uint32_t, YuvConversion> s_drmConversions = {
    {DRM_FORMAT_NV12, YuvConversion{
                          {YuvFormat{DRM_FORMAT_R8, 1, 1}, YuvFormat{DRM_FORMAT_GR88, 2, 2}},
                      }},
    {DRM_FORMAT_P010, YuvConversion{
                          {YuvFormat{DRM_FORMAT_R16, 1, 1}, YuvFormat{DRM_FORMAT_GR1616, 2, 2}},
                      }},
};

static constexpr std::array s_knownDrmFormats = {
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_XBGR8888,
    DRM_FORMAT_RGBX8888,
    DRM_FORMAT_BGRX8888,
    DRM_FORMAT_ARGB8888,
    DRM_FORMAT_ABGR8888,
    DRM_FORMAT_RGBA8888,
    DRM_FORMAT_BGRA8888,
    DRM_FORMAT_XRGB2101010,
    DRM_FORMAT_XBGR2101010,
    DRM_FORMAT_RGBX1010102,
    DRM_FORMAT_BGRX1010102,
    DRM_FORMAT_ARGB2101010,
    DRM_FORMAT_ABGR2101010,
    DRM_FORMAT_RGBA1010102,
    DRM_FORMAT_BGRA1010102,
    DRM_FORMAT_XRGB16161616F,
    DRM_FORMAT_ARGB16161616F,
    DRM_FORMAT_XBGR16161616F,
    DRM_FORMAT_ABGR16161616F,
    DRM_FORMAT_ARGB4444,
    DRM_FORMAT_ABGR4444,
    DRM_FORMAT_RGBA4444,
    DRM_FORMAT_BGRA4444,
    DRM_FORMAT_ARGB1555,
    DRM_FORMAT_ABGR1555,
    DRM_FORMAT_RGBA5551,
    DRM_FORMAT_BGRA5551,
};

struct KWIN_EXPORT FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;
    bool floatingPoint;
    vk::Format vulkanFormat;

    std::optional<YuvConversion> yuvConversion() const
    {
        const auto it = s_drmConversions.find(drmFormat);
        return it != s_drmConversions.end() ? *it : std::optional<YuvConversion>{};
    }

    static std::optional<FormatInfo> get(uint32_t drmFormat);
    static QString drmFormatName(uint32_t format);
};
}
