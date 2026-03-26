/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"
#include "vulkan/vulkan_texture.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo, const std::shared_ptr<ColorDescription> &colorDescription)
    : m_framebuffer(fbo)
    , m_transform(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransform() : OutputTransform())
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(GraphicsBuffer *buffer,
                           OutputTransform transform,
                           const std::shared_ptr<ColorDescription> &colorDescription)
    : m_buffer(buffer)
    , m_transform(transform)
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(VulkanTexture *texture, const std::shared_ptr<ColorDescription> &colorDescription)
    : m_vulkanTexture(texture)
    // TODO we may need a transform in the texture for Vulkan, too?
    , m_transform(OutputTransform())
    , m_colorDescription(colorDescription)
{
}

QSize RenderTarget::transformedSize() const
{
    return m_transform.map(size());
}

Rect RenderTarget::transformedRect() const
{
    return Rect(QPoint(0, 0), transformedSize());
}

QSize RenderTarget::size() const
{
    if (m_buffer) {
        return m_buffer->size();
    } else if (m_vulkanTexture) {
        return m_vulkanTexture->size();
    } else {
        return m_framebuffer->size();
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

VulkanTexture *RenderTarget::vulkanTexture() const
{
    return m_vulkanTexture;
}

GraphicsBuffer *RenderTarget::buffer() const
{
    return m_buffer.get();
}

const std::shared_ptr<ColorDescription> &RenderTarget::colorDescription() const
{
    return m_colorDescription;
}

} // namespace KWin
