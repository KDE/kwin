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
#include "utils/envvar.h"

// system
#include <sys/mman.h>
#if defined(Q_OS_LINUX)
#include <linux/dma-buf.h>
#include <linux/sync_file.h>
#endif
// drm
#include <drm_fourcc.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace KWin
{

static std::optional<bool> s_disableBufferWait = environmentVariableBoolValue("KWIN_DRM_DISABLE_BUFFER_READABILITY_CHECKS");

DrmFramebufferData::DrmFramebufferData(DrmGpu *gpu, uint32_t fbid, GraphicsBuffer *buffer)
    : m_gpu(gpu)
    , m_framebufferId(fbid)
    , m_buffer(buffer)
{
}

DrmFramebufferData::~DrmFramebufferData()
{
    if (drmModeCloseFB(m_gpu->fd(), m_framebufferId) != 0) {
        drmModeRmFB(m_gpu->fd(), m_framebufferId);
    }
    if (m_buffer) {
        m_gpu->forgetBuffer(m_buffer);
    }
}

DrmFramebuffer::DrmFramebuffer(const std::shared_ptr<DrmFramebufferData> &data, GraphicsBuffer *buffer, FileDescriptor &&readFence)
    : m_data(data)
    , m_bufferRef(buffer)
{
    if (s_disableBufferWait.value_or(data->m_gpu->isVmwgfx())) {
        // buffer readability checks cause frames to be wrongly delayed on Virtual Machines running vmwgfx
        m_readable = true;
    }
    m_syncFd = std::move(readFence);
#if defined(Q_OS_LINUX)
    if (!m_syncFd.isValid()) {
        dma_buf_export_sync_file req{
            .flags = DMA_BUF_SYNC_READ,
            .fd = -1,
        };
        if (drmIoctl(buffer->dmabufAttributes()->fd[0].get(), DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &req) == 0) {
            m_syncFd = FileDescriptor{req.fd};
        }
    }
#endif
}

uint32_t DrmFramebuffer::framebufferId() const
{
    return m_data->m_framebufferId;
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
        m_readable = std::ranges::all_of(fds, [](const auto &fd) {
            return !fd.isValid() || fd.isReadable();
        });
        return m_readable;
    }
}

void DrmFramebuffer::setDeadline(std::chrono::steady_clock::time_point deadline)
{
#ifdef SYNC_IOC_SET_DEADLINE
    if (!m_syncFd.isValid()) {
        return;
    }
    sync_set_deadline args{
        .deadline_ns = uint64_t(deadline.time_since_epoch().count()),
        .pad = 0,
    };
    drmIoctl(m_syncFd.get(), SYNC_IOC_SET_DEADLINE, &args);
#endif
}

std::shared_ptr<KWin::DrmFramebufferData> DrmFramebuffer::data() const
{
    return m_data;
}
}
