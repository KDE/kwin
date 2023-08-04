/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "openglsurfacetexture_wayland.h"

namespace KWin
{

class AbstractEglBackend;
class GraphicsBuffer;

class KWIN_EXPORT BasicEGLSurfaceTextureWayland : public OpenGLSurfaceTextureWayland
{
public:
    BasicEGLSurfaceTextureWayland(OpenGLBackend *backend, SurfacePixmap *pixmap);
    ~BasicEGLSurfaceTextureWayland() override;

    AbstractEglBackend *backend() const;

    bool create() override;
    void update(const QRegion &region) override;

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
};

} // namespace KWin
