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
}
