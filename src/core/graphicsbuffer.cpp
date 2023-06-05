/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbuffer.h"

#include <drm_fourcc.h>

namespace KWin
{

GraphicsBuffer::GraphicsBuffer(QObject *parent)
    : QObject(parent)
{
}

bool GraphicsBuffer::isReferenced() const
{
    return m_refCount > 0;
}

bool GraphicsBuffer::isDropped() const
{
    return m_dropped;
}

void GraphicsBuffer::ref()
{
    ++m_refCount;
}

void GraphicsBuffer::unref()
{
    Q_ASSERT(m_refCount > 0);
    --m_refCount;
    if (!m_refCount) {
        if (m_dropped) {
            delete this;
        } else {
            Q_EMIT released();
        }
    }
}

void GraphicsBuffer::drop()
{
    m_dropped = true;

    if (!m_refCount) {
        delete this;
    }
}

void *GraphicsBuffer::map(MapFlags flags)
{
    return nullptr;
}

void GraphicsBuffer::unmap()
{
}

const DmaBufAttributes *GraphicsBuffer::dmabufAttributes() const
{
    return nullptr;
}

const ShmAttributes *GraphicsBuffer::shmAttributes() const
{
    return nullptr;
}

bool GraphicsBuffer::alphaChannelFromDrmFormat(uint32_t format)
{
    switch (format) {
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:

    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:

    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_BGRA8888:

    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:

    case DRM_FORMAT_XRGB8888_A8:
    case DRM_FORMAT_XBGR8888_A8:
    case DRM_FORMAT_RGBX8888_A8:
    case DRM_FORMAT_BGRX8888_A8:
    case DRM_FORMAT_RGB888_A8:
    case DRM_FORMAT_BGR888_A8:
    case DRM_FORMAT_RGB565_A8:
    case DRM_FORMAT_BGR565_A8:
        return true;
    default:
        return false;
    }
}

} // namespace KWin
