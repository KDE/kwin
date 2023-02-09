/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderviewport.h"

namespace KWin
{

static QMatrix4x4 createLogicalToLocalMatrix(const QRectF &renderRect, double scale)
{
    QMatrix4x4 ret;
    ret.translate(-renderRect.x(), -renderRect.y());
    ret.scale(scale, scale);
    return ret;
}

static QMatrix4x4 createProjectionMatrix(const QRectF &renderRect, double scale)
{
    QMatrix4x4 ret;
    ret.ortho(QRectF(renderRect.left() * scale, renderRect.top() * scale, renderRect.width() * scale, renderRect.height() * scale));
    return ret;
}

RenderViewport::RenderViewport(const QRectF &renderRect, double scale)
    : m_renderRect(renderRect)
    , m_projectionMatrix(createProjectionMatrix(renderRect, scale))
    , m_logicalToLocal(createLogicalToLocalMatrix(renderRect, scale))
    , m_scale(scale)
{
}

QMatrix4x4 RenderViewport::projectionMatrix() const
{
    return m_projectionMatrix;
}

QRectF RenderViewport::renderRect() const
{
    return m_renderRect;
}

double RenderViewport::scale() const
{
    return m_scale;
}

QRectF RenderViewport::mapToRenderTarget(const QRectF &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QRect RenderViewport::mapToRenderTarget(const QRect &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QPoint RenderViewport::mapToRenderTarget(const QPoint &logicalGeometry) const
{
    return m_logicalToLocal.map(logicalGeometry);
}

QPointF RenderViewport::mapToRenderTarget(const QPointF &logicalGeometry) const
{
    return m_logicalToLocal.map(logicalGeometry);
}

QRegion RenderViewport::mapToRenderTarget(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret |= m_logicalToLocal.mapRect(rect);
    }
    return ret;
}

}
