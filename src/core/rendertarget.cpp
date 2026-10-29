/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo, const std::shared_ptr<ColorDescription> &colorDescription)
    : m_framebuffer(fbo)
    , m_transform(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransform() : OutputTransform())
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
    return m_framebuffer->size();
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

const std::shared_ptr<ColorDescription> &RenderTarget::colorDescription() const
{
    return m_colorDescription;
}

} // namespace KWin
