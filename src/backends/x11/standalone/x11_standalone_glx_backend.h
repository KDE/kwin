/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"
#include "openglbackend.h"
#include "openglsurfacetexture_x11.h"
#include "utils/damagejournal.h"
#include "x11eventfilter.h"

#include <epoxy/glx.h>
#include <fixx11h.h>
#include <xcb/glx.h>

#include <kwingltexture.h>
#include <kwingltexture_p.h>

#include <QHash>
#include <memory>

namespace KWin
{

class GlxPixmapTexturePrivate;
class VsyncMonitor;
class X11StandaloneBackend;
class GlxBackend;

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

class GlxLayer : public OutputLayer
{
public:
    GlxLayer(GlxBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> beginFrame() override;
    bool endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;

private:
    GlxBackend *const m_backend;
};

/**
 * @brief OpenGL Backend using GLX over an X overlay window.
 */
class GlxBackend : public OpenGLBackend
{
    Q_OBJECT

public:
    GlxBackend(Display *display, X11StandaloneBackend *backend);
    ~GlxBackend() override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap) override;
    OutputLayerBeginFrameInfo beginFrame();
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion);
    void present(Output *output) override;
    bool makeCurrent() override;
    void doneCurrent() override;
    OverlayWindow *overlayWindow() const override;
    void init() override;
    OutputLayer *primaryLayer(Output *output) override;

    Display *display() const
    {
        return m_x11Display;
    }

private:
    void vblank(std::chrono::nanoseconds timestamp);
    void present(const QRegion &damage);
    bool initBuffer();
    bool checkVersion();
    void initExtensions();
    bool initRenderingContext();
    bool initFbConfig();
    void initVisualDepthHashTable();
    void setSwapInterval(int interval);
    void screenGeometryChanged();

    int visualDepth(xcb_visualid_t visual) const;
    const FBConfigInfo &infoForVisual(xcb_visualid_t visual);

    /**
     * @brief The OverlayWindow used by this Backend.
     */
    std::unique_ptr<OverlayWindow> m_overlayWindow;
    ::Window window;
    GLXFBConfig fbconfig;
    GLXWindow glxWindow;
    GLXContext ctx;
    QHash<xcb_visualid_t, FBConfigInfo> m_fbconfigHash;
    QHash<xcb_visualid_t, int> m_visualDepthHash;
    std::unique_ptr<SwapEventFilter> m_swapEventFilter;
    std::unique_ptr<GLFramebuffer> m_fbo;
    DamageJournal m_damageJournal;
    QRegion m_lastRenderedRegion;
    int m_bufferAge;
    bool m_haveMESACopySubBuffer = false;
    bool m_haveMESASwapControl = false;
    bool m_haveEXTSwapControl = false;
    bool m_haveSGISwapControl = false;
    Display *m_x11Display;
    X11StandaloneBackend *m_backend;
    std::unique_ptr<VsyncMonitor> m_vsyncMonitor;
    std::unique_ptr<GlxLayer> m_layer;
    friend class GlxPixmapTexturePrivate;
};

class GlxPixmapTexture final : public GLTexture
{
public:
    explicit GlxPixmapTexture(GlxBackend *backend);

    bool create(SurfacePixmapX11 *texture);

private:
    Q_DECLARE_PRIVATE(GlxPixmapTexture)
};

class GlxPixmapTexturePrivate final : public GLTexturePrivate
{
public:
    GlxPixmapTexturePrivate(GlxPixmapTexture *texture, GlxBackend *backend);
    ~GlxPixmapTexturePrivate() override;

    bool create(SurfacePixmapX11 *texture);

protected:
    void onDamage() override;

private:
    GlxBackend *m_backend;
    GlxPixmapTexture *q;
    GLXPixmap m_glxPixmap;
};

class GlxSurfaceTextureX11 final : public OpenGLSurfaceTextureX11
{
public:
    GlxSurfaceTextureX11(GlxBackend *backend, SurfacePixmapX11 *pixmap);

    bool create() override;
    void update(const QRegion &region) override;
};

} // namespace
