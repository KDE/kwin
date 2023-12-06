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
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#ifdef Q_OS_LINUX
#include <linux/dma-buf.h>
#endif

#ifndef DRM_IOCTL_MODE_CLOSEFB
#define DRM_IOCTL_MODE_CLOSEFB 0xD0
#endif

namespace KWin
{

static bool s_envIsSet = false;
static bool s_disableBufferWait = qEnvironmentVariableIntValue("KWIN_DRM_DISABLE_BUFFER_READABILITY_CHECKS", &s_envIsSet) && s_envIsSet;

DrmFramebuffer::DrmFramebuffer(DrmGpu *gpu, uint32_t fbId, GraphicsBuffer *buffer)
    : m_framebufferId(fbId)
    , m_gpu(gpu)
    , m_bufferRef(buffer)
{
    if (s_disableBufferWait || (m_gpu->isI915() && !s_envIsSet)) {
        // buffer readability checks cause frames to be wrongly delayed on some Intel laptops
        // See https://gitlab.freedesktop.org/drm/intel/-/issues/9415
        m_readable = true;
    } else {
#if defined(Q_OS_LINUX) && defined(DMA_BUF_IOCTL_EXPORT_SYNC_FILE)
        dma_buf_export_sync_file req{
            .flags = DMA_BUF_SYNC_READ,
            .fd = -1,
        };
        if (drmIoctl(buffer->dmabufAttributes()->fd[0].get(), DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &req) == 0) {
            m_syncFd = FileDescriptor{req.fd};
        }
#endif
    }
}

DrmFramebuffer::~DrmFramebuffer()
{
    uint32_t nonConstFb = m_framebufferId;
    if (drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_CLOSEFB, &nonConstFb) != 0) {
        drmIoctl(m_gpu->fd(), DRM_IOCTL_MODE_RMFB, &nonConstFb);
    }
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

const FileDescriptor &DrmFramebuffer::syncFd() const
{
    return m_syncFd;
}

bool DrmFramebuffer::isReadable()
{
    if (m_readable) {
        return true;
    } else if (m_syncFd.isValid()) {
        return m_readable = m_syncFd.isReadable();
    } else {
        const auto &fds = m_bufferRef->dmabufAttributes()->fd;
        return m_readable = std::all_of(fds.begin(), fds.end(), [](const auto &fd) {
                   return !fd.isValid() || fd.isReadable();
               });
    }
}
}
