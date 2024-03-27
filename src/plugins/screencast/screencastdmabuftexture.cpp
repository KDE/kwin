/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "plugins/screencast/screencastdmabuftexture.h"
#include "core/graphicsbuffer.h"
#include "opengl/glutils.h"

namespace KWin
{

ScreenCastDmaBufTexture::ScreenCastDmaBufTexture(std::shared_ptr<GLTexture> texture, std::unique_ptr<GLFramebuffer> &&framebuffer, GraphicsBuffer *buffer)
    : m_texture(std::move(texture))
    , m_framebuffer(std::move(framebuffer))
    , m_buffer(buffer)
{
}
ScreenCastDmaBufTexture::~ScreenCastDmaBufTexture()
{
    m_framebuffer.reset();
    m_texture.reset();
    m_buffer->drop();
}

GraphicsBuffer *ScreenCastDmaBufTexture::buffer() const
{
    return m_buffer;
}

KWin::GLTexture *ScreenCastDmaBufTexture::texture() const
{
    return m_texture.get();
}

KWin::GLFramebuffer *ScreenCastDmaBufTexture::framebuffer() const
{
    return m_framebuffer.get();
}

} // namespace KWin
