/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_dmabuf.h"
#include "drm_fourcc.h"
#include "kwineglext.h"
#include "kwineglutils_p.h"

#include "utils/common.h"
#include "wayland_server.h"

#include <unistd.h>

namespace KWin
{

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func) (EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;

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

EglDmabufBuffer::EglDmabufBuffer(EGLImage image,
                                 const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                 uint32_t format,
                                 const QSize &size,
                                 quint32 flags,
                                 EglDmabuf *interfaceImpl)
    : EglDmabufBuffer(planes, format, size, flags, interfaceImpl)
{
    m_importType = ImportType::Direct;
    addImage(image);
}


EglDmabufBuffer::EglDmabufBuffer(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                 uint32_t format,
                                 const QSize &size,
                                 quint32 flags,
                                 EglDmabuf *interfaceImpl)
    : LinuxDmaBufV1ClientBuffer(planes, format, size, flags)
    , m_interfaceImpl(interfaceImpl)
{
    m_importType = ImportType::Conversion;
}

EglDmabufBuffer::~EglDmabufBuffer()
{
    removeImages();
}

void EglDmabufBuffer::setInterfaceImplementation(EglDmabuf *interfaceImpl)
{
    m_interfaceImpl = interfaceImpl;
}

void EglDmabufBuffer::addImage(EGLImage image)
{
    m_images << image;
}

void EglDmabufBuffer::removeImages()
{
    for (auto image : qAsConst(m_images)) {
        eglDestroyImageKHR(m_interfaceImpl->m_backend->eglDisplay(), image);
    }
    m_images.clear();
}

EGLImage EglDmabuf::createImage(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
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

    attribs << EGL_IMAGE_PRESERVED_KHR << EGL_TRUE;

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

KWaylandServer::LinuxDmaBufV1ClientBuffer *EglDmabuf::importBuffer(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                                                   quint32 format,
                                                                   const QSize &size,
                                                                   quint32 flags)
{
    Q_ASSERT(planes.count() > 0);

    // Try first to import as a single image
    if (auto *img = createImage(planes, format, size)) {
        return new EglDmabufBuffer(img, planes, format, size, flags, this);
    }

    // TODO: to enable this we must be able to store multiple textures per window pixmap
    //       and when on window draw do yuv to rgb transformation per shader (see Weston)
//    // not a single image, try yuv import
//    return yuvImport(planes, format, size, flags);

    return nullptr;
}

KWaylandServer::LinuxDmaBufV1ClientBuffer *EglDmabuf::yuvImport(const QVector<KWaylandServer::LinuxDmaBufV1Plane> &planes,
                                                                quint32 format,
                                                                const QSize &size,
                                                                quint32 flags)
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

    auto *buf = new EglDmabufBuffer(planes, format, size, flags, this);

    for (int i = 0; i < yuvFormat.outputPlanes; i++) {
        int planeIndex = yuvFormat.planes[i].planeIndex;
        KWaylandServer::LinuxDmaBufV1Plane plane = {
            planes[planeIndex].fd,
            planes[planeIndex].offset,
            planes[planeIndex].stride,
            planes[planeIndex].modifier
        };
        const auto planeFormat = yuvFormat.planes[i].format;
        const auto planeSize = QSize(size.width() / yuvFormat.planes[i].widthDivisor,
                                     size.height() / yuvFormat.planes[i].heightDivisor);
        auto *image = createImage(QVector<KWaylandServer::LinuxDmaBufV1Plane>(1, plane),
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

EglDmabuf* EglDmabuf::factory(AbstractEglBackend *backend)
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

    return new EglDmabuf(backend);
}

EglDmabuf::EglDmabuf(AbstractEglBackend *backend)
    : m_backend(backend)
{
    auto prevBuffersSet = waylandServer()->linuxDmabufBuffers();
    for (auto *buffer : prevBuffersSet) {
        auto *buf = static_cast<EglDmabufBuffer*>(buffer);
        buf->setInterfaceImplementation(this);
        buf->addImage(createImage(buf->planes(), buf->format(), buf->size()));
    }
    setSupportedFormatsAndModifiers();
}

EglDmabuf::~EglDmabuf()
{
    auto curBuffers = waylandServer()->linuxDmabufBuffers();
    for (auto *buffer : curBuffers) {
        auto *buf = static_cast<EglDmabufBuffer*>(buffer);
        buf->removeImages();
    }
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
                qCDebug(KWIN_OPENGL) << "Filter multi-plane format" << *it;
                it = formats.erase(it);
                it--;
                break;
            }
        }
        it++;
    }
}

static int bpcForFormat(uint32_t format)
{
    switch(format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
        return 8;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
        return 10;
    default:
        return -1;
    }
};

void EglDmabuf::setSupportedFormatsAndModifiers()
{
    const EGLDisplay eglDisplay = m_backend->eglDisplay();
    EGLint count = 0;
    EGLBoolean success = eglQueryDmaBufFormatsEXT(eglDisplay, 0, nullptr, &count);

    if (!success || count == 0) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT failed!" << getEglErrorString();
        return;
    }

    QVector<uint32_t> formats(count);
    if (!eglQueryDmaBufFormatsEXT(eglDisplay, count, (EGLint *) formats.data(), &count)) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT with count" << count << "failed!" << getEglErrorString();
        return;
    }

    filterFormatsWithMultiplePlanes(formats);

    QHash<uint32_t, QSet<uint64_t>> supportedFormats;
    for (auto format : qAsConst(formats)) {
        if (eglQueryDmaBufModifiersEXT != nullptr) {
            EGLint count = 0;
            const EGLBoolean success = eglQueryDmaBufModifiersEXT(eglDisplay, format, 0, nullptr, nullptr, &count);
            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                if (eglQueryDmaBufModifiersEXT(eglDisplay, format, count, modifiers.data(), nullptr, &count)) {
                    QSet<uint64_t> modifiersSet;
                    for (const uint64_t &mod : qAsConst(modifiers)) {
                        modifiersSet.insert(mod);
                    }
                    supportedFormats.insert(format, modifiersSet);
                    continue;
                }
            }
        }
        supportedFormats.insert(format, {DRM_FORMAT_MOD_INVALID});
    }
    qCDebug(KWIN_OPENGL) << "EGL driver advertises" << supportedFormats.count() << "supported dmabuf formats" << (eglQueryDmaBufModifiersEXT != nullptr ? "with" : "without") << "modifiers";

    auto filterFormats = [&supportedFormats](int bpc) {
        QHash<uint32_t, QSet<uint64_t>> set;
        for (auto it = supportedFormats.constBegin(); it != supportedFormats.constEnd(); it++) {
            if (bpcForFormat(it.key()) == bpc) {
                set.insert(it.key(), it.value());
            }
        }
        return set;
    };
    if (m_backend->prefer10bpc()) {
        m_tranches.append({
            .device = m_backend->deviceId(),
            .flags = {},
            .formatTable = filterFormats(10),
        });
    }
    m_tranches.append({
        .device = m_backend->deviceId(),
        .flags = {},
        .formatTable = filterFormats(8),
    });
    m_tranches.append({
        .device = m_backend->deviceId(),
        .flags = {},
        .formatTable = filterFormats(-1),
    });
    LinuxDmaBufV1RendererInterface::setSupportedFormatsAndModifiers(m_tranches);
}

}
