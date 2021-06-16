/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetextureprovider.h"

#include <epoxy/egl.h>

namespace KWaylandServer
{
class DrmClientBuffer;
class ShmClientBuffer;
class LinuxDmaBufV1ClientBuffer;
}

namespace KWin
{

class AbstractEglBackend;

class KWIN_EXPORT BasicEGLSurfaceTextureProviderWayland : public OpenGLSurfaceTextureProvider
{
public:
    BasicEGLSurfaceTextureProviderWayland(OpenGLBackend *backend, SurfacePixmapWayland *pixmap);
    ~BasicEGLSurfaceTextureProviderWayland() override;

    AbstractEglBackend *backend() const;

    bool create() override;
    void update(const QRegion &region) override;

protected:
    SurfacePixmapWayland *m_pixmap;

private:
    bool loadShmTexture(KWaylandServer::ShmClientBuffer *buffer);
    void updateShmTexture(KWaylandServer::ShmClientBuffer *buffer, const QRegion &region);
    bool loadEglTexture(KWaylandServer::DrmClientBuffer *buffer);
    void updateEglTexture(KWaylandServer::DrmClientBuffer *buffer);
    bool loadDmabufTexture(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer);
    void updateDmabufTexture(KWaylandServer::LinuxDmaBufV1ClientBuffer *buffer);
    EGLImageKHR attach(KWaylandServer::DrmClientBuffer *buffer);
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
