/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer.h"

#include <QRegion>

namespace KWin
{

ItemRenderer::ItemRenderer()
{
}

ItemRenderer::~ItemRenderer()
{
}

QPainter *ItemRenderer::painter() const
{
    return nullptr;
}

void ItemRenderer::beginFrame(RenderTarget *renderTarget)
{
}

void ItemRenderer::endFrame()
{
}

QMatrix4x4 ItemRenderer::renderTargetProjectionMatrix() const
{
    return m_renderTargetProjectionMatrix;
}

QRect ItemRenderer::renderTargetRect() const
{
    return m_renderTargetRect.toRect();
}

static QMatrix4x4 createProjectionMatrix(const QRectF &rect, qreal scale)
{
    QMatrix4x4 ret;
    ret.ortho(QRectF(rect.left() * scale, rect.top() * scale, rect.width() * scale, rect.height() * scale));
    return ret;
}

void ItemRenderer::setRenderTargetRect(const QRectF &rect)
{
    if (rect == m_renderTargetRect) {
        return;
    }

    m_renderTargetRect = rect;
    m_renderTargetProjectionMatrix = createProjectionMatrix(rect, m_renderTargetScale);
}

qreal ItemRenderer::renderTargetScale() const
{
    return m_renderTargetScale;
}

void ItemRenderer::setRenderTargetScale(qreal scale)
{
    if (qFuzzyCompare(scale, m_renderTargetScale)) {
        return;
    }

    m_renderTargetScale = scale;
    m_renderTargetProjectionMatrix = createProjectionMatrix(m_renderTargetRect, scale);
}

QRegion ItemRenderer::mapToRenderTarget(const QRegion &region) const
{
    QRegion result;
    for (const QRect &rect : region) {
        result += QRect((rect.x() - m_renderTargetRect.x()) * m_renderTargetScale,
                        (rect.y() - m_renderTargetRect.y()) * m_renderTargetScale,
                        rect.width() * m_renderTargetScale,
                        rect.height() * m_renderTargetScale);
    }
    return result;
}

} // namespace KWin
