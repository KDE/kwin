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
#include "utils/common.h"
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
    void updateSize();

private:
    void resetBufferAge();

    WaylandOutput *m_waylandOutput;
    wl_egl_window *m_overlay = nullptr;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    int m_bufferAge = 0;
    DamageJournal m_damageJournal;

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

    QRegion beginFrame(AbstractOutput *output) override;
    void endFrame(AbstractOutput *output, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void init() override;

    bool havePlatformBase() const {
        return m_havePlatformBase;
    }

    void aboutToStartPainting(AbstractOutput *output, const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();

    bool createEglWaylandOutput(AbstractOutput *output);

    void cleanupSurfaces() override;
    void cleanupOutput(EglWaylandOutput *output);

    bool makeContextCurrent(EglWaylandOutput *output);
    void presentOnSurface(EglWaylandOutput *output, const QRegion &damagedRegion);

    WaylandBackend *m_backend;
    QMap<AbstractOutput *, EglWaylandOutput*> m_outputs;
    bool m_havePlatformBase;
    friend class EglWaylandTexture;
};

}
}

#endif
