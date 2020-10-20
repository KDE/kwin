/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dmabufhandle.h"
#include "kwinscreencast_logging.h"
#include "main.h"
#include "platform.h"

#include <kwingltexture.h>

#include <unistd.h>

namespace KWin
{

DmaBufHandle::DmaBufHandle()
    : m_eglDisplay(kwinApp()->platform()->sceneEglDisplay())
{
    m_fileDescriptors.fill(-1);
    m_strides.fill(0);
    m_offsets.fill(0);
}

DmaBufHandle::~DmaBufHandle()
{
    destroy();
}

bool DmaBufHandle::create()
{
    if (!m_texture) {
        qCWarning(KWIN_SCREENCAST) << "DmaBufHandle::create() requires a valid OpenGL texture";
        return false;
    }
    if (!m_eglContext) {
        qCWarning(KWIN_SCREENCAST) << "DmaBufHandle::create() requires a valid EGL context";
        return false;
    }

    m_eglImage = eglCreateImageKHR(m_eglDisplay, m_eglContext, EGL_GL_TEXTURE_2D_KHR,
                                   reinterpret_cast<EGLClientBuffer>(m_texture->texture()), nullptr);
    if (m_eglImage == EGL_NO_IMAGE_KHR) {
        qCWarning(KWIN_SCREENCAST, "Failed to create EGLImage for exported texture: 0x%x",
                  eglGetError());
        return false;
    }

    if (!eglExportDMABUFImageQueryMESA(m_eglDisplay, m_eglImage, &m_fourcc, &m_planeCount, nullptr)) {
        qCWarning(KWIN_SCREENCAST, "Failed to query params of exported dmabuf texture: 0x%x",
                  eglGetError());
        destroy();
        return false;
    }

    if (!eglExportDMABUFImageMESA(m_eglDisplay, m_eglImage, m_fileDescriptors.data(),
                                  m_strides.data(), m_offsets.data())) {
        qCWarning(KWIN_SCREENCAST, "Failed to export a dmabuf texture: 0x%x", eglGetError());
        destroy();
        return false;
    }

    m_isCreated = true;

    return true;
}

void DmaBufHandle::destroy()
{
    for (int i = 0; i < m_planeCount; ++i) {
        if (m_fileDescriptors[i] != -1) {
            close(m_fileDescriptors[i]);
        }
    }
    if (m_eglImage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(m_eglDisplay, m_eglImage);
    }

    m_fileDescriptors.fill(-1);
    m_strides.fill(0);
    m_offsets.fill(0);
    m_eglImage = EGL_NO_IMAGE_KHR;
    m_planeCount = 0;
    m_fourcc = 0;
    m_isCreated = false;
}

GLTexture *DmaBufHandle::texture() const
{
    return m_texture;
}

void DmaBufHandle::setTexture(GLTexture *texture)
{
    if (m_isCreated) {
        qCWarning(KWIN_SCREENCAST) << "Can't set a texture to an already created dmabuf handle";
    } else {
        m_texture = texture;
    }
}

EGLContext DmaBufHandle::context() const
{
    return m_eglContext;
}

void DmaBufHandle::setContext(EGLContext context)
{
    if (m_isCreated) {
        qCWarning(KWIN_SCREENCAST) << "Can't change the OpenGL for an already created dma-buf handle";
    } else {
        m_eglContext = context;
    }
}

int DmaBufHandle::planeCount() const
{
    return m_planeCount;
}

int DmaBufHandle::fourcc() const
{
    return m_fourcc;
}

int DmaBufHandle::fileDescriptor(int plane) const
{
    if (plane >= 0 && plane < m_planeCount) {
        return m_fileDescriptors[plane];
    } else {
        qCWarning(KWIN_SCREENCAST) << "Retrieving the fd for invalid plane" << plane;
        return -1;
    }
}

int DmaBufHandle::stride(int plane) const
{
    if (plane >= 0 && plane < m_planeCount) {
        return m_strides[plane];
    } else {
        qCWarning(KWIN_SCREENCAST) << "Retrieving the stride for invalid plane" << plane;
        return 0;
    }
}

int DmaBufHandle::offset(int plane) const
{
    if (plane >= 0 && plane < m_planeCount) {
        return m_offsets[plane];
    } else {
        qCWarning(KWIN_SCREENCAST) << "Retrieving the offset for invalid plane" << plane;
        return 0;
    }
}

} // namespace KWin
