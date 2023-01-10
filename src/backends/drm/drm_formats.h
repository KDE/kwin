#pragma once

#include <drm_fourcc.h>
#include <optional>
#include <stdint.h>

namespace KWin
{

struct DrmFormat
{
    uint32_t drmFormat;
    uint32_t bitsPerColor;
    uint32_t alphaBits;
    uint32_t bitsPerPixel;
    uint32_t openGlFormat;
};

inline static std::optional<DrmFormat> formatProperties(uint32_t drmFormat)
{
    // we only care about formats that are rgb and at least 8 bits per color
    switch (drmFormat) {
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 24,
            .openGlFormat = GL_RGBA8,
        };
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openGlFormat = GL_RGBA8,
        };
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 8,
            .alphaBits = 8,
            .bitsPerPixel = 32,
            .openGlFormat = GL_RGBA8,
        };
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 0,
            .bitsPerPixel = 32,
            .openGlFormat = GL_RGB10_A2,
        };
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 2,
            .bitsPerPixel = 32,
            .openGlFormat = GL_RGB10_A2,
        };
    case DRM_FORMAT_XRGB16161616:
    case DRM_FORMAT_XBGR16161616:
    case DRM_FORMAT_XRGB16161616F:
    case DRM_FORMAT_XBGR16161616F:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 0,
            .bitsPerPixel = 64,
            .openGlFormat = GL_RGBA16,
        };
    case DRM_FORMAT_ARGB16161616:
    case DRM_FORMAT_ABGR16161616:
    case DRM_FORMAT_ARGB16161616F:
    case DRM_FORMAT_ABGR16161616F:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 16,
            .alphaBits = 16,
            .bitsPerPixel = 64,
            .openGlFormat = GL_RGBA16,
        };
    case DRM_FORMAT_AXBXGXRX106106106106:
        return DrmFormat{
            .drmFormat = drmFormat,
            .bitsPerColor = 10,
            .alphaBits = 10,
            .bitsPerPixel = 64,
            .openGlFormat = GL_RGB10_A2,
        };
    default:
        return std::nullopt;
    }
}

}
