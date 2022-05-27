/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_WAYLAND_BACKEND_H
#define KWIN_EGL_WAYLAND_BACKEND_H
#include "abstract_egl_backend.h"
#include "outputlayer.h"
#include "utils/damagejournal.h"
// wayland
#include <wayland-egl.h>

class QTemporaryFile;
struct wl_buffer;
struct wl_shm;

namespace KWin
{
class GLFramebuffer;

namespace Wayland
{
class WaylandBackend;
class WaylandOutput;
class EglWaylandBackend;

class EglWaylandOutput : public OutputLayer
{
public:
    EglWaylandOutput(WaylandOutput *output, EglWaylandBackend *backend);
    ~EglWaylandOutput() override;

    bool init();
    void updateSize();

    GLFramebuffer *fbo() const;
    bool makeContextCurrent() const;
    void present();

    OutputLayerBeginFrameInfo beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void aboutToStartPainting(const QRegion &damage) override;

private:
    void resetBufferAge();

    WaylandOutput *m_waylandOutput;
    wl_egl_window *m_overlay = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    int m_bufferAge = 0;
    DamageJournal m_damageJournal;
    QScopedPointer<GLFramebuffer> m_fbo;
    EglWaylandBackend *const m_backend;

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

    SurfaceTexture *createSurfaceTextureInternal(SurfacePixmapInternal *pixmap) override;
    SurfaceTexture *createSurfaceTextureWayland(SurfacePixmapWayland *pixmap) override;

    void init() override;
    void present(Output *output) override;
    OutputLayer *primaryLayer(Output *output) override;

    bool havePlatformBase() const
    {
        return m_havePlatformBase;
    }

    QSharedPointer<KWin::GLTexture> textureForOutput(KWin::Output *output) const override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    bool createEglWaylandOutput(Output *output);

    void cleanupSurfaces() override;

    void presentOnSurface(EglWaylandOutput *output, const QRegion &damagedRegion);

    WaylandBackend *m_backend;
    QMap<Output *, QSharedPointer<EglWaylandOutput>> m_outputs;
    bool m_havePlatformBase;
    friend class EglWaylandTexture;
};

}
}

#endif
