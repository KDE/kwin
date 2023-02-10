/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "viewport.h"

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

ViewPort::ViewPort(const QRectF &renderRect, double scale)
    : m_renderRect(renderRect)
    , m_projectionMatrix(createProjectionMatrix(renderRect, scale))
    , m_logicalToLocal(createLogicalToLocalMatrix(renderRect, scale))
    , m_scale(scale)
{
}

QMatrix4x4 ViewPort::projectionMatrix() const
{
    return m_projectionMatrix;
}

QRectF ViewPort::renderRect() const
{
    return m_renderRect;
}

double ViewPort::scale() const
{
    return m_scale;
}

QRectF ViewPort::mapToRenderTarget(const QRectF &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QRect ViewPort::mapToRenderTarget(const QRect &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QRegion ViewPort::mapToRenderTarget(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret |= m_logicalToLocal.mapRect(rect);
    }
    return ret;
}

}
