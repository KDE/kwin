/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_buffer.h"

#include "core/graphicsbuffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"

// system
#include <sys/mman.h>
// c++
#include <cerrno>
// drm
#include <drm_fourcc.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

DrmFramebuffer::DrmFramebuffer(DrmGpu *gpu, uint32_t fbId, GraphicsBuffer *buffer)
    : m_framebufferId(fbId)
    , m_gpu(gpu)
    , m_bufferRef(buffer)
{
}

DrmFramebuffer::~DrmFramebuffer()
{
    drmModeRmFB(m_gpu->fd(), m_framebufferId);
}

uint32_t DrmFramebuffer::framebufferId() const
{
    return m_framebufferId;
}

GraphicsBuffer *DrmFramebuffer::buffer() const
{
    return *m_bufferRef;
}

void DrmFramebuffer::releaseBuffer()
{
    m_bufferRef = nullptr;
}
}
