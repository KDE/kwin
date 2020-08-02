/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GLX_BACKEND_H
#define KWIN_GLX_BACKEND_H
#include "backend.h"
#include "texture.h"
#include "swap_profiler.h"
#include "x11eventfilter.h"

#include <xcb/glx.h>
#include <epoxy/glx.h>
#include <fixx11h.h>
#include <memory>

namespace KWin
{

// GLX_MESA_swap_interval
using glXSwapIntervalMESA_func = int (*)(unsigned int interval);
extern glXSwapIntervalMESA_func glXSwapIntervalMESA;

class FBConfigInfo
{
public:
    GLXFBConfig fbconfig;
    int bind_texture_format;
    int texture_targets;
    int y_inverted;
    int mipmap;
};


// ------------------------------------------------------------------


class SwapEventFilter : public X11EventFilter
{
public:
    SwapEventFilter(xcb_drawable_t drawable, xcb_glx_drawable_t glxDrawable);
    bool event(xcb_generic_event_t *event) override;

private:
    xcb_drawable_t m_drawable;
    xcb_glx_drawable_t m_glxDrawable;
};


/**
 * @brief OpenGL Backend using GLX over an X overlay window.
 */
class GlxBackend : public OpenGLBackend
{
public:
    GlxBackend(Display *display);
    ~GlxBackend() override;
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion) override;
    bool makeCurrent() override;
    void doneCurrent() override;
    OverlayWindow* overlayWindow() const override;
    bool usesOverlayWindow() const override;
    void init() override;

protected:
    void present() override;

private:
    bool initBuffer();
    bool checkVersion();
    void initExtensions();
    void waitSync();
    bool initRenderingContext();
    bool initFbConfig();
    void initVisualDepthHashTable();
    void setSwapInterval(int interval);
    Display *display() const {
        return m_x11Display;
    }

    int visualDepth(xcb_visualid_t visual) const;
    FBConfigInfo *infoForVisual(xcb_visualid_t visual);

    /**
     * @brief The OverlayWindow used by this Backend.
     */
    OverlayWindow *m_overlayWindow;
    Window window;
    GLXFBConfig fbconfig;
    GLXWindow glxWindow;
    GLXContext ctx;
    QHash<xcb_visualid_t, FBConfigInfo *> m_fbconfigHash;
    QHash<xcb_visualid_t, int> m_visualDepthHash;
    std::unique_ptr<SwapEventFilter> m_swapEventFilter;
    int m_bufferAge;
    bool m_haveMESACopySubBuffer = false;
    bool m_haveMESASwapControl = false;
    bool m_haveEXTSwapControl = false;
    bool m_haveSGISwapControl = false;
    bool m_haveINTELSwapEvent = false;
    bool haveSwapInterval = false;
    bool haveWaitSync = false;
    Display *m_x11Display;
    SwapProfiler m_swapProfiler;
    friend class GlxTexture;
};

/**
 * @brief Texture using an GLXPixmap.
 */
class GlxTexture : public SceneOpenGLTexturePrivate
{
public:
    ~GlxTexture() override;
    void onDamage() override;
    bool loadTexture(WindowPixmap *pixmap) override;
    OpenGLBackend *backend() override;

private:
    friend class GlxBackend;
    GlxTexture(SceneOpenGLTexture *texture, GlxBackend *backend);
    bool loadTexture(xcb_pixmap_t pix, const QSize &size, xcb_visualid_t visual);
    Display *display() const {
        return m_backend->m_x11Display;
    }
    SceneOpenGLTexture *q;
    GlxBackend *m_backend;
    GLXPixmap m_glxpixmap; // the glx pixmap the texture is bound to
};

} // namespace
#endif // KWIN_GLX_BACKEND_H
