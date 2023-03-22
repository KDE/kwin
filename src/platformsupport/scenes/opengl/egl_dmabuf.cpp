/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2018 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_dmabuf.h"
#include "kwineglext.h"
#include "kwineglutils_p.h"

#include "utils/common.h"
#include "wayland_server.h"

#include <drm_fourcc.h>
#include <unistd.h>

namespace KWin
{

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func)(EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func)(EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);
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
    {DRM_FORMAT_YUYV,
     1,
     2,
     EGL_TEXTURE_Y_XUXV_WL,
     {{1, 1,
       DRM_FORMAT_GR88,
       0},
      {2, 1,
       DRM_FORMAT_ARGB8888,
       0}}},
    {DRM_FORMAT_NV12,
     2,
     2,
     EGL_TEXTURE_Y_UV_WL,
     {{1, 1,
       DRM_FORMAT_R8,
       0},
      {2, 2,
       DRM_FORMAT_GR88,
       1}}},
    {DRM_FORMAT_YUV420,
     3,
     3,
     EGL_TEXTURE_Y_U_V_WL,
     {{1, 1,
       DRM_FORMAT_R8,
       0},
      {2, 2,
       DRM_FORMAT_R8,
       1},
      {2, 2,
       DRM_FORMAT_R8,
       2}}},
    {DRM_FORMAT_YUV444,
     3,
     3,
     EGL_TEXTURE_Y_U_V_WL,
     {{1, 1,
       DRM_FORMAT_R8,
       0},
      {1, 1,
       DRM_FORMAT_R8,
       1},
      {1, 1,
       DRM_FORMAT_R8,
       2}}}};

EglDmabufBuffer::EglDmabufBuffer(EGLImage image,
                                 DmaBufAttributes &&attrs,
                                 quint32 flags,
                                 EglDmabuf *interfaceImpl)
    : EglDmabufBuffer(QVector<EGLImage>{image}, std::move(attrs), flags, interfaceImpl)
{
    m_importType = ImportType::Direct;
}

EglDmabufBuffer::EglDmabufBuffer(const QVector<EGLImage> &images,
                                 DmaBufAttributes &&attrs,
                                 quint32 flags,
                                 EglDmabuf *interfaceImpl)
    : LinuxDmaBufV1ClientBuffer(std::move(attrs), flags)
    , m_images(images)
    , m_interfaceImpl(interfaceImpl)
    , m_importType(ImportType::Conversion)
{
}

EglDmabufBuffer::~EglDmabufBuffer()
{
    removeImages();
}

void EglDmabufBuffer::setInterfaceImplementation(EglDmabuf *interfaceImpl)
{
    m_interfaceImpl = interfaceImpl;
}

void EglDmabufBuffer::setImages(const QVector<EGLImage> &images)
{
    m_images = images;
}

void EglDmabufBuffer::removeImages()
{
    for (auto image : std::as_const(m_images)) {
        eglDestroyImageKHR(m_interfaceImpl->m_backend->eglDisplay(), image);
    }
    m_images.clear();
}

KWaylandServer::LinuxDmaBufV1ClientBuffer *EglDmabuf::importBuffer(DmaBufAttributes &&attrs, quint32 flags)
{
    Q_ASSERT(attrs.planeCount > 0);

    // Try first to import as a single image
    if (auto *img = m_backend->importDmaBufAsImage(attrs)) {
        return new EglDmabufBuffer(img, std::move(attrs), flags, this);
    }

    // TODO: to enable this we must be able to store multiple textures per window pixmap
    //       and when on window draw do yuv to rgb transformation per shader (see Weston)
    //    // not a single image, try yuv import
    //    return yuvImport(attrs, flags);

    return nullptr;
}

