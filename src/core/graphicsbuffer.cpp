/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbuffer.h"
#include "utils/drm_format_helper.h"

#include <QCoreApplication>

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
    Q_ASSERT(QCoreApplication::instance()->thread() == thread());
    ++m_refCount;
}

void GraphicsBuffer::unref()
{
    Q_ASSERT(QCoreApplication::instance()->thread() == thread());
    Q_ASSERT(m_refCount > 0);
    --m_refCount;
    if (!m_refCount) {
        if (m_dropped) {
            delete this;
        } else {
            m_releasePoints.clear();
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

GraphicsBuffer::Map GraphicsBuffer::map(MapFlags flags)
{
    return {};
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

void GraphicsBuffer::addReleasePoint(const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    m_releasePoints.push_back(releasePoint);
}

bool GraphicsBuffer::alphaChannelFromDrmFormat(uint32_t format)
{
    const auto info = FormatInfo::get(format);
    return info && info->alphaBits > 0;
}

} // namespace KWin

#include "moc_graphicsbuffer.cpp"
