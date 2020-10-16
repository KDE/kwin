/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_buffer_gbm.h"
#include "gbm_surface.h"

#include "logging.h"

// system
#include <sys/mman.h>
// c++
#include <cerrno>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

namespace KWin
{

// DrmSurfaceBuffer
DrmSurfaceBuffer::DrmSurfaceBuffer(int fd, const std::shared_ptr<GbmSurface> &surface)
    : DrmBuffer(fd)
    , m_surface(surface)
{
    m_bo = m_surface->lockFrontBuffer();
    if (!m_bo) {
        qCWarning(KWIN_DRM) << "Locking front buffer failed";
        return;
    }
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    if (drmModeAddFB(fd, m_size.width(), m_size.height(), 24, 32, gbm_bo_get_stride(m_bo), gbm_bo_get_handle(m_bo).u32, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed";
    }
    gbm_bo_set_user_data(m_bo, this, nullptr);
}

DrmSurfaceBuffer::~DrmSurfaceBuffer()
{
    if (m_bufferId) {
        drmModeRmFB(fd(), m_bufferId);
    }
    releaseGbm();
}

void DrmSurfaceBuffer::releaseGbm()
{
    m_surface->releaseBuffer(m_bo);
    m_bo = nullptr;
}

}