KWaylandServer::LinuxDmaBufV1ClientBuffer *EglDmabuf::yuvImport(DmaBufAttributes &&attrs, quint32 flags)
{
    YuvFormat yuvFormat;
    for (YuvFormat f : yuvFormats) {
        if (f.format == attrs.format) {
            yuvFormat = f;
            break;
        }
    }
    if (yuvFormat.format == 0) {
        return nullptr;
    }
    if (attrs.planeCount != yuvFormat.inputPlanes) {
        return nullptr;
    }

    QVector<EGLImage> images;
    for (int i = 0; i < yuvFormat.outputPlanes; i++) {
        const int planeIndex = yuvFormat.planes[i].planeIndex;
        const DmaBufAttributes planeAttrs{
            .planeCount = 1,
            .width = attrs.width / yuvFormat.planes[i].widthDivisor,
            .height = attrs.height / yuvFormat.planes[i].heightDivisor,
            .format = yuvFormat.planes[i].format,
            .modifier = attrs.modifier,
            .fd = {attrs.fd[planeIndex].duplicate()},
            .offset = {attrs.offset[planeIndex], 0, 0, 0},
            .pitch = {attrs.pitch[planeIndex], 0, 0, 0},
        };
        auto *image = m_backend->importDmaBufAsImage(planeAttrs);
        if (!image) {
            return nullptr;
        }
        images.push_back(image);
    }

    return new EglDmabufBuffer(images, std::move(attrs), flags, this);
}

EglDmabuf *EglDmabuf::factory(AbstractEglBackend *backend)
{
    if (!backend->hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import"))) {
        return nullptr;
    }

    if (backend->hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
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
        auto *buf = static_cast<EglDmabufBuffer *>(buffer);
        buf->setInterfaceImplementation(this);
        buf->setImages({m_backend->importDmaBufAsImage(buf->attributes())});
    }
    setSupportedFormatsAndModifiers();
}

EglDmabuf::~EglDmabuf()
{
    auto curBuffers = waylandServer()->linuxDmabufBuffers();
    for (auto *buffer : curBuffers) {
        auto *buf = static_cast<EglDmabufBuffer *>(buffer);
        buf->removeImages();
    }
}

static int bpcForFormat(uint32_t format)
{
    switch (format) {
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
    if (!eglQueryDmaBufFormatsEXT(eglDisplay, count, (EGLint *)formats.data(), &count)) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT with count" << count << "failed!" << getEglErrorString();
        return;
    }

    m_supportedFormats.clear();
    for (auto format : std::as_const(formats)) {
        if (eglQueryDmaBufModifiersEXT != nullptr) {
            EGLint count = 0;
            const EGLBoolean success = eglQueryDmaBufModifiersEXT(eglDisplay, format, 0, nullptr, nullptr, &count);
            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                QVector<EGLBoolean> externalOnly(count);
                if (eglQueryDmaBufModifiersEXT(eglDisplay, format, count, modifiers.data(), externalOnly.data(), &count)) {
                    for (int i = modifiers.size() - 1; i >= 0; i--) {
                        if (externalOnly[i]) {
                            modifiers.remove(i);
                            externalOnly.remove(i);
                        }
                    }
                    if (!modifiers.empty()) {
                        m_supportedFormats.insert(format, modifiers);
                    }
                    continue;
                }
            }
        }
        m_supportedFormats.insert(format, {DRM_FORMAT_MOD_INVALID});
    }
    qCDebug(KWIN_OPENGL) << "EGL driver advertises" << m_supportedFormats.count() << "supported dmabuf formats" << (eglQueryDmaBufModifiersEXT != nullptr ? "with" : "without") << "modifiers";

    auto filterFormats = [this](int bpc) {
        QHash<uint32_t, QVector<uint64_t>> set;
        for (auto it = m_supportedFormats.constBegin(); it != m_supportedFormats.constEnd(); it++) {
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

QVector<KWaylandServer::LinuxDmaBufV1Feedback::Tranche> EglDmabuf::tranches() const
{
    return m_tranches;
}

QHash<uint32_t, QVector<uint64_t>> EglDmabuf::supportedFormats() const
{
    return m_supportedFormats;
}
}
