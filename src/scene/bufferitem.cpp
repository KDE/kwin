/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bufferitem.h"
#include "core/syncobjtimeline.h"
#include "scene/itemrenderer.h"
#include "scene/scene.h"
#include "scene/texture.h"

namespace KWin
{

BufferItem::BufferItem(Item *parent)
    : Item(parent)
{
}

BufferItem::~BufferItem()
{
}

Texture *BufferItem::texture() const
{
    return m_texture.get();
}

void BufferItem::setBuffer(GraphicsBuffer *buffer, const std::shared_ptr<SyncReleasePoint> &releasePoint)
{
    m_buffer = buffer;
    m_releasePoint = releasePoint;
    m_bufferDirty = true;
    discardQuads();
    scheduleRepaint(boundingRect());
}

bool BufferItem::hasAlphaChannel() const
{
    return m_buffer && m_buffer->hasAlphaChannel();
}

void BufferItem::preprocess()
{
    if (!m_buffer) {
        m_texture.reset();
        return;
    }
    if (!m_texture) {
        m_texture = scene()->renderer()->createTexture(m_buffer.buffer(), m_releasePoint);
        m_bufferDirty = false;
    }
    if (m_bufferDirty) {
        m_texture->attach(m_buffer.buffer(), Rect(QPoint(), m_buffer->size()), m_releasePoint);
        m_bufferDirty = false;
    }
}

WindowQuadList BufferItem::buildQuads() const
{
    const RectF geometry = boundingRect();
    if (geometry.isEmpty() || !m_buffer) {
        return WindowQuadList{};
    }

    const RectF bufferRect(QPoint(), m_buffer->size());

    WindowQuad quad;
    quad[0] = WindowVertex(geometry.topLeft(), bufferRect.topLeft());
    quad[1] = WindowVertex(geometry.topRight(), bufferRect.topRight());
    quad[2] = WindowVertex(geometry.bottomRight(), bufferRect.bottomRight());
    quad[3] = WindowVertex(geometry.bottomLeft(), bufferRect.bottomLeft());

    WindowQuadList ret;
    ret.append(quad);
    return ret;
}

void BufferItem::releaseResources()
{
    m_texture.reset();
}

}
