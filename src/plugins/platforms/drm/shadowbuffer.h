/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QSize>
#include <kwinglutils.h>

namespace KWin
{

class DrmAbstractOutput;

class ShadowBuffer
{
public:
    ShadowBuffer(const QSize &size);
    ~ShadowBuffer();

    bool isComplete() const;

    void bind();
    void unbind();
    enum class RenderMode {
        Normal,
        OpenGlToDrm,// flips the Y axis and changes color format to xrgb
    };
    void render(DrmAbstractOutput *output, RenderMode mode = RenderMode::Normal);

    int texture() const;

    QSize textureSize() const;

private:
    GLuint m_texture;
    GLuint m_framebuffer;
    QScopedPointer<GLVertexBuffer> m_vbo;
    QSize m_size;
};

}
