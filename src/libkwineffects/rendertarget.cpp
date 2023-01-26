/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "rendertarget.h"
#include "kwinglutils.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo)
    : m_framebuffer(fbo)
    , m_transformation(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransformMatrix() : QMatrix4x4())
{
}

RenderTarget::RenderTarget(QImage *image)
    : m_image(image)
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

QRectF RenderTarget::applyTransformation(const QRectF &rect, const QRectF &viewport) const
{
    const auto center = viewport.center();
    QMatrix4x4 relativeTransformation;
    relativeTransformation.translate(center.x(), center.y());
    relativeTransformation *= m_transformation;
    relativeTransformation.translate(-center.x(), -center.y());
    return relativeTransformation.mapRect(rect);
}

QRect RenderTarget::applyTransformation(const QRect &rect, const QRect &viewport) const
{
    return applyTransformation(QRectF(rect), QRectF(viewport)).toRect();
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

} // namespace KWin
