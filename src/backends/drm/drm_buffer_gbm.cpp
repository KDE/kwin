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
#include "drm_gpu.h"

// system
#include <sys/mman.h>
// c++
#include <cerrno>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
// KWaylandServer
#include "KWaylandServer/clientbuffer.h"
#include <drm_fourcc.h>

namespace KWin
{

GbmBuffer::GbmBuffer(GbmSurface *surface, gbm_bo *bo)
    : m_surface(surface)
    , m_bo(bo)
{
    m_stride = gbm_bo_get_stride(m_bo);
}

GbmBuffer::GbmBuffer(gbm_bo *buffer, KWaylandServer::ClientBuffer *clientBuffer)
    : m_bo(buffer)
    , m_clientBuffer(clientBuffer)
    , m_stride(gbm_bo_get_stride(m_bo))
{
    if (m_clientBuffer) {
        m_clientBuffer->ref();
    }
}

GbmBuffer::~GbmBuffer()
{
    releaseBuffer();
}

void GbmBuffer::releaseBuffer()
{
    if (m_clientBuffer) {
        m_clientBuffer->unref();
        m_clientBuffer = nullptr;
    }
    if (!m_bo) {
        return;
    }
    if (m_mapping) {
        gbm_bo_unmap(m_bo, m_mapping);
    }
    if (m_surface) {
        m_surface->releaseBuffer(this);
        m_surface = nullptr;
    } else {
        gbm_bo_destroy(m_bo);
    }
    m_bo = nullptr;
}

bool GbmBuffer::map(uint32_t flags)
{
    if (m_data) {
        return true;
    }
    if (!m_bo) {
        return false;
    }
    m_data = gbm_bo_map(m_bo, 0, 0, gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo), flags, &m_stride, &m_mapping);
    return m_data;
}

KWaylandServer::ClientBuffer *GbmBuffer::clientBuffer() const
{
    return m_clientBuffer;
}

gbm_bo* GbmBuffer::getBo() const
{
    return m_bo;
}

void *GbmBuffer::mappedData() const
{
    return m_data;
}

uint32_t GbmBuffer::stride() const
{
    return m_stride;
}


DrmGbmBuffer::DrmGbmBuffer(DrmGpu *gpu, GbmSurface *surface, gbm_bo *bo)
    : DrmBuffer(gpu, gbm_bo_get_format(bo), gbm_bo_get_modifier(bo)), GbmBuffer(surface, bo)
{
    initialize();
}

DrmGbmBuffer::DrmGbmBuffer(DrmGpu *gpu, gbm_bo *buffer, KWaylandServer::ClientBuffer *clientBuffer)
    : DrmBuffer(gpu, gbm_bo_get_format(buffer), gbm_bo_get_modifier(buffer)), GbmBuffer(buffer, clientBuffer)
{
    initialize();
}

DrmGbmBuffer::~DrmGbmBuffer()
{
    if (m_bufferId) {
        if (drmModeRmFB(m_gpu->fd(), m_bufferId) != 0) {
            qCCritical(KWIN_DRM) << "drmModeRmFB on GPU" << m_gpu->devNode() << "failed!" << strerror(errno);
        }
    }
}

void DrmGbmBuffer::initialize()
{
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    uint32_t handles[4] = { };
    uint32_t strides[4] = { };
    uint32_t offsets[4] = { };
    uint64_t modifiers[4] = { };

    if (gbm_bo_get_handle_for_plane(m_bo, 0).s32 != -1) {
        for (int i = 0; i < gbm_bo_get_plane_count(m_bo); i++) {
            handles[i] = gbm_bo_get_handle_for_plane(m_bo, i).u32;
            strides[i] = gbm_bo_get_stride_for_plane(m_bo, i);
            offsets[i] = gbm_bo_get_offset(m_bo, i);
            modifiers[i] = m_modifier;
        }
    } else {
        handles[0] = gbm_bo_get_handle(m_bo).u32;
        strides[0] = gbm_bo_get_stride(m_bo);
        modifiers[0] = DRM_FORMAT_MOD_INVALID;
    }

    if (modifiers[0] != DRM_FORMAT_MOD_INVALID && m_gpu->addFB2ModifiersSupported()) {
        if (drmModeAddFB2WithModifiers(m_gpu->fd(), m_size.width(), m_size.height(), m_format, handles, strides, offsets, modifiers, &m_bufferId, DRM_MODE_FB_MODIFIERS)) {
            if (m_surface) {
                gbm_format_name_desc name;
                gbm_format_get_name(m_format, &name);
                qCCritical(KWIN_DRM) << "drmModeAddFB2WithModifiers on GPU" << m_gpu->devNode() << "failed for a buffer with format" << name.name << "and modifier" << modifiers[0] << strerror(errno);
            }
        }
    } else {
        if (drmModeAddFB2(m_gpu->fd(), m_size.width(), m_size.height(), m_format, handles, strides, offsets, &m_bufferId, 0)) {
            // fallback
            if (drmModeAddFB(m_gpu->fd(), m_size.width(), m_size.height(), 24, 32, strides[0], handles[0], &m_bufferId) != 0) {
                if (m_surface) {
                    gbm_format_name_desc name;
                    gbm_format_get_name(m_format, &name);
                    qCCritical(KWIN_DRM) << "drmModeAddFB2 and drmModeAddFB both failed on GPU" << m_gpu->devNode() << "for a buffer with format" << name.name << "and modifier" << modifiers[0] << strerror(errno);
                }
            }
        }
    }

    gbm_bo_set_user_data(m_bo, this, nullptr);
}

bool DrmGbmBuffer::needsModeChange(DrmBuffer *b) const
{
    if (DrmGbmBuffer *sb = dynamic_cast<DrmGbmBuffer*>(b)) {
        return hasBo() != sb->hasBo();
    } else {
        return true;
    }
}

bool DrmGbmBuffer::hasBo() const
{
    return m_bo != nullptr;
}

}
