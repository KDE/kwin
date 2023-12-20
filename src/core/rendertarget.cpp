/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"

namespace KWin
{

static OutputTransform textureTransformToOutputTransform(TextureTransforms transforms) // TODO: Kill TextureTransform?
{
    if (transforms == TextureTransforms()) {
        return OutputTransform::Normal;
    } else if (transforms == TextureTransform::Rotate90) {
        return OutputTransform::Rotate90;
    } else if (transforms == TextureTransform::Rotate180) {
        return OutputTransform::Rotate180;
    } else if (transforms == TextureTransform::Rotate270) {
        return OutputTransform::Rotate270;
    } else if (transforms == TextureTransform::MirrorX) {
        return OutputTransform::FlipX;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorX | TextureTransform::Rotate90)) {
        return OutputTransform::FlipX90;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorX | TextureTransform::Rotate180)) {
        return OutputTransform::FlipX180;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorX | TextureTransform::Rotate270)) {
        return OutputTransform::FlipX270;
    } else if (transforms == TextureTransform::MirrorY) {
        return OutputTransform::FlipY;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorY | TextureTransform::Rotate90)) {
        return OutputTransform::FlipY90;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorY | TextureTransform::Rotate180)) {
        return OutputTransform::FlipY180;
    } else if (transforms == TextureTransforms(TextureTransform::MirrorY | TextureTransform::Rotate270)) {
        return OutputTransform::FlipY270;
    }
    Q_UNREACHABLE();
}

RenderTarget::RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription)
    : m_framebuffer(fbo)
    , m_transform(fbo->colorAttachment() ? textureTransformToOutputTransform(fbo->colorAttachment()->contentTransforms()) : OutputTransform())
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

OutputTransform RenderTarget::transform() const
{
    return m_transform;
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
