/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rendertarget.h"
#include "kwinglutils.h"

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

RenderTarget::RenderTarget(GLFramebuffer *fbo, const QRectF &renderRect, double scale)
    : m_nativeHandle(fbo)
    , m_renderRect(renderRect)
    , m_projectionMatrix(createProjectionMatrix(renderRect, scale))
    , m_logicalToLocal(createLogicalToLocalMatrix(renderRect, scale))
    , m_scale(scale)
{
}

RenderTarget::RenderTarget(QImage *image, const QRectF &renderRect, double scale)
    : m_nativeHandle(image)
    , m_renderRect(renderRect)
    , m_projectionMatrix(createProjectionMatrix(renderRect, scale))
    , m_logicalToLocal(createLogicalToLocalMatrix(renderRect, scale))
    , m_scale(scale)
{
}

QSize RenderTarget::size() const
{
    if (auto fbo = std::get_if<GLFramebuffer *>(&m_nativeHandle)) {
        return (*fbo)->size();
    } else if (auto image = std::get_if<QImage *>(&m_nativeHandle)) {
        return (*image)->size();
    } else {
        Q_UNREACHABLE();
    }
}

RenderTarget::NativeHandle RenderTarget::nativeHandle() const
{
    return m_nativeHandle;
}

QMatrix4x4 RenderTarget::projectionMatrix() const
{
    return m_projectionMatrix;
}

QRectF RenderTarget::renderRect() const
{
    return m_renderRect;
}

double RenderTarget::scale() const
{
    return m_scale;
}

QRectF RenderTarget::mapToRenderTarget(const QRectF &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QRect RenderTarget::mapToRenderTarget(const QRect &logicalGeometry) const
{
    return m_logicalToLocal.mapRect(logicalGeometry);
}

QRegion RenderTarget::mapToRenderTarget(const QRegion &logicalGeometry) const
{
    QRegion ret;
    for (const auto &rect : logicalGeometry) {
        ret |= m_logicalToLocal.mapRect(rect);
    }
    return ret;
}

} // namespace KWin
