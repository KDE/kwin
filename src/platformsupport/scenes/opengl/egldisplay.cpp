/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egldisplay.h"
#include "core/dmabufattributes.h"
#include "kwineglimagetexture.h"
#include "kwineglutils_p.h"
#include "libkwineffects/kwinglutils.h"
#include "utils/common.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

namespace KWin
{

static bool isOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

std::unique_ptr<EglDisplay> EglDisplay::create(::EGLDisplay display, bool owning)
{
    if (!display) {
        return nullptr;
    }
    EGLint major, minor;
    if (eglInitialize(display, &major, &minor) == EGL_FALSE) {
        qCWarning(KWIN_OPENGL) << "eglInitialize failed";
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {
            qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        }
        return nullptr;
    }
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_OPENGL) << "Error during eglInitialize " << error;
        return nullptr;
    }
    qCDebug(KWIN_OPENGL) << "Egl Initialize succeeded";
    if (eglBindAPI(isOpenGLES() ? EGL_OPENGL_ES_API : EGL_OPENGL_API) == EGL_FALSE) {
        qCCritical(KWIN_OPENGL) << "bind OpenGL API failed";
        return nullptr;
    }
    qCDebug(KWIN_OPENGL) << "EGL version: " << major << "." << minor;

    const auto extensions = QByteArray(eglQueryString(display, EGL_EXTENSIONS)).split(' ');

    const QByteArray requiredExtensions[] = {
        QByteArrayLiteral("EGL_KHR_no_config_context"),
        QByteArrayLiteral("EGL_KHR_surfaceless_context"),
    };
    for (const QByteArray &extensionName : requiredExtensions) {
        if (!extensions.contains(extensionName)) {
            qCWarning(KWIN_OPENGL) << extensionName << "extension is unsupported";
            return nullptr;
        }
    }

    return std::make_unique<EglDisplay>(display, extensions, owning);
}

EglDisplay::EglDisplay(::EGLDisplay display, const QList<QByteArray> &extensions, bool owning)
    : m_handle(display)
    , m_extensions(extensions)
    , m_owning(owning)
    , m_supportsBufferAge(extensions.contains(QByteArrayLiteral("EGL_EXT_buffer_age")) && qgetenv("KWIN_USE_BUFFER_AGE") != "0")
    , m_supportsSwapBuffersWithDamage(extensions.contains(QByteArrayLiteral("EGL_EXT_swap_buffers_with_damage")))
    , m_supportsNativeFence(extensions.contains(QByteArrayLiteral("EGL_ANDROID_native_fence_sync")))
    , m_importFormats(queryImportFormats())
{
}

EglDisplay::~EglDisplay()
{
    if (m_owning) {
        eglTerminate(m_handle);
    }
}

QList<QByteArray> EglDisplay::extensions() const
{
    return m_extensions;
}

::EGLDisplay EglDisplay::handle() const
{
    return m_handle;
}

bool EglDisplay::hasExtension(const QByteArray &name) const
{
    return m_extensions.contains(name);
}

static bool checkExtension(const QByteArrayView extensions, const QByteArrayView extension)
{
    for (int i = 0; i < extensions.size();) {
        if (extensions[i] == ' ') {
            i++;
            continue;
        }
        int next = extensions.indexOf(' ', i);
        if (next == -1) {
            next = extensions.size();
        }

        const int size = next - i;
        if (extension.size() == size && extensions.sliced(i, size) == extension) {
            return true;
        }

        i = next;
    }

    return false;
}

QString EglDisplay::renderNode() const
{
    const char *clientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (checkExtension(clientExtensions, "EGL_EXT_device_query")) {
        EGLAttrib eglDeviceAttrib;
        if (eglQueryDisplayAttribEXT(m_handle, EGL_DEVICE_EXT, &eglDeviceAttrib)) {
            EGLDeviceEXT eglDevice = reinterpret_cast<EGLDeviceEXT>(eglDeviceAttrib);

            const char *deviceExtensions = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
            if (checkExtension(deviceExtensions, "EGL_EXT_device_drm_render_node")) {
                if (const char *node = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT)) {
                    return QString::fromLocal8Bit(node);
                }
            }
            if (checkExtension(deviceExtensions, "EGL_EXT_device_drm")) {
                // Fallback to display device.
                if (const char *node = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT)) {
                    return QString::fromLocal8Bit(node);
                }
            }
        }
    }
    return QString();
}

bool EglDisplay::supportsBufferAge() const
{
    return m_supportsBufferAge;
}

bool EglDisplay::supportsSwapBuffersWithDamage() const
{
    return m_supportsSwapBuffersWithDamage;
}

