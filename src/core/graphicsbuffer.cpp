/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/graphicsbuffer.h"
#include "core/drm_formats.h"

#include <QCoreApplication>

#include <drm_fourcc.h>

namespace KWin
{

GraphicsBuffer::GraphicsBuffer()
{
}

GraphicsBuffer::~GraphicsBuffer()
{
}

bool GraphicsBuffer::isReferenced() const
{
    return m_references > 0;
}

void GraphicsBuffer::ref()
{
    const int r = ++m_references;
    if (r == 1) {
        std::unique_lock lock(m_mutex);
        // two threads could race to increment and decrement
        // m_references at the same time, so we need to check again
        if (m_references == 1) {
            referenced();
        }
    }
}

void GraphicsBuffer::unref()
{
    const int r = --m_references;
    if (r == 0) {
        std::unique_lock lock(m_mutex);
        // two threads could race to increment and decrement
        // m_references at the same time, so we need to check again
        if (m_references == 0) {
            m_releasePoints.clear();
            released();
        }
    }
}

GraphicsBuffer::Map GraphicsBuffer::map(MapFlags flags)
{
    return {};
}

void GraphicsBuffer::unmap()
{
}

const DmaBufAttributes *GraphicsBuffer::udmabufAttributes() const
{
    return nullptr;
}

const DmaBufAttributes *GraphicsBuffer::dmabufAttributes() const
{
    return nullptr;
}

const ShmAttributes *GraphicsBuffer::shmAttributes() const
{
    return nullptr;
}

const SinglePixelAttributes *GraphicsBuffer::singlePixelAttributes() const
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

void GraphicsBuffer::referenced()
{
}

void GraphicsBuffer::released()
{
}

} // namespace KWin

#include "moc_graphicsbuffer.cpp"
