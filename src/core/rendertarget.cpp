/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription)
    : m_size(fbo->size())
    , m_framebuffer(fbo)
    , m_transform(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransform() : OutputTransform())
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(QImage *image, const ColorDescription &colorDescription)
    : m_size(image->size())
    , m_image(image)
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(vk::CommandBuffer cmd, vk::ImageView view, const QSize &framebufferSize, const ColorDescription &colorDescription)
    : m_size(framebufferSize)
    , m_commandBuffer(cmd)
    , m_imageView(view)
    , m_colorDescription(colorDescription)
{
}

QSize RenderTarget::size() const
{
    return m_size;
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

vk::CommandBuffer RenderTarget::commandBuffer() const
{
    return m_commandBuffer;
}

vk::ImageView RenderTarget::imageView() const
{
    return m_imageView;
}

const ColorDescription &RenderTarget::colorDescription() const
{
    return m_colorDescription;
}

} // namespace KWin
