/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "udmabufallocator.h"
#include "core/gpumanager.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"
#include "utils/memorymap.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(Q_OS_LINUX)
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#endif

namespace KWin
{

class UdmabufGraphicsBuffer : public GraphicsBuffer
{
public:
    explicit UdmabufGraphicsBuffer(DmaBufAttributes &&attributes, MemoryMap &&map);

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

private:
    const DmaBufAttributes m_attributes;
    const bool m_hasAlphaChannel;
    MemoryMap m_memoryMap;
    uint32_t m_mapCount = 0;
};

static uint64_t align(uint64_t size, uint64_t minimum)
{
    if (auto remainder = size % minimum) {
        return size + (minimum - remainder);
    } else {
        return size;
    }
}

UdmabufGraphicsBuffer::UdmabufGraphicsBuffer(DmaBufAttributes &&attributes, MemoryMap &&memoryMap)
    : m_attributes(std::move(attributes))
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(attributes.format))
    , m_memoryMap(std::move(memoryMap))
{
}

GraphicsBuffer::Map UdmabufGraphicsBuffer::map(MapFlags flags)
{
#if defined(Q_OS_LINUX)
    if (m_mapCount == 1) {
        struct dma_buf_sync sync = {
            .flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_READ,
        };
        ioctl(m_attributes.fd[0].get(), DMA_BUF_IOCTL_SYNC, &sync);
    }
#endif
    return Map{
        .data = m_memoryMap.data(),
        .stride = uint32_t(m_attributes.pitch[0]),
    };
}

void UdmabufGraphicsBuffer::unmap()
{
    Q_ASSERT(m_mapCount > 0);
    m_mapCount--;
    if (m_mapCount == 0) {
#if defined(Q_OS_LINUX)
        struct dma_buf_sync sync = {
            .flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_WRITE | DMA_BUF_SYNC_READ,
        };
        ioctl(m_attributes.fd[0].get(), DMA_BUF_IOCTL_SYNC, &sync);
#endif
    }
}

QSize UdmabufGraphicsBuffer::size() const
{
    return QSize(m_attributes.width, m_attributes.height);
}

bool UdmabufGraphicsBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

const DmaBufAttributes *UdmabufGraphicsBuffer::dmabufAttributes() const
{
    return &m_attributes;
}

UDmabufAllocator::UDmabufAllocator()
{
}

std::shared_ptr<GraphicsBuffer> UDmabufAllocator::allocate(const GraphicsBufferOptions &options)
{
    if (!options.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }
    return allocate(options.format, options.size);
}

std::shared_ptr<GraphicsBuffer> UDmabufAllocator::allocate(uint32_t format, const QSize &size)
{
#if HAVE_MEMFD && defined(Q_OS_LINUX)
    if (!GpuManager::self()->udmabuf().isValid()) {
        return nullptr;
    }
    auto info = FormatInfo::get(format);
    if (!info) {
        return nullptr;
    }
    const int stride = align(size.width() * info->bitsPerPixel / 8, 256);
    const int bufferSize = align(size.height() * stride, getpagesize());

    FileDescriptor fd = FileDescriptor(memfd_create("udmabuf", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (!fd.isValid()) {
        qCWarning(KWIN_CORE, "Creating memfd for udmabuf failed!");
        return nullptr;
    }

    if (ftruncate(fd.get(), bufferSize) < 0) {
        qCWarning(KWIN_CORE, "Resizing memfd for udmabuf failed!");
        return nullptr;
    }
    if (fcntl(fd.get(), F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL) != 0) {
        qCWarning(KWIN_CORE, "Sealing memfd for udmabuf failed!");
        return nullptr;
    }

    MemoryMap memoryMap(stride * size.height(), PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
    if (!memoryMap.isValid()) {
        qCWarning(KWIN_CORE, "Mapping memfd for udmabuf failed!");
        return nullptr;
    }

    ShmAttributes attributes{
        .fd = std::move(fd),
        .stride = stride,
        .offset = 0,
        .size = size,
        .format = format,
    };
    auto dmabufAttributes = GpuManager::self()->createUdmabuf(&attributes);
    if (!dmabufAttributes) {
        return nullptr;
    }
    return std::make_shared<UdmabufGraphicsBuffer>(std::move(*dmabufAttributes), std::move(memoryMap));
#else
    return nullptr;
#endif
}

}
