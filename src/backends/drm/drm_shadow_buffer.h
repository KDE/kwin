/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QSize>
#include <kwinglutils.h>

#include "drm_egl_backend.h"
#include "drm_plane.h"

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
    std::shared_ptr<GLTexture> texture() const;
    uint32_t drmFormat() const;

private:
    GLint internalFormat(const GbmFormat &format) const;
    std::shared_ptr<GLTexture> m_texture;
    std::unique_ptr<GLFramebuffer> m_fbo;
    std::unique_ptr<GLVertexBuffer> m_vbo;
    const QSize m_size;
    const uint32_t m_drmFormat;
};

}
