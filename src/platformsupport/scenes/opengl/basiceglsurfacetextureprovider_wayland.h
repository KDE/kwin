/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

#include <epoxy/egl.h>

namespace KWaylandServer
{
class BufferInterface;
}

namespace KWin
{

class AbstractEglBackend;

class KWIN_EXPORT BasicEGLSurfaceTextureProviderWayland : public OpenGLSurfaceTextureProvider
{
    Q_OBJECT

public:
    BasicEGLSurfaceTextureProviderWayland(OpenGLBackend *backend, SurfacePixmapWayland *pixmap);
    ~BasicEGLSurfaceTextureProviderWayland() override;

    AbstractEglBackend *backend() const;

    bool create() override;
    void update(const QRegion &region) override;

protected:
    SurfacePixmapWayland *m_pixmap;

private:
    bool loadShmTexture(KWaylandServer::BufferInterface *buffer);
    void updateShmTexture(KWaylandServer::BufferInterface *buffer, const QRegion &region);
    bool loadEglTexture(KWaylandServer::BufferInterface *buffer);
    void updateEglTexture(KWaylandServer::BufferInterface *buffer);
    bool loadDmabufTexture(KWaylandServer::BufferInterface *buffer);
    void updateDmabufTexture(KWaylandServer::BufferInterface *buffer);
    EGLImageKHR attach(KWaylandServer::BufferInterface *buffer);
    void destroy();

    enum class BufferType {
        None,
        Shm,
        DmaBuf,
        Egl,
    };

    EGLImageKHR m_image = EGL_NO_IMAGE_KHR;
    BufferType m_bufferType = BufferType::None;
};

} // namespace KWin
