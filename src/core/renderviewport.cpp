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

RenderViewport::RenderViewport(const QRectF &renderRect, double scale, const RenderTarget &renderTarget, const QPoint &renderOffset)
    : m_transform(renderTarget.transform())
    , m_transformBounds(m_transform.map(renderTarget.size()))
    , m_renderRect(renderRect)
    , m_scaledRenderRect(snapToPixelGrid(scaledRect(renderRect, scale)))
    , m_renderOffset(renderOffset)
    , m_projectionMatrix(createProjectionMatrix(renderTarget, m_scaledRenderRect, renderOffset))
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

QRect RenderViewport::scaledRenderRect() const
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

QRect RenderViewport::deviceRect() const
{
    return QRect(m_renderOffset, deviceSize());
}

QSize RenderViewport::deviceSize() const
{
    return m_scaledRenderRect.size();
}

QRectF RenderViewport::mapToDeviceCoordinates(const QRectF &logicalGeometry) const
{
    return scaledRect(logicalGeometry.translated(-m_renderRect.topLeft()), m_scale).translated(m_renderOffset);
}

QRect RenderViewport::mapToDeviceCoordinatesAligned(const QRectF &logicalGeometry) const
{
    return mapToDeviceCoordinates(logicalGeometry).toAlignedRect();
}

QRect RenderViewport::mapToDeviceCoordinatesAligned(const QRect &logicalGeometry) const
{
    return scaledRect(QRectF(logicalGeometry).translated(-m_renderRect.topLeft()), m_scale).toAlignedRect().translated(m_renderOffset);
}

QRegion RenderViewport::mapToDeviceCoordinatesAligned(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const QRect &logicalRect : logicalGeometry) {
        ret |= mapToDeviceCoordinatesAligned(logicalRect);
    }
    return ret;
}

QRectF RenderViewport::mapFromDeviceCoordinates(const QRectF &deviceGeometry) const
{
    return scaledRect(deviceGeometry.translated(-m_renderOffset), 1.0 / m_scale).translated(m_renderRect.topLeft());
}

QRect RenderViewport::mapFromDeviceCoordinatesAligned(const QRect &deviceGeometry) const
{
    return scaledRect(deviceGeometry.translated(-m_renderOffset), 1.0 / m_scale).translated(m_renderRect.topLeft()).toAlignedRect();
}

QRect RenderViewport::mapFromDeviceCoordinatesContained(const QRect &deviceGeometry) const
{
    const QRectF ret = scaledRect(deviceGeometry.translated(-m_renderOffset), 1.0 / m_scale).translated(m_renderRect.topLeft());
    return QRect(QPoint(std::ceil(ret.x()), std::ceil(ret.y())),
                 QPoint(std::floor(ret.x() + ret.width()) - 1, std::floor(ret.y() + ret.height()) - 1));
}

QRegion RenderViewport::mapFromDeviceCoordinatesAligned(const QRegion &deviceGeometry) const
{
    QRegion ret;
    for (const QRect &deviceRect : deviceGeometry) {
        ret |= mapFromDeviceCoordinatesAligned(deviceRect);
    }
    return ret;
}

QRegion RenderViewport::mapFromDeviceCoordinatesContained(const QRegion &deviceGeometry) const
{
    QRegion ret;
    for (const QRect &deviceRect : deviceGeometry) {
        ret |= mapFromDeviceCoordinatesContained(deviceRect);
    }
    return ret;
}

QRectF RenderViewport::mapToRenderTarget(const QRectF &logicalGeometry) const
{
    const QRectF deviceGeometry = scaledRect(logicalGeometry, m_scale)
                                      .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
    return m_transform.map(deviceGeometry, m_transformBounds);
}

QRect RenderViewport::mapToRenderTarget(const QRect &logicalGeometry) const
{
    const QRect deviceGeometry = snapToPixelGrid(scaledRect(logicalGeometry, m_scale))
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
        .translated(-m_scaledRenderRect.topLeft() + m_renderOffset);
}

QRect RenderViewport::mapToRenderTargetTexture(const QRect &logicalGeometry) const
{
    return snapToPixelGrid(scaledRect(logicalGeometry, m_scale))
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

QRegion RenderViewport::mapToRenderTargetTexture(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret += mapToRenderTargetTexture(rect);
    }
    return ret;
}
}
