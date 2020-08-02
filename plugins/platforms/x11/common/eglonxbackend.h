/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_EGL_ON_X_BACKEND_H
#define KWIN_EGL_ON_X_BACKEND_H
#include "abstract_egl_backend.h"
#include "swap_profiler.h"

#include <xcb/xcb.h>

namespace KWin
{

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 */
class KWIN_EXPORT EglOnXBackend : public AbstractEglBackend
{
public:
    EglOnXBackend(Display *display);
    explicit EglOnXBackend(xcb_connection_t *connection, Display *display, xcb_window_t rootWindow, int screenNumber, xcb_window_t renderingWindow);
    ~EglOnXBackend() override;
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion) override;
    OverlayWindow* overlayWindow() const override;
    bool usesOverlayWindow() const override;
    void init() override;

    bool isX11TextureFromPixmapSupported() const {
        return m_x11TextureFromPixmapSupported;
    }

protected:
    void present() override;
    void presentSurface(EGLSurface surface, const QRegion &damage, const QRect &screenGeometry);
    virtual bool createSurfaces();
    EGLSurface createSurface(xcb_window_t window);
    void setHavePlatformBase(bool have) {
        m_havePlatformBase = have;
    }
    bool havePlatformBase() const {
        return m_havePlatformBase;
    }
    bool makeContextCurrent(const EGLSurface &surface);

    void setX11TextureFromPixmapSupported(bool set) {
        m_x11TextureFromPixmapSupported = set;
    }

private:
    bool initBufferConfigs();
    bool initRenderingContext();
    /**
     * @brief The OverlayWindow used by this Backend.
     */
    OverlayWindow *m_overlayWindow;
    int surfaceHasSubPost;
    int m_bufferAge;
    bool m_usesOverlayWindow;
    xcb_connection_t *m_connection;
    Display *m_x11Display;
    xcb_window_t m_rootWindow;
    int m_x11ScreenNumber;
    xcb_window_t m_renderingWindow = XCB_WINDOW_NONE;
    bool m_havePlatformBase = false;
    bool m_x11TextureFromPixmapSupported = true;
    SwapProfiler m_swapProfiler;
    friend class EglTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 */
class EglTexture : public AbstractEglTexture
{
public:
    ~EglTexture() override;
    void onDamage() override;
    bool loadTexture(WindowPixmap *pixmap) override;

private:
    bool loadTexture(xcb_pixmap_t pix, const QSize &size);
    friend class EglOnXBackend;
    EglTexture(SceneOpenGLTexture *texture, EglOnXBackend *backend);
    EglOnXBackend *m_backend;
};

} // namespace

#endif //  KWIN_EGL_ON_X_BACKEND_H
