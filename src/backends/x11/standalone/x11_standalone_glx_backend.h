/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/outputlayer.h"
#include "options.h"
#include "platformsupport/scenes/opengl/openglbackend.h"
#include "platformsupport/scenes/opengl/openglsurfacetexture_x11.h"
#include "utils/damagejournal.h"
#include "x11eventfilter.h"

#include <epoxy/glx.h>
#include <fixx11h.h>
#include <xcb/glx.h>

#include "opengl/gltexture.h"
#include "opengl/gltexture_p.h"

#include <QHash>
#include <memory>

namespace KWin
{

class GlxPixmapTexturePrivate;
class VsyncMonitor;
class X11StandaloneBackend;
class GlxBackend;
class GLRenderTimeQuery;
class GlxContext;

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
    SwapEventFilter(GlxBackend *backend, xcb_drawable_t drawable, xcb_glx_drawable_t glxDrawable);
    bool event(xcb_generic_event_t *event) override;

private:
    GlxBackend *m_backend;
    xcb_drawable_t m_drawable;
    xcb_glx_drawable_t m_glxDrawable;
};

class GlxLayer : public OutputLayer
{
public:
    GlxLayer(GlxBackend *backend);

    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

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
    GlxBackend(::Display *display, X11StandaloneBackend *backend);
    ~GlxBackend() override;
    std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap) override;
    OutputLayerBeginFrameInfo doBeginFrame();
    void endFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame);
    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    bool makeCurrent() override;
    void doneCurrent() override;
    OpenGlContext *openglContext() const override;
    OverlayWindow *overlayWindow() const override;
    void init() override;
    OutputLayer *primaryLayer(Output *output) override;

    ::Display *display() const
    {
        return m_x11Display;
    }

    void vblank(std::chrono::nanoseconds timestamp);

private:
    void present(const QRegion &damage);
    bool initBuffer();
    bool checkVersion();
    void initExtensions();
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
    xcb_colormap_t m_colormap;
    std::shared_ptr<GlxContext> m_context;
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
    ::Display *m_x11Display;
    X11StandaloneBackend *m_backend;
    std::unique_ptr<VsyncMonitor> m_vsyncMonitor;
    std::unique_ptr<GlxLayer> m_layer;
    std::unique_ptr<GLRenderTimeQuery> m_query;
    Options::GlSwapStrategy m_swapStrategy = Options::AutoSwapStrategy;
    std::shared_ptr<OutputFrame> m_frame;
    friend class GlxPixmapTexture;
};

class GlxPixmapTexture final : public GLTexture
{
public:
    explicit GlxPixmapTexture(GlxBackend *backend);
    ~GlxPixmapTexture();

    bool create(SurfacePixmapX11 *texture);

private:
    void onDamage() override;

    GlxBackend *const m_backend;
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
