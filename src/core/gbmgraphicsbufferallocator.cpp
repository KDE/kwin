/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/gbmgraphicsbufferallocator.h"
#include "backends/drm/gbm_dmabuf.h" // FIXME: move dmaBufAttributesForBo() elsewhere

#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{

GbmGraphicsBufferAllocator::GbmGraphicsBufferAllocator(gbm_device *device)
    : m_gbmDevice(device)
{
}

GbmGraphicsBufferAllocator::~GbmGraphicsBufferAllocator()
{
}

GbmGraphicsBuffer *GbmGraphicsBufferAllocator::allocate(const GraphicsBufferOptions &options)
{
    if (options.software) {
        return nullptr;
    }

    if (!options.modifiers.isEmpty() && !(options.modifiers.size() == 1 && options.modifiers.first() == DRM_FORMAT_MOD_INVALID)) {
        gbm_bo *bo = gbm_bo_create_with_modifiers(m_gbmDevice,
                                                  options.size.width(),
                                                  options.size.height(),
                                                  options.format,
                                                  options.modifiers.constData(),
                                                  options.modifiers.size());
        if (bo) {
            std::optional<DmaBufAttributes> attributes = dmaBufAttributesForBo(bo);
            if (!attributes.has_value()) {
                gbm_bo_destroy(bo);
                return nullptr;
            }
            return new GbmGraphicsBuffer(std::move(attributes.value()), bo);
        }
    }

    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    if (options.modifiers.size() == 1 && options.modifiers.first() == DRM_FORMAT_MOD_LINEAR) {
        flags |= GBM_BO_USE_LINEAR;
    } else if (!modifiers.contains(DRM_FORMAT_MOD_INVALID)) {
        return nullptr;
    }

    gbm_bo *bo = gbm_bo_create(m_gbmDevice,
                               options.size.width(),
                               options.size.height(),
                               options.format,
                               flags);
    if (bo) {
        std::optional<DmaBufAttributes> attributes = dmaBufAttributesForBo(bo);
        if (!attributes.has_value()) {
            gbm_bo_destroy(bo);
            return nullptr;
        }
        if (flags & GBM_BO_USE_LINEAR) {
            attributes->modifier = DRM_FORMAT_MOD_LINEAR;
        } else {
            attributes->modifier = DRM_FORMAT_MOD_INVALID;
        }
        return new GbmGraphicsBuffer(std::move(attributes.value()), bo);
    }

    return nullptr;
}

GbmGraphicsBuffer::GbmGraphicsBuffer(DmaBufAttributes attributes, gbm_bo *handle)
    : m_bo(handle)
    , m_dmabufAttributes(std::move(attributes))
    , m_size(m_dmabufAttributes.width, m_dmabufAttributes.height)
    , m_hasAlphaChannel(alphaChannelFromDrmFormat(m_dmabufAttributes.format))
{
}

GbmGraphicsBuffer::~GbmGraphicsBuffer()
{
    gbm_bo_destroy(m_bo);
}

QSize GbmGraphicsBuffer::size() const
{
    return m_size;
}

bool GbmGraphicsBuffer::hasAlphaChannel() const
{
    return m_hasAlphaChannel;
}

const DmaBufAttributes *GbmGraphicsBuffer::dmabufAttributes() const
{
    return &m_dmabufAttributes;
}

} // namespace KWin
