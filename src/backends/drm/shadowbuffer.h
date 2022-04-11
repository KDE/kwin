/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QSize>
#include <kwinglutils.h>

#include "drm_object_plane.h"
#include "egl_gbm_backend.h"

namespace KWin
{

class ShadowBuffer
{
public:
    ShadowBuffer(const QSize &size, const GbmFormat &format);
    ~ShadowBuffer();

    bool isComplete() const;
    void render(DrmPlane::Transformations transform);

    GLFramebuffer *fbo() const;
    QSharedPointer<GLTexture> texture() const;
    uint32_t drmFormat() const;

private:
    GLint internalFormat(const GbmFormat &format) const;
    QSharedPointer<GLTexture> m_texture;
    QScopedPointer<GLFramebuffer> m_fbo;
    QScopedPointer<GLVertexBuffer> m_vbo;
    const QSize m_size;
    const uint32_t m_drmFormat;
};

}
