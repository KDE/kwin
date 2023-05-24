/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/dumbgraphicsbufferallocator.h"
#include "utils/common.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/mman.h>
#include <xf86drm.h>

namespace KWin
{

DumbGraphicsBuffer::DumbGraphicsBuffer(int drmFd, uint32_t handle, DmaBufAttributes attributes)
    : m_drmFd(drmFd)
    , m_handle(handle)
    , m_size(attributes.pitch[0] * attributes.height)
    , m_dmabufAttributes(std::move(attributes))
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(m_dmabufAttributes.format))
{
}

DumbGraphicsBuffer::~DumbGraphicsBuffer()
{
    unmap();

    drm_mode_destroy_dumb destroyArgs{
        .handle = m_handle,
    };
    drmIoctl(m_drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
}

QSize DumbGraphicsBuffer::size() const
{
    return QSize(m_dmabufAttributes.width, m_dmabufAttributes.height);
}

bool DumbGraphicsBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

const DmaBufAttributes *DumbGraphicsBuffer::dmabufAttributes() const
{
    return &m_dmabufAttributes;
}

void *DumbGraphicsBuffer::map()
{
    if (m_data) {
        return m_data;
    }

    drm_mode_map_dumb mapArgs{
        .handle = m_handle,
    };
    if (drmIoctl(m_drmFd, DRM_IOCTL_MODE_MAP_DUMB, &mapArgs) != 0) {
        qCWarning(KWIN_CORE) << "DRM_IOCTL_MODE_MAP_DUMB failed:" << strerror(errno);
        return nullptr;
    }

    void *address = mmap(nullptr, m_size, PROT_WRITE, MAP_SHARED, m_drmFd, mapArgs.offset);
    if (address == MAP_FAILED) {
        qCWarning(KWIN_CORE) << "mmap() failed:" << strerror(errno);
        return nullptr;
    }

    m_data = address;
    return m_data;
}

void DumbGraphicsBuffer::unmap()
{
    if (m_data) {
        munmap(m_data, m_size);
        m_data = nullptr;
    }
}

DumbGraphicsBufferAllocator::DumbGraphicsBufferAllocator(int drmFd)
    : m_drmFd(drmFd)
{
}

DumbGraphicsBuffer *DumbGraphicsBufferAllocator::allocate(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    if (!modifiers.isEmpty()) {
        return nullptr;
    }

    drm_mode_create_dumb createArgs{
        .height = uint32_t(size.height()),
        .width = uint32_t(size.width()),
        .bpp = 32,
    };
    if (drmIoctl(m_drmFd, DRM_IOCTL_MODE_CREATE_DUMB, &createArgs) != 0) {
        qCWarning(KWIN_CORE) << "DRM_IOCTL_MODE_CREATE_DUMB failed:" << strerror(errno);
        return nullptr;
    }

    int primeFd;
    if (drmPrimeHandleToFD(m_drmFd, createArgs.handle, DRM_CLOEXEC, &primeFd) != 0) {
        qCWarning(KWIN_CORE) << "drmPrimeHandleToFD() failed:" << strerror(errno);
        drm_mode_destroy_dumb destroyArgs{
            .handle = createArgs.handle,
        };
        drmIoctl(m_drmFd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroyArgs);
        return nullptr;
    }

    return new DumbGraphicsBuffer(m_drmFd, createArgs.handle, DmaBufAttributes{
        .planeCount = 1,
        .width = size.width(),
        .height = size.height(),
        .format = format,
        .modifier = DRM_FORMAT_MOD_LINEAR,
        .fd = {FileDescriptor(primeFd), FileDescriptor{}, FileDescriptor{}, FileDescriptor{}},
        .offset = {0, 0, 0, 0},
        .pitch = {int(createArgs.pitch), 0, 0, 0},
    });
}

} // namespace KWin
