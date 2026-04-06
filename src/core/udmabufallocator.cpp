/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "udmabufallocator.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"
#include "utils/memorymap.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace KWin
{

class UDmabufGraphicsBuffer : public GraphicsBuffer
{
    Q_OBJECT

public:
    explicit UDmabufGraphicsBuffer(DmaBufAttributes &&attributes, MemoryMap &&memoryMap);

    Map map(MapFlags flags) override;
    void unmap() override;

    QSize size() const override;
    bool hasAlphaChannel() const override;
    const DmaBufAttributes *dmabufAttributes() const override;

private:
    DmaBufAttributes m_attributes;
    MemoryMap m_memoryMap;
    bool m_hasAlphaChannel;
};

UDmabufGraphicsBuffer::UDmabufGraphicsBuffer(DmaBufAttributes &&attributes, MemoryMap &&memoryMap)
    : m_attributes(std::move(attributes))
    , m_memoryMap(std::move(memoryMap))
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(attributes.format))
{
}

GraphicsBuffer::Map UDmabufGraphicsBuffer::map(MapFlags flags)
{
    if (m_memoryMap.isValid()) {
        return Map{
            .data = m_memoryMap.data(),
            .stride = uint32_t(m_attributes.pitch[0]),
        };
    } else {
        return Map{};
    }
}

void UDmabufGraphicsBuffer::unmap()
{
}

QSize UDmabufGraphicsBuffer::size() const
{
    return QSize(m_attributes.width, m_attributes.height);
}

bool UDmabufGraphicsBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

const DmaBufAttributes *UDmabufGraphicsBuffer::dmabufAttributes() const
{
    return &m_attributes;
}

UDmabufAllocator::UDmabufAllocator(FileDescriptor &&dev, dev_t devNum)
    : m_dev(std::move(dev))
    , m_devNum(devNum)
{
}

GraphicsBuffer *UDmabufAllocator::allocate(const GraphicsBufferOptions &options)
{
    if (!options.software) {
        return nullptr;
    }
    if (!options.modifiers.empty() && !options.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }

    uint32_t bytesPerPixel = 0;
    switch (options.format) {
    case DRM_FORMAT_R8:
        bytesPerPixel = 1;
        break;
    case DRM_FORMAT_RG88:
    case DRM_FORMAT_GR88:
        bytesPerPixel = 2;
        break;
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_XRGB2101010:
        bytesPerPixel = 4;
        break;
    case DRM_FORMAT_ARGB16161616:
    case DRM_FORMAT_XRGB16161616:
        bytesPerPixel = 8;
        break;
    default:
        return nullptr;
    }

    const int stride = options.size.width() * bytesPerPixel;

    // the buffer size needs to be aligned to the next page
    const uint64_t bufferSize = std::ceil(options.size.height() * stride / double(getpagesize())) * getpagesize();

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

    struct udmabuf_create create{
        .memfd = uint32_t(fd.get()),
        .flags = 0,
        .offset = 0,
        .size = bufferSize,
    };
    int err = ioctl(m_dev.get(), UDMABUF_CREATE, &create);
    if (err != 0) {
        qCWarning(KWIN_CORE, "Could not create udmabuf: %s", strerror(errno));
        return nullptr;
    }

    DmaBufAttributes attributes;
    attributes.planeCount = 1;
    attributes.width = options.size.width();
    attributes.height = options.size.height();
    attributes.format = options.format;
    attributes.modifier = DRM_FORMAT_MOD_LINEAR;
    attributes.device = m_devNum;
    attributes.fd[0] = std::move(fd);
    attributes.offset[0] = 0;
    attributes.pitch[0] = stride;

    MemoryMap memoryMap(attributes.pitch[0] * attributes.height, PROT_READ | PROT_WRITE, MAP_SHARED, attributes.fd[0].get(), attributes.offset[0]);
    if (!memoryMap.isValid()) {
        return nullptr;
    }

    return new UDmabufGraphicsBuffer(std::move(attributes), std::move(memoryMap));
}

std::unique_ptr<UDmabufAllocator> UDmabufAllocator::create()
{
    FileDescriptor fd(::open("/dev/udmabuf", O_RDWR | O_CLOEXEC));
    if (!fd.isValid()) {
        qCWarning(KWIN_CORE) << "Failed to open udmabuf";
        return nullptr;
    }
    struct stat buf;
    if (fstat(fd.get(), &buf) == -1) {
        qCWarning(KWIN_CORE) << "Failed to fstat udmabuf fd" << strerror(errno);
        return nullptr;
    }
    return std::make_unique<UDmabufAllocator>(std::move(fd), buf.st_rdev);
}

}
#include "udmabufallocator.moc"
