/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

namespace KWin
{

class GLShader;
class GLTexture;
class OpenGLBackend;

class KWIN_EXPORT OpenGLSurfaceContents
{
public:
    OpenGLSurfaceContents()
    {
    }
    OpenGLSurfaceContents(const std::shared_ptr<GLTexture> &contents)
        : planes({contents})
    {
    }
    OpenGLSurfaceContents(const QList<std::shared_ptr<GLTexture>> &planes)
        : planes(planes)
    {
    }

    void setDirty();
    void reset()
    {
        planes.clear();
    }
    bool isValid() const
    {
        return !planes.isEmpty();
    }

    QList<std::shared_ptr<GLTexture>> planes;
};

class KWIN_EXPORT OpenGLSurfaceTexture : public SurfaceTexture
{
public:
    explicit OpenGLSurfaceTexture(OpenGLBackend *backend);
    ~OpenGLSurfaceTexture() override;

    bool isValid() const override;

    OpenGLBackend *backend() const;
    OpenGLSurfaceContents texture() const;

    virtual bool create() = 0;
    virtual void update(const QRegion &region) = 0;

protected:
    OpenGLBackend *m_backend;
    OpenGLSurfaceContents m_texture;
};

} // namespace KWin
