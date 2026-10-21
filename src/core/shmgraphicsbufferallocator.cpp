/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/shmgraphicsbufferallocator.h"

#include "config-kwin.h"

#include "core/graphicsbuffer.h"
#include "utils/memorymap.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace KWin
{

class ShmGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    ShmGraphicsBuffer(ShmAttributes &&attributes, MemoryMap &&memoryMap);

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const ShmAttributes *shmAttributes() const override;
    uint32_t format() const override;
    uint64_t modifier() const override;
    uint32_t pitch() const override;

private:
    ShmAttributes m_attributes;
    MemoryMap m_memoryMap;
    bool m_hasAlphaChannel;
};

ShmGraphicsBuffer::ShmGraphicsBuffer(ShmAttributes &&attributes, MemoryMap &&memoryMap)
    : m_attributes(std::move(attributes))
    , m_memoryMap(std::move(memoryMap))
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(attributes.format))
{
}

GraphicsBuffer::Map ShmGraphicsBuffer::map(MapFlags flags)
{
    if (m_memoryMap.isValid()) {
        return Map{
            .data = m_memoryMap.data(),
            .stride = uint32_t(m_attributes.stride),
        };
    } else {
        return Map{};
    }
}

void ShmGraphicsBuffer::unmap()
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

uint32_t ShmGraphicsBuffer::format() const
{
    return m_attributes.format;
}

uint64_t ShmGraphicsBuffer::modifier() const
{
    return DRM_FORMAT_MOD_LINEAR;
}

uint32_t ShmGraphicsBuffer::pitch() const
{
    return m_attributes.stride;
}

std::expected<GraphicsBuffer *, std::string> ShmGraphicsBufferAllocator::allocate(const GraphicsBufferOptions &options)
{
    if (!options.software) {
        return std::unexpected("can't allocate a non-software buffer with shm");
    }
    if (!options.modifiers.empty() && !options.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
        return std::unexpected("can't allocate non-linear buffers with shm");
    }

    switch (options.format) {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        break;
    default:
        return std::unexpected(std::format("shm allocator doesn't support requested format {}{}{}{}",
                                           (options.format >> 24) & 0xFF,
                                           (options.format >> 16) & 0xFF,
                                           (options.format >> 8) & 0xFF,
                                           (options.format >> 0) & 0xFF));
    }

    const int stride = options.size.width() * 4;
    const int bufferSize = options.size.height() * stride;

#if HAVE_MEMFD
    FileDescriptor fd = FileDescriptor(memfd_create("shm", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!fd.isValid()) {
        return std::unexpected(std::format("memfd_create failed: {}", strerror(errno)));
    }

    if (ftruncate(fd.get(), bufferSize) < 0) {
        return std::unexpected(std::format("ftruncate failed: {}", strerror(errno)));
    }

    fcntl(fd.get(), F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
#else
    char templateName[] = "/tmp/kwin-shm-XXXXXX";
    FileDescriptor fd{mkstemp(templateName)};
    if (!fd.isValid()) {
        return std::unexpected(std::format("mkstemp failed: {}", strerror(errno)));
    }

    unlink(templateName);
    int flags = fcntl(fd.get(), F_GETFD);
    if (flags == -1 || fcntl(fd.get(), F_SETFD, flags | FD_CLOEXEC) == -1) {
        return std::unexpected(std::format("fnctl failed: {}", strerror(errno)));
    }

    if (ftruncate(fd.get(), bufferSize) < 0) {
        return std::unexpected(std::format("ftruncate failed: {}", strerror(errno)));
    }
#endif

    ShmAttributes attributes{
        .fd = std::move(fd),
        .stride = stride,
        .offset = 0,
        .size = options.size,
        .format = options.format,
    };

    MemoryMap memoryMap(attributes.stride * attributes.size.height(), PROT_READ | PROT_WRITE, MAP_SHARED, attributes.fd.get(), attributes.offset);
    if (!memoryMap.isValid()) {
        return std::unexpected(std::format("couldn't map shm buffer: {}", strerror(errno)));
    }

    return new ShmGraphicsBuffer(std::move(attributes), std::move(memoryMap));
}

} // namespace KWin

#include "shmgraphicsbufferallocator.moc"
