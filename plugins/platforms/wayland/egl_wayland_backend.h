/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_EGL_WAYLAND_BACKEND_H
#define KWIN_EGL_WAYLAND_BACKEND_H
#include "abstract_egl_backend.h"
#include "scene_opengl.h"
// wayland
#include <wayland-egl.h>

class QTemporaryFile;
struct wl_buffer;
struct wl_shm;

namespace KWin
{

namespace Wayland {
    class WaylandBackend;
}

/**
 * @brief OpenGL Backend using Egl on a Wayland surface.
 *
 * This Backend is the basis for a session compositor running on top of a Wayland system compositor.
 * It creates a Surface as large as the screen and maps it as a fullscreen shell surface on the
 * system compositor. The OpenGL context is created on the Wayland surface, so for rendering X11 is
 * not involved.
 *
 * Also in repainting the backend is currently still rather limited. Only supported mode is fullscreen
 * repaints, which is obviously not optimal. Best solution is probably to go for buffer_age extension
 * and make it the only available solution next to fullscreen repaints.
 **/
class EglWaylandBackend : public QObject, public AbstractEglBackend
{
    Q_OBJECT
public:
    EglWaylandBackend(Wayland::WaylandBackend *b);
    virtual ~EglWaylandBackend();
    void screenGeometryChanged(const QSize &size) Q_DECL_OVERRIDE;
    SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture) Q_DECL_OVERRIDE;
    QRegion prepareRenderingFrame() Q_DECL_OVERRIDE;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) Q_DECL_OVERRIDE;
    virtual bool usesOverlayWindow() const override;
    void init() override;

protected:
    void present() Q_DECL_OVERRIDE;

private Q_SLOTS:
    void overlaySizeChanged(const QSize &size);

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    bool makeContextCurrent();
    int m_bufferAge;
    Wayland::WaylandBackend *m_wayland;
    wl_egl_window *m_overlay;
    bool m_havePlatformBase;
    friend class EglWaylandTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglWaylandTexture : public AbstractEglTexture
{
public:
    virtual ~EglWaylandTexture();

private:
    friend class EglWaylandBackend;
    EglWaylandTexture(SceneOpenGL::Texture *texture, EglWaylandBackend *backend);
};

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H
