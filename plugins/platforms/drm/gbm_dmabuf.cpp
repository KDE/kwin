/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "gbm_dmabuf.h"
#include "kwineglimagetexture.h"
#include "platformsupport/scenes/opengl/drm_fourcc.h"
#include "main.h"
#include "platform.h"
#include <unistd.h>

namespace KWin
{

GbmDmaBuf::GbmDmaBuf(GLTexture *texture, gbm_bo *bo, int fd)
    : DmaBufTexture(texture)
    , m_bo(bo)
    , m_fd(fd)
{}

GbmDmaBuf::~GbmDmaBuf()
{
    m_texture.reset(nullptr);

    close(m_fd);
    gbm_bo_destroy(m_bo);
}


KWin::GbmDmaBuf *GbmDmaBuf::createBuffer(const QSize &size, gbm_device *device)
{
    if (!device) {
        return nullptr;
    }

    auto bo = gbm_bo_create(device, size.width(), size.height(), GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);

    if (!bo) {
        gbm_bo_destroy(bo);
        return nullptr;
    }

    const int fd = gbm_bo_get_fd(bo);
    if (fd < 0) {
        gbm_bo_destroy(bo);
        return nullptr;
    }

    EGLint importAttributes[] = {
        EGL_WIDTH, EGLint(gbm_bo_get_width(bo)),
        EGL_HEIGHT, EGLint(gbm_bo_get_height(bo)),
        EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGLint(gbm_bo_get_offset(bo, 0)),
        EGL_DMA_BUF_PLANE0_PITCH_EXT, EGLint(gbm_bo_get_stride(bo)),
        EGL_NONE
    };

    EGLDisplay display = kwinApp()->platform()->sceneEglDisplay();
    EGLImageKHR destinationImage = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, importAttributes);
    if (destinationImage == EGL_NO_IMAGE_KHR) {
        return nullptr;
    }

    return new GbmDmaBuf(new KWin::EGLImageTexture(display, destinationImage, GL_RGBA8, size), bo, fd);
}

}

