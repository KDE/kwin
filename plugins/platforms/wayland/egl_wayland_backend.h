/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2013 Martin Gräßlin <mgraesslin@kde.org>
Copyright 2019 Roman Gilg <subdiff@gmail.com>

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
// wayland
#include <wayland-egl.h>

class QTemporaryFile;
struct wl_buffer;
struct wl_shm;

namespace KWin
{

namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class EglWaylandBackend;

class EglWaylandOutput : public QObject
{
    Q_OBJECT
public:
    EglWaylandOutput(WaylandOutput *output, QObject *parent = nullptr);
    ~EglWaylandOutput() override = default;

    bool init(EglWaylandBackend *backend);
    void updateSize(const QSize &size);

private:
    WaylandOutput *m_waylandOutput;
    wl_egl_window *m_overlay = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    int m_bufferAge = 0;
    /**
    * @brief The damage history for the past 10 frames.
    */
    QVector<QRegion> m_damageHistory;

    friend class EglWaylandBackend;
};

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
 */
class EglWaylandBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglWaylandBackend(WaylandBackend *b);
    ~EglWaylandBackend() override;
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    QRegion prepareRenderingForScreen(int screenId) override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    void init() override;

    bool havePlatformBase() const {
        return m_havePlatformBase;
    }

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    bool createEglWaylandOutput(WaylandOutput *output);

    void cleanupSurfaces() override;
    void cleanupOutput(EglWaylandOutput *output);

    bool makeContextCurrent(EglWaylandOutput *output);
    void present() override;
    void presentOnSurface(EglWaylandOutput *output);

    WaylandBackend *m_backend;
    QVector<EglWaylandOutput*> m_outputs;
    bool m_havePlatformBase;
    bool m_swapping = false;
    friend class EglWaylandTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 */
class EglWaylandTexture : public AbstractEglTexture
{
public:
    ~EglWaylandTexture() override;

private:
    friend class EglWaylandBackend;
    EglWaylandTexture(SceneOpenGLTexture *texture, EglWaylandBackend *backend);
};

}
}

#endif
