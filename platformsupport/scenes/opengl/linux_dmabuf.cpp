/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2019 Roman Gilg <subdiff@gmail.com>
Copyright © 2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "linux_dmabuf.h"

#include "drm_fourcc.h"
#include "../../../wayland_server.h"

#include <KWayland/Server/display.h>

#include <unistd.h>
#include <EGL/eglmesaext.h>

namespace KWin
{

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func) (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_LINUX_DMA_BUF_EXT                     0x3270
#define EGL_LINUX_DRM_FOURCC_EXT                  0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT                 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT             0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT              0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT                 0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT             0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT              0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT                 0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT             0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT              0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT              0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT                 0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT 0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT   0x327E
#define EGL_ITU_REC601_EXT                        0x327F
#define EGL_ITU_REC709_EXT                        0x3280
#define EGL_ITU_REC2020_EXT                       0x3281
#define EGL_YUV_FULL_RANGE_EXT                    0x3282
#define EGL_YUV_NARROW_RANGE_EXT                  0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT               0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT             0x3285
#endif // EGL_EXT_image_dma_buf_import

#ifndef EGL_EXT_image_dma_buf_import_modifiers
#define EGL_DMA_BUF_PLANE3_FD_EXT                 0x3440
#define EGL_DMA_BUF_PLANE3_OFFSET_EXT             0x3441
#define EGL_DMA_BUF_PLANE3_PITCH_EXT              0x3442
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT        0x3443
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT        0x3444
#define EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT        0x3445
#define EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT        0x3446
#define EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT        0x3447
#define EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT        0x3448
#define EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT        0x3449
#define EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT        0x344A
#endif // EGL_EXT_image_dma_buf_import_modifiers

struct YuvPlane
{
    int widthDivisor;
    int heightDivisor;
    uint32_t format;
    int planeIndex;
};

struct YuvFormat
{
    uint32_t format;
    int inputPlanes;
    int outputPlanes;
    int textureType;
    struct YuvPlane planes[3];
};

YuvFormat yuvFormats[] = {
    {
        DRM_FORMAT_YUYV,
        1, 2,
        EGL_TEXTURE_Y_XUXV_WL,
        {
            {
                1, 1,
                DRM_FORMAT_GR88,
                0
            },
            {
                2, 1,
                DRM_FORMAT_ARGB8888,
                0
            }
        }
    },
    {
        DRM_FORMAT_NV12,
        2, 2,
        EGL_TEXTURE_Y_UV_WL,
        {
            {
                1, 1,
                DRM_FORMAT_R8,
                0
            },
            {
                2, 2,
                DRM_FORMAT_GR88,
                1
            }
        }
    },
    {
        DRM_FORMAT_YUV420,
        3, 3,
        EGL_TEXTURE_Y_U_V_WL,
        {
            {
                1, 1,
                DRM_FORMAT_R8,
                0
            },
            {
                2, 2,
                DRM_FORMAT_R8,
                1
            },
            {
                2, 2,
                DRM_FORMAT_R8,
                2
            }
        }
    },
    {
        DRM_FORMAT_YUV444,
        3, 3,
        EGL_TEXTURE_Y_U_V_WL,
        {
            {
                1, 1,
                DRM_FORMAT_R8,
                0
            },
            {
                1, 1,
                DRM_FORMAT_R8,
                1
            },
            {
                1, 1,
                DRM_FORMAT_R8,
                2
            }
        }
    }
};

DmabufBuffer::DmabufBuffer(EGLImage image,
                           const QVector<Plane> &planes,
                           uint32_t format,
                           const QSize &size,
                           Flags flags,
                           LinuxDmabuf *interfaceImpl)
    : DmabufBuffer(planes, format, size, flags, interfaceImpl)
{
    m_importType = ImportType::Direct;
    addImage(image);
}


DmabufBuffer::DmabufBuffer(const QVector<Plane> &planes,
                           uint32_t format,
                           const QSize &size,
                           Flags flags,
                           LinuxDmabuf *interfaceImpl)
    : KWayland::Server::LinuxDmabufUnstableV1Buffer(format, size)
    , m_planes(planes)
    , m_flags(flags)
    , m_interfaceImpl(interfaceImpl)
{
    m_importType = ImportType::Conversion;
}

DmabufBuffer::~DmabufBuffer()
{
    if (m_interfaceImpl) {
        m_interfaceImpl->removeBuffer(this);
        removeImages();
    }

    // Close all open file descriptors
    for (int i = 0; i < m_planes.count(); i++) {
        if (m_planes[i].fd != -1)
            ::close(m_planes[i].fd);
        m_planes[i].fd = -1;
    }
}

void DmabufBuffer::addImage(EGLImage image)
{
    m_images << image;
}

void DmabufBuffer::removeImages()
{
    for (auto image : m_images) {
        eglDestroyImageKHR(m_interfaceImpl->m_backend->eglDisplay(), image);
    }
    m_images.clear();
    m_interfaceImpl = nullptr;
}

using Plane = KWayland::Server::LinuxDmabufUnstableV1Interface::Plane;
using Flags = KWayland::Server::LinuxDmabufUnstableV1Interface::Flags;

EGLImage LinuxDmabuf::createImage(const QVector<Plane> &planes,
                                  uint32_t format,
                                  const QSize &size)
{
    const bool hasModifiers = eglQueryDmaBufModifiersEXT != nullptr &&
            planes[0].modifier != DRM_FORMAT_MOD_INVALID;

    QVector<EGLint> attribs;
    attribs << EGL_WIDTH                            <<  size.width()
            << EGL_HEIGHT                           <<  size.height()
            << EGL_LINUX_DRM_FOURCC_EXT             <<  EGLint(format)

            << EGL_DMA_BUF_PLANE0_FD_EXT            <<  planes[0].fd
            << EGL_DMA_BUF_PLANE0_OFFSET_EXT        <<  EGLint(planes[0].offset)
            << EGL_DMA_BUF_PLANE0_PITCH_EXT         <<  EGLint(planes[0].stride);

    if (hasModifiers) {
        attribs
            << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT   <<  EGLint(planes[0].modifier & 0xffffffff)
            << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT   <<  EGLint(planes[0].modifier >> 32);
    }

    if (planes.count() > 1) {
        attribs
            << EGL_DMA_BUF_PLANE1_FD_EXT            <<  planes[1].fd
            << EGL_DMA_BUF_PLANE1_OFFSET_EXT        <<  EGLint(planes[1].offset)
            << EGL_DMA_BUF_PLANE1_PITCH_EXT         <<  EGLint(planes[1].stride);

        if (hasModifiers) {
            attribs
            << EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT   <<  EGLint(planes[1].modifier & 0xffffffff)
            << EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT   <<  EGLint(planes[1].modifier >> 32);
        }
    }

    if (planes.count() > 2) {
        attribs
            << EGL_DMA_BUF_PLANE2_FD_EXT            <<  planes[2].fd
            << EGL_DMA_BUF_PLANE2_OFFSET_EXT        <<  EGLint(planes[2].offset)
            << EGL_DMA_BUF_PLANE2_PITCH_EXT         <<  EGLint(planes[2].stride);

        if (hasModifiers) {
            attribs
            << EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT   <<  EGLint(planes[2].modifier & 0xffffffff)
            << EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT   <<  EGLint(planes[2].modifier >> 32);
        }
    }

    if (eglQueryDmaBufModifiersEXT != nullptr && planes.count() > 3) {
        attribs
            << EGL_DMA_BUF_PLANE3_FD_EXT            <<  planes[3].fd
            << EGL_DMA_BUF_PLANE3_OFFSET_EXT        <<  EGLint(planes[3].offset)
            << EGL_DMA_BUF_PLANE3_PITCH_EXT         <<  EGLint(planes[3].stride);

        if (hasModifiers) {
            attribs
            << EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT   <<  EGLint(planes[3].modifier & 0xffffffff)
            << EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT   <<  EGLint(planes[3].modifier >> 32);
        }
    }

    attribs << EGL_NONE;

    EGLImage image = eglCreateImageKHR(m_backend->eglDisplay(),
                                       EGL_NO_CONTEXT,
                                       EGL_LINUX_DMA_BUF_EXT,
                                       (EGLClientBuffer) nullptr,
                                       attribs.data());
    if (image == EGL_NO_IMAGE_KHR) {
        return nullptr;
    }

    return image;
}

KWayland::Server::LinuxDmabufUnstableV1Buffer* LinuxDmabuf::importBuffer(const QVector<Plane> &planes,
                                                                         uint32_t format,
                                                                         const QSize &size,
                                                                         Flags flags)
{
    Q_ASSERT(planes.count() > 0);

    // Try first to import as a single image
    if (auto *img = createImage(planes, format, size)) {
        return new DmabufBuffer(img, planes, format, size, flags, this);
    }

    // TODO: to enable this we must be able to store multiple textures per window pixmap
    //       and when on window draw do yuv to rgb transformation per shader (see Weston)
//    // not a single image, try yuv import
//    return yuvImport(planes, format, size, flags);

    return nullptr;
}

KWayland::Server::LinuxDmabufUnstableV1Buffer* LinuxDmabuf::yuvImport(const QVector<Plane> &planes,
                                                                      uint32_t format,
                                                                      const QSize &size,
                                                                      Flags flags)
{
    YuvFormat yuvFormat;
    for (YuvFormat f : yuvFormats) {
        if (f.format == format) {
            yuvFormat = f;
            break;
        }
    }
    if (yuvFormat.format == 0) {
        return nullptr;
    }
    if (planes.count() != yuvFormat.inputPlanes) {
        return nullptr;
    }

    auto *buf = new DmabufBuffer(planes, format, size, flags, this);

    for (int i = 0; i < yuvFormat.outputPlanes; i++) {
        int planeIndex = yuvFormat.planes[i].planeIndex;
        Plane plane = {
            planes[planeIndex].fd,
            planes[planeIndex].offset,
            planes[planeIndex].stride,
            planes[planeIndex].modifier
        };
        const auto planeFormat = yuvFormat.planes[i].format;
        const auto planeSize = QSize(size.width() / yuvFormat.planes[i].widthDivisor,
                                     size.height() / yuvFormat.planes[i].heightDivisor);
        auto *image = createImage(QVector<Plane>(1, plane),
                                  planeFormat,
                                  planeSize);
        if (!image) {
            delete buf;
            return nullptr;
        }
        buf->addImage(image);
    }
    // TODO: add buf import properties
    return buf;
}

LinuxDmabuf* LinuxDmabuf::factory(AbstractEglBackend *backend)
{
    if (!backend->hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import"))) {
        return nullptr;
    }

    if (backend->hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    if (eglQueryDmaBufFormatsEXT == nullptr) {
        return nullptr;
    }

    return new LinuxDmabuf(backend);
}

LinuxDmabuf::LinuxDmabuf(AbstractEglBackend *backend)
    : KWayland::Server::LinuxDmabufUnstableV1Interface::Impl()
    , m_backend(backend)
{
    Q_ASSERT(waylandServer());
    m_interface = waylandServer()->display()->createLinuxDmabufInterface(backend);
    setSupportedFormatsAndModifiers();
    m_interface->setImpl(this);
    m_interface->create();
}

LinuxDmabuf::~LinuxDmabuf()
{
    for (auto *dmabuf : qAsConst(m_buffers)) {
        dmabuf->removeImages();
    }
}

void LinuxDmabuf::removeBuffer(DmabufBuffer *buffer)
{
    m_buffers.remove(buffer);
}

const uint32_t s_multiPlaneFormats[] = {
    DRM_FORMAT_XRGB8888_A8,
    DRM_FORMAT_XBGR8888_A8,
    DRM_FORMAT_RGBX8888_A8,
    DRM_FORMAT_BGRX8888_A8,
    DRM_FORMAT_RGB888_A8,
    DRM_FORMAT_BGR888_A8,
    DRM_FORMAT_RGB565_A8,
    DRM_FORMAT_BGR565_A8,

    DRM_FORMAT_NV12,
    DRM_FORMAT_NV21,
    DRM_FORMAT_NV16,
    DRM_FORMAT_NV61,
    DRM_FORMAT_NV24,
    DRM_FORMAT_NV42,

    DRM_FORMAT_YUV410,
    DRM_FORMAT_YVU410,
    DRM_FORMAT_YUV411,
    DRM_FORMAT_YVU411,
    DRM_FORMAT_YUV420,
    DRM_FORMAT_YVU420,
    DRM_FORMAT_YUV422,
    DRM_FORMAT_YVU422,
    DRM_FORMAT_YUV444,
    DRM_FORMAT_YVU444
};

void filterFormatsWithMultiplePlanes(QVector<uint32_t> &formats)
{
    QVector<uint32_t>::iterator it = formats.begin();
    while (it != formats.end()) {
        for (auto linuxFormat : s_multiPlaneFormats) {
            if (*it == linuxFormat) {
                qDebug() << "Filter multi-plane format" << *it;
                it = formats.erase(it);
                it--;
                break;
            }
        }
        it++;
    }
}

void LinuxDmabuf::setSupportedFormatsAndModifiers()
{
    const EGLDisplay eglDisplay = m_backend->eglDisplay();
    EGLint count = 0;
    EGLBoolean success = eglQueryDmaBufFormatsEXT(eglDisplay, 0, nullptr, &count);

    if (!success || count == 0) {
        return;
    }

    QVector<uint32_t> formats(count);
    if (!eglQueryDmaBufFormatsEXT(eglDisplay, count, (EGLint *) formats.data(), &count)) {
        return;
    }

    filterFormatsWithMultiplePlanes(formats);

    QHash<uint32_t, QSet<uint64_t> > set;

    for (auto format : qAsConst(formats)) {
        if (eglQueryDmaBufModifiersEXT != nullptr) {
            count = 0;
            success = eglQueryDmaBufModifiersEXT(eglDisplay, format, 0, nullptr, nullptr, &count);

            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                if (eglQueryDmaBufModifiersEXT(eglDisplay,
                                               format, count, modifiers.data(),
                                               nullptr, &count)) {
                    QSet<uint64_t> modifiersSet;
                    for (auto mod : qAsConst(modifiers)) {
                        modifiersSet.insert(mod);
                    }
                    set.insert(format, modifiersSet);
                    continue;
                }
            }
        }
        set.insert(format, QSet<uint64_t>());
    }

    m_interface->setSupportedFormatsWithModifiers(set);
}

}
