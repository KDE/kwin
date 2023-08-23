/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <memory>

namespace KWin
{
class GLFramebuffer;
class GLTexture;
class GraphicsBuffer;

class ScreenCastDmaBufTexture
{
public:
    explicit ScreenCastDmaBufTexture(std::shared_ptr<GLTexture> texture, GraphicsBuffer *buffer);
    virtual ~ScreenCastDmaBufTexture();

    GraphicsBuffer *buffer() const;
    GLTexture *texture() const;
    GLFramebuffer *framebuffer() const;

protected:
    std::shared_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_framebuffer;
    GraphicsBuffer *m_buffer;
};

}
