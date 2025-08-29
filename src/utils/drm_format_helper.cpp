/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_format_helper.h"
#include "vulkan/vulkan_include.h"

import vulkan_hpp;

namespace KWin
{

std::optional<FormatInfo> FormatInfo::get(uint32_t drmFormat)
{
    switch (drmFormat) {
    case DRM_FORMAT_XRGB8888:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
            .vulkanFormat = vk::Format::eB8G8R8A8Unorm,
        };
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_ARGB8888:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 8,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
            .vulkanFormat = vk::Format::eB8G8R8A8Unorm,
        };
    case DRM_FORMAT_ABGR8888:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 8,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
            .vulkanFormat = vk::Format::eR8G8B8A8Unorm,
        };
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 8,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGBA8,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_XRGB2101010:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .vulkanFormat = vk::Format::eA2R10G10B10UnormPack32,
        };
    case DRM_FORMAT_XBGR2101010:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .vulkanFormat = vk::Format::eA2B10G10R10UnormPack32,
        };
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_ARGB2101010:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 2,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .vulkanFormat = vk::Format::eA2R10G10B10UnormPack32,
        };
    case DRM_FORMAT_ABGR2101010:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 2,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .vulkanFormat = vk::Format::eA2B10G10R10UnormPack32,
        };
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 2,
            .bitsPerPixel = 32,
            .openglFormat = GL_RGB10_A2,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_XRGB16161616:
    case DRM_FORMAT_XBGR16161616:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 0,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16,
            .floatingPoint = false,
        };
    case DRM_FORMAT_ARGB16161616:
    case DRM_FORMAT_ABGR16161616:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 16,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16,
            .floatingPoint = false,
        };
    case DRM_FORMAT_XRGB16161616F:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 0,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_ARGB16161616F:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 16,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_XBGR16161616F:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 0,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
            .floatingPoint = true,
            .vulkanFormat = vk::Format::eR16G16B16A16Sfloat,
        };
    case DRM_FORMAT_ABGR16161616F:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 16,
            .bitsPerPixel = 64,
            .openglFormat = GL_RGBA16F,
            .floatingPoint = true,
            .vulkanFormat = vk::Format::eR16G16B16A16Sfloat,
        };
    case DRM_FORMAT_ARGB4444:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 4,
            .alphaBits = 4,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGBA4,
            .vulkanFormat = vk::Format::eA4R4G4B4UnormPack16,
        };
    case DRM_FORMAT_ABGR4444:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 4,
            .alphaBits = 4,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGBA4,
            .vulkanFormat = vk::Format::eA4B4G4R4UnormPack16,
        };
    case DRM_FORMAT_RGBA4444:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 4,
            .alphaBits = 4,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGBA4,
            .vulkanFormat = vk::Format::eR4G4B4A4UnormPack16,
        };
    case DRM_FORMAT_BGRA4444:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 4,
            .alphaBits = 4,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGBA4,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eB4G4R4A4UnormPack16,
        };
    case DRM_FORMAT_ARGB1555:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 5,
            .alphaBits = 1,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGB5_A1,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eA1R5G5B5UnormPack16,
        };
    case DRM_FORMAT_ABGR1555:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 5,
            .alphaBits = 1,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGB5_A1,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_RGBA5551:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 5,
            .alphaBits = 1,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGB5_A1,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eR5G5B5A1UnormPack16,
        };
    case DRM_FORMAT_BGRA5551:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 5,
            .alphaBits = 1,
            .bitsPerPixel = 16,
            .openglFormat = GL_RGB5_A1,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eB5G5R5A1UnormPack16,
        };
    case DRM_FORMAT_NV12:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 24,
            .openglFormat = GL_R8,
            .floatingPoint = false,
            .vulkanFormat = vk::Format::eUndefined,
        };
    case DRM_FORMAT_P010:
        return FormatInfo{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 48,
            .openglFormat = GL_R16,
            .floatingPoint = false,
        };
    default:
        return std::nullopt;
    }
}

QString FormatInfo::drmFormatName(uint32_t format)
{
    return QString::asprintf(
        "%c%c%c%c %s-endian (0x%08x)",
        QLatin1Char(format & 0xff).toLatin1(),
        QLatin1Char((format >> 8) & 0xff).toLatin1(),
        QLatin1Char((format >> 16) & 0xff).toLatin1(),
        QLatin1Char((format >> 24) & 0x7f).toLatin1(),
        format & DRM_FORMAT_BIG_ENDIAN ? "big" : "little",
        format);
}

}
