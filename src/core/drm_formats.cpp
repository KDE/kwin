/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_formats.h"

namespace KWin
{

ModifierList intersect(const ModifierList &one, const ModifierList &two)
{
    ModifierList ret;
    std::ranges::set_intersection(one, two, std::inserter(ret, ret.begin()));
    return ret;
}

const QHash<uint32_t, YuvConversion> FormatInfo::s_drmConversions = {
    {DRM_FORMAT_NV12, YuvConversion{
                          {YuvFormat{DRM_FORMAT_R8, 1, 1}, YuvFormat{DRM_FORMAT_GR88, 2, 2}},
                      }},
    {DRM_FORMAT_P010, YuvConversion{
                          {YuvFormat{DRM_FORMAT_R16, 1, 1}, YuvFormat{DRM_FORMAT_GR1616, 2, 2}},
                      }},
    {DRM_FORMAT_XYUV8888, YuvConversion{
                              {YuvFormat{DRM_FORMAT_XRGB8888, 1, 1}},
                          }},
};

const std::unordered_map<uint32_t, FormatInfo> FormatInfo::s_knownFormats = {
    {DRM_FORMAT_XRGB8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XRGB8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_B8G8R8A8_UNORM,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_XBGR8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XBGR8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_RGBX8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBX8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_BGRX8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRX8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_ARGB8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_B8G8R8A8_UNORM,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_ABGR8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_RGBA8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_BGRA8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_XRGB2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_XRGB2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_XBGR2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_XBGR2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_RGBX1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_RGBX1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_BGRX1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_BGRX1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_ARGB2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_ARGB2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_ABGR2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_ABGR2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_RGBA1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_RGBA1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_BGRA1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_BGRA1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                 .floatingPoint = false,
                             }},
    {DRM_FORMAT_XRGB16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_XRGB16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 0,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_UNDEFINED,
                                  .floatingPoint = false,
                              }},
    {DRM_FORMAT_XBGR16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_XBGR16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 0,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_R16G16B16A16_UNORM,
                                  .floatingPoint = false,
                              }},
    {DRM_FORMAT_ARGB16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_ARGB16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 16,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_UNDEFINED,
                                  .floatingPoint = false,
                              }},
    {DRM_FORMAT_ABGR16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_ABGR16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 16,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_R16G16B16A16_UNORM,
                                  .floatingPoint = false,
                              }},
    {DRM_FORMAT_XRGB16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_XRGB16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 0,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_UNDEFINED,
                                   .floatingPoint = true,
                               }},
    {DRM_FORMAT_XBGR16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_XBGR16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 0,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
                                   .floatingPoint = true,
                               }},
    {DRM_FORMAT_ARGB16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_ARGB16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 16,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_UNDEFINED,
                                   .floatingPoint = true,
                               }},
    {DRM_FORMAT_ABGR16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_ABGR16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 16,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
                                   .floatingPoint = true,
                               }},
    {DRM_FORMAT_ARGB4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_ABGR4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_RGBA4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_BGRA4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_ARGB1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB1555,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_ABGR1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR1555,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_RGBA5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA5551,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_A1B5G5R5_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    {DRM_FORMAT_BGRA5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA5551,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                              .floatingPoint = false,
                          }},
    // TODO support YUV formats with Vulkan
    {DRM_FORMAT_NV12, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV12,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_R8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                      }},
    {DRM_FORMAT_P010, FormatInfo{
                          .drmFormat = DRM_FORMAT_P010,
                          .bitsPerColor = 10,
                          .alphaBits = 0,
                          .bitsPerPixel = 48,
                          .openglFormat = GL_R16,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                      }},
    {DRM_FORMAT_XYUV8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XYUV8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                          }},
};

// TODO use swizzles to support all format variations with Vulkan
std::optional<FormatInfo> FormatInfo::get(uint32_t drmFormat)
{
    const auto it = s_knownFormats.find(drmFormat);
    return it == s_knownFormats.end() ? std::nullopt : std::optional(it->second);
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
