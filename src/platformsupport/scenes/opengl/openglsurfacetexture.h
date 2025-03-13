/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/surfaceitem.h"

namespace KWin
{

class AbstractEglBackend;
class GLTexture;

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
    explicit OpenGLSurfaceTexture(AbstractEglBackend *backend, SurfacePixmap *pixmap);
    ~OpenGLSurfaceTexture() override;

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;

    OpenGLSurfaceContents texture() const;

private:
    bool loadShmTexture(GraphicsBuffer *buffer);
    void updateShmTexture(GraphicsBuffer *buffer, const QRegion &region);
    bool loadDmabufTexture(GraphicsBuffer *buffer);
    void updateDmabufTexture(GraphicsBuffer *buffer);
    void destroy();

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
    };

    BufferType m_bufferType = BufferType::None;
    AbstractEglBackend *m_backend;
    SurfacePixmap *m_pixmap;
    OpenGLSurfaceContents m_texture;
};

} // namespace KWin
