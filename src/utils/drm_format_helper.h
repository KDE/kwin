/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QHash>
#include <QList>
#include <QString>

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
};

struct KWIN_EXPORT FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;
    bool floatingPoint;

    std::optional<YuvConversion> yuvConversion() const
    {
        const auto it = s_drmConversions.find(drmFormat);
        return it != s_drmConversions.end() ? *it : std::optional<YuvConversion>{};
    }

    static std::optional<FormatInfo> get(uint32_t drmFormat);
    static QString drmFormatName(uint32_t format);
};
}
