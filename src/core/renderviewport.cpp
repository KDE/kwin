/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "core/renderviewport.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"

namespace KWin
{

static QMatrix4x4 createProjectionMatrix(const RenderTarget &renderTarget, const QRect &rect, const QPoint &renderOffset)
{
    QMatrix4x4 ret;

    ret.scale(1, -1); // flip the y axis back
    ret *= renderTarget.transform().toMatrix();
    // TODO change the "offset" to a device-local viewport rect?
    ret.scale((renderTarget.transformedSize().width() - 2 * renderOffset.x()) / double(renderTarget.transformedSize().width()),
              (renderTarget.transformedSize().height() - 2 * renderOffset.y()) / double(renderTarget.transformedSize().height()));
    ret.scale(1, -1); // undo ortho() flipping the y axis

    ret.ortho(rect);
    return ret;
}

RenderViewport::RenderViewport(const RectF &renderRect, double scale, const RenderTarget &renderTarget, const QPoint &renderOffset)
    : m_transform(renderTarget.transform())
    , m_transformBounds(m_transform.map(renderTarget.size()))
    , m_renderRect(renderRect)
    , m_scaledRenderRect(renderRect.scaled(scale).rounded())
    , m_renderOffset(renderOffset)
    , m_projectionMatrix(createProjectionMatrix(renderTarget, m_scaledRenderRect, renderOffset))
    , m_scale(scale)
{
}

QMatrix4x4 RenderViewport::projectionMatrix() const
{
    return m_projectionMatrix;
}

RectF RenderViewport::renderRect() const
{
    return m_renderRect;
}

Rect RenderViewport::scaledRenderRect() const
{
    return m_scaledRenderRect;
}

double RenderViewport::scale() const
{
    return m_scale;
}

OutputTransform RenderViewport::transform() const
{
    return m_transform;
}

QPoint RenderViewport::renderOffset() const
{
    return m_renderOffset;
}

Rect RenderViewport::deviceRect() const
{
    return Rect(m_renderOffset, deviceSize());
}

QSize RenderViewport::deviceSize() const
{
    return m_scaledRenderRect.size();
}

RectF RenderViewport::mapToDeviceCoordinates(const RectF &logicalGeometry) const
{
    return logicalGeometry.translated(-m_renderRect.topLeft()).scaled(m_scale).translated(m_renderOffset);
}

Rect RenderViewport::mapToDeviceCoordinatesAligned(const RectF &logicalGeometry) const
{
    return mapToDeviceCoordinates(logicalGeometry).roundedOut();
}

Rect RenderViewport::mapToDeviceCoordinatesAligned(const Rect &logicalGeometry) const
{
    return RectF(logicalGeometry).translated(-m_renderRect.topLeft()).scaled(m_scale).roundedOut().translated(m_renderOffset);
}

Region RenderViewport::mapToDeviceCoordinatesAligned(const Region &logicalGeometry) const
{
    Region ret;
    for (const Rect &logicalRect : logicalGeometry.rects()) {
        ret |= mapToDeviceCoordinatesAligned(logicalRect);
    }
    return ret;
}

RectF RenderViewport::mapFromDeviceCoordinates(const RectF &deviceGeometry) const
{
    return deviceGeometry.translated(-m_renderOffset).scaled(1.0 / m_scale).translated(m_renderRect.topLeft());
}

Rect RenderViewport::mapFromDeviceCoordinatesAligned(const Rect &deviceGeometry) const
{
    return deviceGeometry.translated(-m_renderOffset).scaled(1.0 / m_scale).translated(m_renderRect.topLeft()).toAlignedRect();
}

Rect RenderViewport::mapFromDeviceCoordinatesContained(const Rect &deviceGeometry) const
{
    return deviceGeometry
        .translated(-m_renderOffset)
        .scaled(1.0 / m_scale)
        .translated(m_renderRect.topLeft())
        .roundedIn();
}

Region RenderViewport::mapFromDeviceCoordinatesAligned(const Region &deviceGeometry) const
{
    Region ret;
    for (const Rect &deviceRect : deviceGeometry.rects()) {
        ret |= mapFromDeviceCoordinatesAligned(deviceRect);
    }
    return ret;
}

Region RenderViewport::mapFromDeviceCoordinatesContained(const Region &deviceGeometry) const
{
    Region ret;
    for (const Rect &deviceRect : deviceGeometry.rects()) {
        ret |= mapFromDeviceCoordinatesContained(deviceRect);
    }
    return ret;
}

RectF RenderViewport::mapToRenderTarget(const RectF &logicalGeometry) const
{
    const RectF deviceGeometry = logicalGeometry
                                     .scaled(m_scale)
                                     .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
    return m_transform.map(deviceGeometry, m_transformBounds);
}

Rect RenderViewport::mapToRenderTarget(const Rect &logicalGeometry) const
{
    const Rect deviceGeometry = logicalGeometry
                                    .scaled(m_scale)
                                    .rounded()
                                    .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
    return m_transform.map(deviceGeometry, m_transformBounds);
}

QPoint RenderViewport::mapToRenderTarget(const QPoint &logicalGeometry) const
{
    const QPoint devicePoint = snapToPixelGrid(QPointF(logicalGeometry) * m_scale) - m_scaledRenderRect.topLeft() + m_renderOffset;
    return m_transform.map(devicePoint, m_transformBounds);
}

QPointF RenderViewport::mapToRenderTarget(const QPointF &logicalGeometry) const
{
    const QPointF devicePoint = logicalGeometry * m_scale - m_scaledRenderRect.topLeft() + m_renderOffset;
    return m_transform.map(devicePoint, m_transformBounds);
}

Region RenderViewport::mapToRenderTarget(const Region &logicalGeometry) const
{
    Region ret;
    for (const auto &rect : logicalGeometry.rects()) {
        ret += mapToRenderTarget(rect);
    }
    return ret;
}

RectF RenderViewport::mapToRenderTargetTexture(const RectF &logicalGeometry) const
{
    return logicalGeometry
        .scaled(m_scale)
        .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
}

Rect RenderViewport::mapToRenderTargetTexture(const Rect &logicalGeometry) const
{
    return logicalGeometry
        .scaled(m_scale)
        .rounded()
        .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
}

QPoint RenderViewport::mapToRenderTargetTexture(const QPoint &logicalGeometry) const
{
    return snapToPixelGrid(QPointF(logicalGeometry) * m_scale) - m_scaledRenderRect.topLeft() + m_renderOffset;
}

QPointF RenderViewport::mapToRenderTargetTexture(const QPointF &logicalGeometry) const
{
    return logicalGeometry * m_scale - m_scaledRenderRect.topLeft() + m_renderOffset;
}

Region RenderViewport::mapToRenderTargetTexture(const Region &logicalGeometry) const
{
    Region ret;
    for (const auto &rect : logicalGeometry.rects()) {
        ret += mapToRenderTargetTexture(rect);
    }
    return ret;
}
}
