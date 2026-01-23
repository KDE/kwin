/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderdevice.h"

#include "drmdevice.h"
#include "graphicsbuffer.h"
#include "opengl/eglcontext.h"
#include "opengl/egldisplay.h"
#include "utils/common.h"

namespace KWin
{

RenderDevice::RenderDevice(std::unique_ptr<DrmDevice> &&device, std::unique_ptr<EglDisplay> &&display)
    : m_device(std::move(device))
    , m_display(std::move(display))
{
}

RenderDevice::~RenderDevice()
{
    for (const auto &image : m_importedBuffers) {
        m_display->destroyImage(image);
    }
}

DrmDevice *RenderDevice::drmDevice() const
{
    return m_device.get();
}

EglDisplay *RenderDevice::eglDisplay() const
{
    return m_display.get();
}

std::shared_ptr<EglContext> RenderDevice::eglContext(EglContext *shareContext)
{
    auto ret = m_eglContext.lock();
    if (!ret || ret->isFailed()) {
        ret = EglContext::create(m_display.get(), EGL_NO_CONFIG_KHR, shareContext ? shareContext->handle() : EGL_NO_CONTEXT);
        m_eglContext = ret;
    }
    return ret;
}

EGLImageKHR RenderDevice::importBufferAsImage(GraphicsBuffer *buffer)
{
    std::pair key(buffer, 0);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = m_display->importDmaBufAsImage(*buffer->dmabufAttributes());
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
        return image;
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
        return EGL_NO_IMAGE;
    }
}

EGLImageKHR RenderDevice::importBufferAsImage(GraphicsBuffer *buffer, int plane, int format, const QSize &size)
{
    std::pair key(buffer, plane);
    auto it = m_importedBuffers.constFind(key);
    if (Q_LIKELY(it != m_importedBuffers.constEnd())) {
        return *it;
    }

    Q_ASSERT(buffer->dmabufAttributes());
    EGLImageKHR image = m_display->importDmaBufAsImage(*buffer->dmabufAttributes(), plane, format, size);
    if (image != EGL_NO_IMAGE_KHR) {
        m_importedBuffers[key] = image;
        connect(buffer, &QObject::destroyed, this, [this, key]() {
            m_display->destroyImage(m_importedBuffers.take(key));
        });
        return image;
    } else {
        qCWarning(KWIN_OPENGL) << "failed to import dmabuf" << buffer;
        return EGL_NO_IMAGE;
    }
}

std::unique_ptr<RenderDevice> RenderDevice::open(const QString &path, int authenticatedFd)
{
    auto drmDevice = DrmDevice::openWithAuthentication(path, authenticatedFd);
    if (!drmDevice) {
        return nullptr;
    }
    auto eglDisplay = EglDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, drmDevice->gbmDevice(), nullptr));
    if (!eglDisplay) {
        return nullptr;
    }
    return std::make_unique<RenderDevice>(std::move(drmDevice), std::move(eglDisplay));
}

}
