/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "renderviewport.h"
#include "rendertarget.h"

namespace KWin
{

static QMatrix4x4 createProjectionMatrix(const QRectF &renderRect, double scale, const RenderTarget &renderTarget)
{
    QMatrix4x4 ret = renderTarget.transformation();
    ret.ortho(QRectF(renderRect.left() * scale, renderRect.top() * scale, renderRect.width() * scale, renderRect.height() * scale));
    return ret;
}

static QMatrix4x4 createLogicalToLocalMatrix(const QRectF &renderRect, const RenderTarget &renderTarget)
{
    // map the normalized device coordinates [-1, 1] from the projection matrix (without scaling) to pixels
    QMatrix4x4 ret;
    ret.scale(renderTarget.size().width(), renderTarget.size().height());
    ret.translate(0.5, 0.5);
    ret.scale(0.5, -0.5);
    ret *= renderTarget.transformation();
    ret.ortho(renderRect);
    return ret;
}

static QMatrix4x4 createLogicalToLocalTextureMatrix(const QRectF &renderRect, double scale)
{
    // map the normalized device coordinates [-1, 1] from the projection matrix (without scaling) to pixels
    QMatrix4x4 ret;
    ret.scale(renderRect.width() * scale, renderRect.height() * scale);
    ret.translate(0.5, 0.5);
    ret.scale(0.5, -0.5);
    ret.ortho(renderRect);
    return ret;
}

RenderViewport::RenderViewport(const QRectF &renderRect, double scale, const RenderTarget &renderTarget)
    : m_renderRect(renderRect)
    , m_projectionMatrix(createProjectionMatrix(renderRect, scale, renderTarget))
    , m_logicalToLocal(createLogicalToLocalMatrix(renderRect, renderTarget))
    , m_logicalToLocalTexture(createLogicalToLocalTextureMatrix(renderRect, scale))
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

QRectF RenderViewport::mapToRenderTargetTexture(const QRectF &logicalGeometry) const
{
    return m_logicalToLocalTexture.mapRect(logicalGeometry);
}

QRect RenderViewport::mapToRenderTargetTexture(const QRect &logicalGeometry) const
{
    return m_logicalToLocalTexture.mapRect(logicalGeometry);
}

QPoint RenderViewport::mapToRenderTargetTexture(const QPoint &logicalGeometry) const
{
    return m_logicalToLocalTexture.map(logicalGeometry);
}

QPointF RenderViewport::mapToRenderTargetTexture(const QPointF &logicalGeometry) const
{
    return m_logicalToLocalTexture.map(logicalGeometry);
}

QRegion RenderViewport::mapToRenderTargetTexture(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret |= m_logicalToLocalTexture.mapRect(rect);
    }
    return ret;
}
}
