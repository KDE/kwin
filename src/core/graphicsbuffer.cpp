/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbuffer.h"
#include "utils/drm_format_helper.h"

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
    const auto info = formatInfo(format);
    return info && info->alphaBits > 0;
}

} // namespace KWin
