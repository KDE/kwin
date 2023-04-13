/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/shmgraphicsbufferallocator.h"

#include "config-kwin.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>

namespace KWin
{

ShmGraphicsBuffer::ShmGraphicsBuffer(ShmAttributes &&attributes)
    : m_attributes(std::move(attributes))
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(attributes.format))
{
}

QSize ShmGraphicsBuffer::size() const
{
    return m_attributes.size;
}

bool ShmGraphicsBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

const ShmAttributes *ShmGraphicsBuffer::shmAttributes() const
{
    return &m_attributes;
}

ShmGraphicsBuffer *ShmGraphicsBufferAllocator::allocate(const QSize &size, uint32_t format, const QVector<uint64_t> &modifiers)
{
    if (!modifiers.isEmpty()) {
        return nullptr;
    }

    switch (format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        break;
    default:
        return nullptr;
    }

    const int stride = size.width() * 4;
    const int bufferSize = size.height() * stride;

#if HAVE_MEMFD
    FileDescriptor fd = FileDescriptor(memfd_create("shm", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!fd.isValid()) {
        return nullptr;
    }

    if (ftruncate(fd.get(), bufferSize) < 0) {
        return nullptr;
    }

    fcntl(fd.get(), F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
#else
    char templateName[] = "/tmp/kwin-shm-XXXXXX";
    FileDescriptor fd{mkstemp(templateName)};
    if (!fd.isValid()) {
        return nullptr;
    }

    unlink(templateName);
    int flags = fcntl(fd.get(), F_GETFD);
    if (flags == -1 || fcntl(fd.get(), F_SETFD, flags | FD_CLOEXEC) == -1) {
        return nullptr;
    }

    if (ftruncate(fd.get(), bufferSize) < 0) {
        return nullptr;
    }
#endif

    return new ShmGraphicsBuffer(ShmAttributes{
        .fd = std::move(fd),
        .stride = stride,
        .offset = 0,
        .size = size,
        .format = format,
    });
}

} // namespace KWin
