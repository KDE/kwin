/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_GLX_BACKEND_H
#define KWIN_GLX_BACKEND_H
#include "scene_opengl.h"
#include "x11eventfilter.h"

#include <xcb/glx.h>
#include <epoxy/glx.h>
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
 **/
class GlxBackend : public OpenGLBackend
{
public:
    GlxBackend();
    virtual ~GlxBackend();
    virtual void screenGeometryChanged(const QSize &size);
    virtual SceneOpenGL::TexturePrivate *createBackendTexture(SceneOpenGL::Texture *texture);
    virtual QRegion prepareRenderingFrame();
    virtual void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion);
    virtual bool makeCurrent() override;
    virtual void doneCurrent() override;
    virtual OverlayWindow* overlayWindow() override;
    virtual bool usesOverlayWindow() const override;
    void init() override;

protected:
    virtual void present();

private:
    bool initBuffer();
    bool checkVersion();
    void initExtensions();
    void waitSync();
    bool initRenderingContext();
    bool initFbConfig();
    void initVisualDepthHashTable();
    void setSwapInterval(int interval);

    int visualDepth(xcb_visualid_t visual) const;
    FBConfigInfo *infoForVisual(xcb_visualid_t visual);

    /**
     * @brief The OverlayWindow used by this Backend.
     **/
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
    friend class GlxTexture;
};

/**
 * @brief Texture using an GLXPixmap.
 **/
class GlxTexture : public SceneOpenGL::TexturePrivate
{
public:
    virtual ~GlxTexture();
    virtual void onDamage();
    virtual bool loadTexture(WindowPixmap *pixmap) override;
    virtual OpenGLBackend *backend();

private:
    friend class GlxBackend;
    GlxTexture(SceneOpenGL::Texture *texture, GlxBackend *backend);
    bool loadTexture(xcb_pixmap_t pix, const QSize &size, xcb_visualid_t visual);
    SceneOpenGL::Texture *q;
    GlxBackend *m_backend;
    GLXPixmap m_glxpixmap; // the glx pixmap the texture is bound to
};

} // namespace
#endif // KWIN_GLX_BACKEND_H
