/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <QHash>
#include <QList>
#include <QString>

#include <epoxy/gl.h>
#include <libdrm/drm_fourcc.h>
#include <optional>
#include <stdint.h>

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

struct FormatInfo
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    GLint openglFormat;

    std::optional<YuvConversion> yuvConversion() const
    {
        const auto it = s_drmConversions.find(drmFormat);
        return it != s_drmConversions.end() ? *it : std::optional<YuvConversion>{};
    }
};

static std::optional<FormatInfo> formatInfo(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
        };
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 8,
            .alphaBits = 8,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
        };
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
        };
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 10,
            .alphaBits = 2,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
        };
    case DRM_FORMAT_XRGB16161616F:
    case DRM_FORMAT_XBGR16161616F:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 16,
            .alphaBits = 0,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
        };
    case DRM_FORMAT_ARGB16161616F:
    case DRM_FORMAT_ABGR16161616F:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 16,
            .alphaBits = 16,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
        };
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 4,
            .alphaBits = 4,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGBA4,
        };
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 5,
            .alphaBits = 1,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGB5_A1,
        };
    case DRM_FORMAT_NV12:
        return FormatInfo{
            .drmFormat = format,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 24,
            .openglFormat = GL_R8,
        };
    default:
        return std::nullopt;
    }
}

static QString drmFormatName(const QString &prefix, uint32_t format)
{
    return QString::asprintf(
        "%s%c%c%c%c %s-endian (0x%08x)", prefix.toUtf8().constData(),
        QLatin1Char(format & 0xff).toLatin1(),
        QLatin1Char((format >> 8) & 0xff).toLatin1(),
        QLatin1Char((format >> 16) & 0xff).toLatin1(),
        QLatin1Char((format >> 24) & 0x7f).toLatin1(),
        format & DRM_FORMAT_BIG_ENDIAN ? "big" : "little",
        format);
}