bool EglDisplay::supportsNativeFence() const
{
    return m_supportsNativeFence;
}

EGLImageKHR EglDisplay::importDmaBufAsImage(const DmaBufAttributes &dmabuf) const
{
    QVector<EGLint> attribs;
    attribs.reserve(6 + dmabuf.planeCount * 10 + 1);

    attribs << EGL_WIDTH << dmabuf.width
            << EGL_HEIGHT << dmabuf.height
            << EGL_LINUX_DRM_FOURCC_EXT << dmabuf.format;

    attribs << EGL_DMA_BUF_PLANE0_FD_EXT << dmabuf.fd[0].get()
            << EGL_DMA_BUF_PLANE0_OFFSET_EXT << dmabuf.offset[0]
            << EGL_DMA_BUF_PLANE0_PITCH_EXT << dmabuf.pitch[0];
    if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
        attribs << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
    }

    if (dmabuf.planeCount > 1) {
        attribs << EGL_DMA_BUF_PLANE1_FD_EXT << dmabuf.fd[1].get()
                << EGL_DMA_BUF_PLANE1_OFFSET_EXT << dmabuf.offset[1]
                << EGL_DMA_BUF_PLANE1_PITCH_EXT << dmabuf.pitch[1];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 2) {
        attribs << EGL_DMA_BUF_PLANE2_FD_EXT << dmabuf.fd[2].get()
                << EGL_DMA_BUF_PLANE2_OFFSET_EXT << dmabuf.offset[2]
                << EGL_DMA_BUF_PLANE2_PITCH_EXT << dmabuf.pitch[2];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    if (dmabuf.planeCount > 3) {
        attribs << EGL_DMA_BUF_PLANE3_FD_EXT << dmabuf.fd[3].get()
                << EGL_DMA_BUF_PLANE3_OFFSET_EXT << dmabuf.offset[3]
                << EGL_DMA_BUF_PLANE3_PITCH_EXT << dmabuf.pitch[3];
        if (dmabuf.modifier != DRM_FORMAT_MOD_INVALID) {
            attribs << EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT << EGLint(dmabuf.modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT << EGLint(dmabuf.modifier >> 32);
        }
    }

    attribs << EGL_NONE;

    return eglCreateImageKHR(m_handle, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
}

QHash<uint32_t, QList<uint64_t>> EglDisplay::supportedDrmFormats() const
{
    return m_importFormats;
}

QHash<uint32_t, QList<uint64_t>> EglDisplay::queryImportFormats() const
{
    if (!hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import")) || !hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        return {};
    }

    typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func)(EGLDisplay dpy, EGLint max_formats, EGLint * formats, EGLint * num_formats);
    typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func)(EGLDisplay dpy, EGLint format, EGLint max_modifiers, EGLuint64KHR * modifiers, EGLBoolean * external_only, EGLint * num_modifiers);
    eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
    eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;
    eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    if (eglQueryDmaBufFormatsEXT == nullptr) {
        return {};
    }

    EGLint count = 0;
    EGLBoolean success = eglQueryDmaBufFormatsEXT(m_handle, 0, nullptr, &count);
    if (!success || count == 0) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT failed!" << getEglErrorString();
        return {};
    }
    QVector<uint32_t> formats(count);
    if (!eglQueryDmaBufFormatsEXT(m_handle, count, (EGLint *)formats.data(), &count)) {
        qCCritical(KWIN_OPENGL) << "eglQueryDmaBufFormatsEXT with count" << count << "failed!" << getEglErrorString();
        return {};
    }
    QHash<uint32_t, QVector<uint64_t>> ret;
    for (const auto format : std::as_const(formats)) {
        if (eglQueryDmaBufModifiersEXT != nullptr) {
            EGLint count = 0;
            const EGLBoolean success = eglQueryDmaBufModifiersEXT(m_handle, format, 0, nullptr, nullptr, &count);
            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                QVector<EGLBoolean> externalOnly(count);
                if (eglQueryDmaBufModifiersEXT(m_handle, format, count, modifiers.data(), nullptr, &count)) {
                    for (int i = modifiers.size() - 1; i >= 0; i--) {
                        if (externalOnly[i]) {
                            modifiers.remove(i);
                            externalOnly.remove(i);
                        }
                    }
                    if (!modifiers.empty()) {
                        ret.insert(format, modifiers);
                    }
                    continue;
                }
            }
        }
        ret.insert(format, {DRM_FORMAT_MOD_INVALID});
    }
    return ret;
}

}
