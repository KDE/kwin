/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "core/renderviewport.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "effect/globals.h"

namespace KWin
{

static QMatrix4x4 createProjectionMatrix(const RenderTarget &renderTarget, const QRect &rect)
{
    QMatrix4x4 ret;

    ret.scale(1, -1); // flip the y axis back
    ret *= renderTarget.transform().toMatrix();
    ret.scale(1, -1); // undo ortho() flipping the y axis

    ret.ortho(rect);
    return ret;
}

RenderViewport::RenderViewport(const QRectF &renderRect, double scale, const RenderTarget &renderTarget)
    : m_transform(renderTarget.transform())
    , m_transformBounds(m_transform.map(renderTarget.size()))
    , m_renderRect(renderRect)
    , m_deviceRenderRect(snapToPixelGrid(scaledRect(renderRect, scale)))
    , m_projectionMatrix(createProjectionMatrix(renderTarget, m_deviceRenderRect))
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
    const QRectF deviceGeometry = scaledRect(logicalGeometry, m_scale)
                                      .translated(-m_deviceRenderRect.topLeft());
    return m_transform.map(deviceGeometry, m_transformBounds);
}

QRect RenderViewport::mapToRenderTarget(const QRect &logicalGeometry) const
{
    const QRect deviceGeometry = snapToPixelGrid(scaledRect(logicalGeometry, m_scale))
                                     .translated(-m_deviceRenderRect.topLeft());
    return m_transform.map(deviceGeometry, m_transformBounds);
}

QPoint RenderViewport::mapToRenderTarget(const QPoint &logicalGeometry) const
{
    const QPoint devicePoint = snapToPixelGrid(QPointF(logicalGeometry) * m_scale) - m_deviceRenderRect.topLeft();
    return m_transform.map(devicePoint, m_transformBounds);
}

QPointF RenderViewport::mapToRenderTarget(const QPointF &logicalGeometry) const
{
    const QPointF devicePoint = logicalGeometry * m_scale - m_deviceRenderRect.topLeft();
    return m_transform.map(devicePoint, m_transformBounds);
}

QRegion RenderViewport::mapToRenderTarget(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret += mapToRenderTarget(rect);
    }
    return ret;
}

QRectF RenderViewport::mapToRenderTargetTexture(const QRectF &logicalGeometry) const
{
    return scaledRect(logicalGeometry, m_scale)
        .translated(-m_deviceRenderRect.topLeft());
}

QRect RenderViewport::mapToRenderTargetTexture(const QRect &logicalGeometry) const
{
    return snapToPixelGrid(scaledRect(logicalGeometry, m_scale))
        .translated(-m_deviceRenderRect.topLeft());
}

QPoint RenderViewport::mapToRenderTargetTexture(const QPoint &logicalGeometry) const
{
    return snapToPixelGrid(QPointF(logicalGeometry) * m_scale) - m_deviceRenderRect.topLeft();
}

QPointF RenderViewport::mapToRenderTargetTexture(const QPointF &logicalGeometry) const
{
    return logicalGeometry * m_scale - m_deviceRenderRect.topLeft();
}

QRegion RenderViewport::mapToRenderTargetTexture(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret += mapToRenderTargetTexture(rect);
    }
    return ret;
}
}
