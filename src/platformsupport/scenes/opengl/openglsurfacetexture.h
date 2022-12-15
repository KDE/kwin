/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

namespace KWin
{

class GLTexture;
class OpenGLBackend;

class KWIN_EXPORT OpenGLSurfaceTexture : public SurfaceTexture
{
public:
    explicit OpenGLSurfaceTexture(OpenGLBackend *backend);
    ~OpenGLSurfaceTexture() override;

    bool isValid() const override;

    OpenGLBackend *backend() const;
    GLTexture *texture() const;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;

protected:
    OpenGLBackend *m_backend;
    std::unique_ptr<GLTexture> m_texture;
};

} // namespace KWin
