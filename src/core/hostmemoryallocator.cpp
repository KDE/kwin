/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "hostmemoryallocator.h"
#include "core/graphicsbuffer.h"

namespace KWin
{

class HostMemoryBuffer : public GraphicsBuffer
{
public:
    explicit HostMemoryBuffer(HostMemoryAttributes &&attributes);

    const HostMemoryAttributes *hostDataAttributes() const override;
    QSize size() const override;
    bool hasAlphaChannel() const override;

private:
    const HostMemoryAttributes m_attributes;
    const bool m_hasAlphaChannel;
};

HostMemoryBuffer::HostMemoryBuffer(HostMemoryAttributes &&attributes)
    : m_attributes(std::move(attributes))
    , m_hasAlphaChannel(FormatInfo::get(m_attributes.format)->alphaBits > 0)
{
}

const HostMemoryAttributes *HostMemoryBuffer::hostDataAttributes() const
{
    return &m_attributes;
}

QSize HostMemoryBuffer::size() const
{
    return m_attributes.size;
}

bool HostMemoryBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

HostMemoryGraphicsBufferAllocator::HostMemoryGraphicsBufferAllocator(size_t alignment)
    : m_alignment(alignment)
{
}

GraphicsBuffer *HostMemoryGraphicsBufferAllocator::allocate(const GraphicsBufferOptions &options)
{
    if (!options.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
        return nullptr;
    }
    const auto info = FormatInfo::get(options.format);
    if (!info) {
        return nullptr;
    }
    const uint32_t stride = options.size.width() * info->bitsPerPixel / 8;
    const size_t size = align(stride * options.size.height(), m_alignment);
    std::unique_ptr<std::byte[]> data(new (std::align_val_t(m_alignment)) std::byte[size]());
    if (!data) {
        return nullptr;
    }
    return new HostMemoryBuffer(HostMemoryAttributes{
        .data = std::move(data),
        .size = options.size,
        .stride = stride,
        .format = options.format,
        .sizeInBytes = size,
    });
}

}
