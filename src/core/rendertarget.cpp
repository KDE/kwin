/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription)
    : m_framebuffer(fbo)
    , m_transformation(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransformMatrix() : QMatrix4x4())
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(QImage *image, const ColorDescription &colorDescription)
    : m_image(image)
    , m_colorDescription(colorDescription)
{
}

QSize RenderTarget::size() const
{
    if (m_framebuffer) {
        return m_framebuffer->size();
    } else if (m_image) {
        return m_image->size();
    } else {
        Q_UNREACHABLE();
    }
}

QRectF RenderTarget::applyTransformation(const QRectF &rect) const
{
    const auto bounds = size();
    QMatrix4x4 relativeTransformation;
    relativeTransformation.translate(bounds.width() / 2.0, bounds.height() / 2.0);
    relativeTransformation *= m_transformation;
    relativeTransformation.translate(-bounds.width() / 2.0, -bounds.height() / 2.0);
    return relativeTransformation.mapRect(rect);
}

QRect RenderTarget::applyTransformation(const QRect &rect) const
{
    return applyTransformation(QRectF(rect)).toRect();
}

QPointF RenderTarget::applyTransformation(const QPointF &point) const
{
    const auto bounds = size();
    QMatrix4x4 relativeTransformation;
    relativeTransformation.translate(bounds.width() / 2.0, bounds.height() / 2.0);
    relativeTransformation *= m_transformation;
    relativeTransformation.translate(-bounds.width() / 2.0, -bounds.height() / 2.0);
    return relativeTransformation.map(point);
}

QPoint RenderTarget::applyTransformation(const QPoint &point) const
{
    const auto bounds = size();
    QMatrix4x4 relativeTransformation;
    relativeTransformation.translate(bounds.width() / 2.0, bounds.height() / 2.0);
    relativeTransformation *= m_transformation;
    relativeTransformation.translate(-bounds.width() / 2.0, -bounds.height() / 2.0);
    return relativeTransformation.map(point);
}

QMatrix4x4 RenderTarget::transformation() const
{
    return m_transformation;
}

GLFramebuffer *RenderTarget::framebuffer() const
{
    return m_framebuffer;
}

GLTexture *RenderTarget::texture() const
{
    return m_framebuffer->colorAttachment();
}

QImage *RenderTarget::image() const
{
    return m_image;
}

const ColorDescription &RenderTarget::colorDescription() const
{
    return m_colorDescription;
}

} // namespace KWin
