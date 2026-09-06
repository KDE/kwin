/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_formats.h"

namespace KWin
{

ModifierList::ModifierList()
{
}

ModifierList::ModifierList(QList<uint64_t> &&move)
    : QList<uint64_t>(std::move(move))
{
}

ModifierList::ModifierList(const QList<uint64_t> &copy)
    : QList<uint64_t>(copy)
{
}

ModifierList::ModifierList(const std::initializer_list<uint64_t> &list)
    : QList<uint64_t>(list)
{
}

void ModifierList::insert(uint64_t modifier)
{
    if (!contains(modifier)) {
        push_back(modifier);
    }
}

void ModifierList::erase(uint64_t modifier)
{
    removeOne(modifier);
}

void ModifierList::insert(const ModifierList &other)
{
    for (const uint64_t &mod : other) {
        insert(mod);
    }
}

void ModifierList::intersect(const ModifierList &other)
{
    erase_if(*this, [&other](const uint64_t &mod) {
        return !other.contains(mod);
    });
}

ModifierList ModifierList::intersected(const ModifierList &other) const
{
    ModifierList ret;
    std::ranges::copy_if(*this, std::back_inserter(ret), [&other](uint64_t modifier) {
        return other.contains(modifier);
    });
    return ret;
}

FormatModifierMap::FormatModifierMap()
{
}

FormatModifierMap::FormatModifierMap(QHash<uint32_t, ModifierList> &&move)
    : QHash<uint32_t, ModifierList>(std::move(move))
{
}

FormatModifierMap::FormatModifierMap(const QHash<uint32_t, ModifierList> &copy)
    : QHash<uint32_t, ModifierList>(copy)
{
}

FormatModifierMap::FormatModifierMap(const std::initializer_list<std::pair<uint32_t, ModifierList>> &list)
    : QHash<uint32_t, ModifierList>(list)
{
}

bool FormatModifierMap::containsFormat(uint32_t format, uint64_t modifier) const
{
    const auto it = find(format);
    return it != end() && it->contains(modifier);
}

void FormatModifierMap::merge(const FormatModifierMap &other)
{
    for (auto it = other.begin(); it != other.end(); it++) {
        (*this)[it.key()].insert(it.value());
    }
}

FormatModifierMap FormatModifierMap::merged(const FormatModifierMap &other) const
{
    auto ret = *this;
    ret.merge(other);
    return ret;
}

FormatModifierMap FormatModifierMap::intersected(const FormatModifierMap &other) const
{
    FormatModifierMap ret;
    for (auto it = begin(); it != end(); it++) {
        const uint32_t format = it.key();
        const auto modifiers = it.value().intersected(other[format]);
        if (!modifiers.isEmpty()) {
            ret[format] = modifiers;
        }
    }
    return ret;
}

// NOTE the mapping of drm formats to Vulkan formats isn't straight-forward.
// - for non-packed 8 and 16 bits per channel formats, the channel order is inverted vs. drm
// - for packed formats, the channel order matches drm
const std::unordered_map<uint32_t, FormatInfo> FormatInfo::s_knownFormats = {
    // 24 bpp
    {DRM_FORMAT_BGR888, FormatInfo{
                            .drmFormat = DRM_FORMAT_BGR888,
                            .bitsPerColor = 8,
                            .alphaBits = 0,
                            .bitsPerPixel = 24,
                            .openglFormat = GL_RGB8,
                            .vulkanFormat = VK_FORMAT_R8G8B8_UNORM,
                            .floatingPoint = false,
                            .yuv = false,
                            .qimageFormat = QImage::Format_RGB888,
                        }},
    {DRM_FORMAT_RGB888, FormatInfo{
                            .drmFormat = DRM_FORMAT_RGB888,
                            .bitsPerColor = 8,
                            .alphaBits = 0,
                            .bitsPerPixel = 24,
                            .openglFormat = GL_RGB8,
                            .vulkanFormat = VK_FORMAT_B8G8R8_UNORM,
                            .floatingPoint = false,
                            .yuv = false,
                            .qimageFormat = QImage::Format_BGR888,
                        }},

    // 32 bpp
    {DRM_FORMAT_XRGB8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XRGB8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_B8G8R8A8_UNORM,
                              .floatingPoint = false,
                              .yuv = false,
                              .qimageFormat = QImage::Format_RGB32,
                          }},
    {DRM_FORMAT_XBGR8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XBGR8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBX8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBX8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                              .qimageFormat = QImage::Format_RGBX8888,
                          }},
    {DRM_FORMAT_BGRX8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRX8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_ARGB8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_B8G8R8A8_UNORM,
                              .floatingPoint = false,
                              .yuv = false,
                              .qimageFormat = QImage::Format_ARGB32_Premultiplied,
                          }},
    {DRM_FORMAT_ABGR8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_R8G8B8A8_UNORM,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBA8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_BGRA8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_XRGB2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_XRGB2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                 .floatingPoint = false,
                                 .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                 .qimageFormat = QImage::Format_RGB30,
#endif
                             }},
    {DRM_FORMAT_XBGR2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_XBGR2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                 .floatingPoint = false,
                                 .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                 .qimageFormat = QImage::Format_BGR30,
#endif
                             }},
    {DRM_FORMAT_RGBX1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_RGBX1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                                 .yuv = false,
                             }},
    {DRM_FORMAT_BGRX1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_BGRX1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                                 .yuv = false,
                             }},
    {DRM_FORMAT_ARGB2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_ARGB2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
                                 .floatingPoint = false,
                                 .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                 .qimageFormat = QImage::Format_A2RGB30_Premultiplied,
#endif
                             }},
    {DRM_FORMAT_ABGR2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_ABGR2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                 .floatingPoint = false,
                                 .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                 .qimageFormat = QImage::Format_A2BGR30_Premultiplied,
#endif
                             }},
    {DRM_FORMAT_RGBA1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_RGBA1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                                 .yuv = false,
                             }},
    {DRM_FORMAT_BGRA1010102, FormatInfo{
                                 .drmFormat = DRM_FORMAT_BGRA1010102,
                                 .bitsPerColor = 10,
                                 .alphaBits = 2,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGB10_A2,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                                 .yuv = false,
                             }},

    // 64 bpp
    {DRM_FORMAT_XRGB16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_XRGB16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 0,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_UNDEFINED,
                                  .floatingPoint = false,
                                  .yuv = false,
                              }},
    {DRM_FORMAT_XBGR16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_XBGR16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 0,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_R16G16B16A16_UNORM,
                                  .floatingPoint = false,
                                  .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                  .qimageFormat = QImage::Format_RGBX64,
#endif
                              }},
    {DRM_FORMAT_ARGB16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_ARGB16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 16,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_UNDEFINED,
                                  .floatingPoint = false,
                                  .yuv = false,
                              }},
    {DRM_FORMAT_ABGR16161616, FormatInfo{
                                  .drmFormat = DRM_FORMAT_ABGR16161616,
                                  .bitsPerColor = 16,
                                  .alphaBits = 16,
                                  .bitsPerPixel = 64,
                                  .openglFormat = GL_RGBA16,
                                  .vulkanFormat = VK_FORMAT_R16G16B16A16_UNORM,
                                  .floatingPoint = false,
                                  .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                  .qimageFormat = QImage::Format_RGBA64_Premultiplied,
#endif
                              }},
    {DRM_FORMAT_XRGB16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_XRGB16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 0,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_UNDEFINED,
                                   .floatingPoint = true,
                                   .yuv = false,
                               }},
    {DRM_FORMAT_XBGR16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_XBGR16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 0,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
                                   .floatingPoint = true,
                                   .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                   .qimageFormat = QImage::QImage::Format_RGBX16FPx4,
#endif

                               }},
    {DRM_FORMAT_ARGB16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_ARGB16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 16,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_UNDEFINED,
                                   .floatingPoint = true,
                                   .yuv = false,
                               }},
    {DRM_FORMAT_ABGR16161616F, FormatInfo{
                                   .drmFormat = DRM_FORMAT_ABGR16161616F,
                                   .bitsPerColor = 16,
                                   .alphaBits = 16,
                                   .bitsPerPixel = 64,
                                   .openglFormat = GL_RGBA16F,
                                   .vulkanFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
                                   .floatingPoint = true,
                                   .yuv = false,
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                                   .qimageFormat = QImage::Format_RGBA16FPx4_Premultiplied,
#endif
                               }},

    // 16 bpp
    {DRM_FORMAT_XRGB4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_XRGB4444,
                              .bitsPerColor = 4,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_XBGR4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_XBGR4444,
                              .bitsPerColor = 4,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBX4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBX4444,
                              .bitsPerColor = 4,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_BGRX4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRX4444,
                              .bitsPerColor = 4,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_ARGB4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_ABGR4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBA4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_BGRA4444, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA4444,
                              .bitsPerColor = 4,
                              .alphaBits = 4,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGBA4,
                              .vulkanFormat = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_XRGB1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_XRGB1555,
                              .bitsPerColor = 5,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_XBGR1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_XBGR1555,
                              .bitsPerColor = 5,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBX5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBX5551,
                              .bitsPerColor = 5,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_BGRX5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRX5551,
                              .bitsPerColor = 5,
                              .alphaBits = 0,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_ARGB1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_ARGB1555,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_ABGR1555, FormatInfo{
                              .drmFormat = DRM_FORMAT_ABGR1555,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGBA5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_RGBA5551,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_BGRA5551, FormatInfo{
                              .drmFormat = DRM_FORMAT_BGRA5551,
                              .bitsPerColor = 5,
                              .alphaBits = 1,
                              .bitsPerPixel = 16,
                              .openglFormat = GL_RGB5_A1,
                              .vulkanFormat = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
                              .floatingPoint = false,
                              .yuv = false,
                          }},
    {DRM_FORMAT_RGB565, FormatInfo{
                            .drmFormat = DRM_FORMAT_RGB565,
                            .bitsPerColor = 6,
                            .alphaBits = 0,
                            .bitsPerPixel = 16,
                            .openglFormat = GL_RGB565,
                            .vulkanFormat = VK_FORMAT_R5G6B5_UNORM_PACK16,
                            .floatingPoint = false,
                            .yuv = false,
                        }},
    {DRM_FORMAT_BGR565, FormatInfo{
                            .drmFormat = DRM_FORMAT_BGR565,
                            .bitsPerColor = 6,
                            .alphaBits = 0,
                            .bitsPerPixel = 16,
                            .openglFormat = GL_RGB565,
                            .vulkanFormat = VK_FORMAT_B5G6R5_UNORM_PACK16,
                            .floatingPoint = false,
                            .yuv = false,
                        }},

    // 48bpp
    {DRM_FORMAT_RGB161616, FormatInfo{
                               .drmFormat = DRM_FORMAT_RGB161616,
                               .bitsPerColor = 16,
                               .alphaBits = 0,
                               .bitsPerPixel = 48,
                               .openglFormat = GL_RGB16,
                               .vulkanFormat = VK_FORMAT_UNDEFINED,
                               .floatingPoint = false,
                               .yuv = false,
                           }},
    {DRM_FORMAT_BGR161616, FormatInfo{
                               .drmFormat = DRM_FORMAT_BGR161616,
                               .bitsPerColor = 16,
                               .alphaBits = 0,
                               .bitsPerPixel = 48,
                               .openglFormat = GL_RGB16,
                               .vulkanFormat = VK_FORMAT_R16G16B16_UNORM,
                               .floatingPoint = false,
                               .yuv = false,
                           }},

    // YUV formats
    {DRM_FORMAT_NV12, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV12,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV21, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV21,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV16, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV16,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV61, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV61,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV24, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV24,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV42, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV42,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 24,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV15, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV15,
                          .bitsPerColor = 40,
                          .alphaBits = 0,
                          .bitsPerPixel = 80,
                          .openglFormat = GL_RGBA16,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV20, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV20,
                          .bitsPerColor = 40,
                          .alphaBits = 0,
                          .bitsPerPixel = 80,
                          .openglFormat = GL_RGBA16,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_NV30, FormatInfo{
                          .drmFormat = DRM_FORMAT_NV30,
                          .bitsPerColor = 40,
                          .alphaBits = 0,
                          .bitsPerPixel = 80,
                          .openglFormat = GL_RGBA16,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_P010, FormatInfo{
                          .drmFormat = DRM_FORMAT_P010,
                          .bitsPerColor = 10,
                          .alphaBits = 0,
                          .bitsPerPixel = 48,
                          .openglFormat = GL_RGBA16,
                          .vulkanFormat = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_YUYV, FormatInfo{
                          .drmFormat = DRM_FORMAT_YUYV,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 32,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_G8B8G8R8_422_UNORM,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_YVYU, FormatInfo{
                          .drmFormat = DRM_FORMAT_YVYU,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 32,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_UYVY, FormatInfo{
                          .drmFormat = DRM_FORMAT_UYVY,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 32,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_B8G8R8G8_422_UNORM,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_VYUY, FormatInfo{
                          .drmFormat = DRM_FORMAT_VYUY,
                          .bitsPerColor = 8,
                          .alphaBits = 0,
                          .bitsPerPixel = 32,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_AYUV, FormatInfo{
                          .drmFormat = DRM_FORMAT_AYUV,
                          .bitsPerColor = 8,
                          .alphaBits = 8,
                          .bitsPerPixel = 32,
                          .openglFormat = GL_RGBA8,
                          .vulkanFormat = VK_FORMAT_UNDEFINED,
                          .floatingPoint = false,
                          .yuv = true,
                      }},
    {DRM_FORMAT_AVUY8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_AVUY8888,
                              .bitsPerColor = 8,
                              .alphaBits = 8,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = true,
                          }},
    {DRM_FORMAT_XYUV8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XYUV8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = true,
                              .qimageFormat = QImage::Format_RGB32,
                          }},
    {DRM_FORMAT_XVUY8888, FormatInfo{
                              .drmFormat = DRM_FORMAT_XVUY8888,
                              .bitsPerColor = 8,
                              .alphaBits = 0,
                              .bitsPerPixel = 32,
                              .openglFormat = GL_RGBA8,
                              .vulkanFormat = VK_FORMAT_UNDEFINED,
                              .floatingPoint = false,
                              .yuv = true,
                          }},
    {DRM_FORMAT_VUY888, FormatInfo{
                            .drmFormat = DRM_FORMAT_VUY888,
                            .bitsPerColor = 8,
                            .alphaBits = 0,
                            .bitsPerPixel = 24,
                            .openglFormat = GL_RGBA8,
                            .vulkanFormat = VK_FORMAT_UNDEFINED,
                            .floatingPoint = false,
                            .yuv = true,
                        }},
    {DRM_FORMAT_VUY101010, FormatInfo{
                               .drmFormat = DRM_FORMAT_VUY101010,
                               .bitsPerColor = 10,
                               .alphaBits = 0,
                               .bitsPerPixel = 30,
                               .openglFormat = GL_RGBA16,
                               .vulkanFormat = VK_FORMAT_UNDEFINED,
                               .floatingPoint = false,
                               .yuv = true,
                           }},
#ifdef DRM_FORMAT_XVUY2101010
    {DRM_FORMAT_XVUY2101010, FormatInfo{
                                 .drmFormat = DRM_FORMAT_XVUY2101010,
                                 .bitsPerColor = 10,
                                 .alphaBits = 0,
                                 .bitsPerPixel = 32,
                                 .openglFormat = GL_RGBA16,
                                 .vulkanFormat = VK_FORMAT_UNDEFINED,
                                 .floatingPoint = false,
                                 .yuv = true,
                             }},
#endif
};

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
