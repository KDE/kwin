/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "core/region.h"

#include <QMatrix4x4>

namespace KWin
{

class RenderTarget;

class KWIN_EXPORT RenderViewport
{
public:
    explicit RenderViewport(const RectF &renderRect, double scale, const RenderTarget &renderTarget, const QPoint &renderOffset);

    QMatrix4x4 projectionMatrix() const;
    RectF renderRect() const;
    Rect scaledRenderRect() const;
    double scale() const;
    OutputTransform transform() const;
    QPoint renderOffset() const;

    /**
     * @returns Rect(renderOffset(), deviceSize())
     */
    Rect deviceRect() const;
    QSize deviceSize() const;

    RectF mapToDeviceCoordinates(const RectF &logicalGeometry) const;
    Rect mapToDeviceCoordinatesAligned(const RectF &logicalGeometry) const;
    Rect mapToDeviceCoordinatesAligned(const Rect &logicalGeometry) const;
    Region mapToDeviceCoordinatesAligned(const Region &logicalGeometry) const;

    RectF mapFromDeviceCoordinates(const RectF &deviceGeometry) const;
    Rect mapFromDeviceCoordinatesAligned(const Rect &deviceGeometry) const;
    Rect mapFromDeviceCoordinatesContained(const Rect &deviceGeometry) const;
    Region mapFromDeviceCoordinatesAligned(const Region &deviceGeometry) const;
    Region mapFromDeviceCoordinatesContained(const Region &deviceGeometry) const;

    RectF mapToRenderTarget(const RectF &logicalGeometry) const;
    Rect mapToRenderTarget(const Rect &logicalGeometry) const;
    QPoint mapToRenderTarget(const QPoint &logicalGeometry) const;
    QPointF mapToRenderTarget(const QPointF &logicalGeometry) const;
    Region mapToRenderTarget(const Region &logicalGeometry) const;

    RectF mapToRenderTargetTexture(const RectF &logicalGeometry) const;
    Rect mapToRenderTargetTexture(const Rect &logicalGeometry) const;
    QPoint mapToRenderTargetTexture(const QPoint &logicalGeometry) const;
    QPointF mapToRenderTargetTexture(const QPointF &logicalGeometry) const;
    Region mapToRenderTargetTexture(const Region &logicalGeometry) const;

private:
    const OutputTransform m_transform;
    const QSize m_transformBounds;
    const RectF m_renderRect;
    const Rect m_scaledRenderRect;
    const QPoint m_renderOffset;
    const QMatrix4x4 m_projectionMatrix;
    const double m_scale;
};

} // namespace KWin
