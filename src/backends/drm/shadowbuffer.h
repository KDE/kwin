/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QSize>
#include <kwinglutils.h>

#include "egl_gbm_backend.h"

namespace KWin
{

class DrmAbstractOutput;

class ShadowBuffer
{
public:
    ShadowBuffer(const QSize &size, const GbmFormat &format);
    ~ShadowBuffer();

    bool isComplete() const;

    void bind();
    void render(DrmAbstractOutput *output);

    int texture() const;

    QSize textureSize() const;

private:
    GLint internalFormat(const GbmFormat &format);
    GLuint m_texture;
    GLuint m_framebuffer;
    QScopedPointer<GLVertexBuffer> m_vbo;
    QSize m_size;
};

}
