/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.

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

// TODO: cmake magic
#ifndef KWIN_HAVE_OPENGLES
// own
#include "glxbackend.h"
// kwin
#include "options.h"
#include "utils.h"
#include "overlaywindow.h"
// kwin libs
#include <kwinglplatform.h>
// KDE
#include <KDE/KDebug>
#include <KDE/KXErrorHandler>

namespace KWin
{
GlxBackend::GlxBackend()
    : OpenGLBackend()
    , window(None)
    , fbconfig(NULL)
    , glxWindow(None)
    , ctx(None)
    , haveSwapInterval(false)
{
    init();
}

GlxBackend::~GlxBackend()
{
    // TODO: cleanup in error case
    // do cleanup after initBuffer()
    cleanupGL();
    glXMakeCurrent(display(), None, NULL);

    if (ctx)
        glXDestroyContext(display(), ctx);

    if (glxWindow)
        glXDestroyWindow(display(), glxWindow);

    if (window)
        XDestroyWindow(display(), window);

    overlayWindow()->destroy();
    checkGLError("Cleanup");
}

void GlxBackend::init()
{
    initGLX();
    // require at least GLX 1.3
    if (!hasGLXVersion(1, 3)) {
        setFailed("Requires at least GLX 1.3");
        return;
    }
    if (!initDrawableConfigs()) {
        setFailed("Could not initialize the drawable configs");
        return;
    }
    if (!initBuffer()) {
        setFailed("Could not initialize the buffer");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }
    // Initialize OpenGL
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect(GlxPlatformInterface);
    glPlatform->printResults();
    initGL(GlxPlatformInterface);
    // Check whether certain features are supported
    haveSwapInterval = glXSwapIntervalMESA || glXSwapIntervalEXT || glXSwapIntervalSGI;
    const bool wantSync = options->glPreferBufferSwap() != Options::NoSwapEncourage;
    if (wantSync) {
        if (glXGetVideoSync && haveSwapInterval && glXIsDirect(display(), ctx)) {
            unsigned int sync;
            if (glXGetVideoSync(&sync) == 0) {
                if (glXWaitVideoSync(1, 0, &sync) == 0) {
                    // NOTICE at this time we should actually check whether we can successfully
                    // deactivate the swapInterval "glXSwapInterval(0) == 0"
                    // (because we don't actually want it active unless we explicitly run a glXSwapBuffers)
                    // However mesa/dri will return a range error (6) because deactivating the
                    // swapinterval (as of today) seems completely unsupported
                    setHasWaitSync(true);
                    setSwapInterval(1);
                } else
                    qWarning() << "NO VSYNC! glXWaitVideoSync(1,0,&uint) isn't 0 but" << glXWaitVideoSync(1, 0, &sync);
            } else
                qWarning() << "NO VSYNC! glXGetVideoSync(&uint) isn't 0 but" << glXGetVideoSync(&sync);
        } else
            qWarning() << "NO VSYNC! glXGetVideoSync, haveSwapInterval, glXIsDirect" <<
                        bool(glXGetVideoSync) << haveSwapInterval << glXIsDirect(display(), ctx);
    } else {
        // disable v-sync (if possible)
        setSwapInterval(0);
    }
    if (glPlatform->isVirtualBox()) {
        // VirtualBox does not support glxQueryDrawable
        // this should actually be in kwinglutils_funcs, but QueryDrawable seems not to be provided by an extension
        // and the GLPlatform has not been initialized at the moment when initGLX() is called.
        glXQueryDrawable = NULL;
    }

    setIsDirectRendering(bool(glXIsDirect(display(), ctx)));

    kDebug(1212) << "Direct rendering:" << isDirectRendering() << endl;
}

bool GlxBackend::initRenderingContext()
{
    bool direct = options->isGlDirect();

    // Use glXCreateContextAttribsARB() when it's available
    if (glXCreateContextAttribsARB) {
        int attribs[] = { 0 };
        ctx = glXCreateContextAttribsARB(display(), fbconfig, 0, direct, attribs);
    }

    if (!ctx)
        ctx = glXCreateNewContext(display(), fbconfig, GLX_RGBA_TYPE, NULL, direct);

    if (!ctx) {
        kDebug(1212) << "Failed to create an OpenGL context.";
        return false;
    }

    if (!glXMakeCurrent(display(), glxWindow, ctx)) {
        kDebug(1212) << "Failed to make the OpenGL context current.";
        glXDestroyContext(display(), ctx);
        ctx = 0;
        return false;
    }

    return true;
}

bool GlxBackend::initBuffer()
{
    if (!initFbConfig())
        return false;

    if (overlayWindow()->create()) {
        // Try to create double-buffered window in the overlay
        XVisualInfo* visual = glXGetVisualFromFBConfig(display(), fbconfig);
        XSetWindowAttributes attrs;
        attrs.colormap = XCreateColormap(display(), rootWindow(), visual->visual, AllocNone);
        window = XCreateWindow(display(), overlayWindow()->window(), 0, 0, displayWidth(), displayHeight(),
                               0, visual->depth, InputOutput, visual->visual, CWColormap, &attrs);
        glxWindow = glXCreateWindow(display(), fbconfig, window, NULL);
        overlayWindow()->setup(window);
        XFree(visual);
    } else {
        kError(1212) << "Failed to create overlay window";
        return false;
    }

    int vis_buffer;
    glXGetFBConfigAttrib(display(), fbconfig, GLX_VISUAL_ID, &vis_buffer);
    XVisualInfo* visinfo_buffer = glXGetVisualFromFBConfig(display(), fbconfig);
    kDebug(1212) << "Buffer visual (depth " << visinfo_buffer->depth << "): 0x" << QString::number(vis_buffer, 16);
    XFree(visinfo_buffer);

    return true;
}

bool GlxBackend::initFbConfig()
{
    const int attribs[] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_RED_SIZE,       1,
        GLX_GREEN_SIZE,     1,
        GLX_BLUE_SIZE,      1,
        GLX_ALPHA_SIZE,     0,
        GLX_DEPTH_SIZE,     0,
        GLX_STENCIL_SIZE,   0,
        GLX_CONFIG_CAVEAT,  GLX_NONE,
        GLX_DOUBLEBUFFER,   true,
        0
    };

    // Try to find a double buffered configuration
    int count = 0;
    GLXFBConfig *configs = glXChooseFBConfig(display(), DefaultScreen(display()), attribs, &count);

    if (count > 0) {
        fbconfig = configs[0];
        XFree(configs);
    }

    if (fbconfig == NULL) {
        kError(1212) << "Failed to find a usable framebuffer configuration";
        return false;
    }

    return true;
}

bool GlxBackend::initDrawableConfigs()
{
    const int attribs[] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT | GLX_PIXMAP_BIT,
        GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
        GLX_X_RENDERABLE,   True,
        GLX_CONFIG_CAVEAT,  GLX_NONE,
        GLX_RED_SIZE,       5,
        GLX_GREEN_SIZE,     5,
        GLX_BLUE_SIZE,      5,
        GLX_ALPHA_SIZE,     0,
        GLX_STENCIL_SIZE,   0,
        GLX_DEPTH_SIZE,     0,
        0
    };

    int count = 0;
    GLXFBConfig *configs = glXChooseFBConfig(display(), DefaultScreen(display()), attribs, &count);

    if (count < 1) {
        kError(1212) << "Could not find any usable framebuffer configurations.";
        return false;
    }

    for (int i = 0; i <= 32; i++) {
        fbcdrawableinfo[i].fbconfig            = NULL;
        fbcdrawableinfo[i].bind_texture_format = 0;
        fbcdrawableinfo[i].texture_targets     = 0;
        fbcdrawableinfo[i].y_inverted          = 0;
        fbcdrawableinfo[i].mipmap              = 0;
    }

    // Find the first usable framebuffer configuration for each depth.
    // Single-buffered ones will appear first in the list.
    const int depths[] = { 15, 16, 24, 30, 32 };
    for (unsigned int i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
        const int depth = depths[i];

        for (int j = 0; j < count; j++) {
            int alpha_size, buffer_size;
            glXGetFBConfigAttrib(display(), configs[j], GLX_ALPHA_SIZE,  &alpha_size);
            glXGetFBConfigAttrib(display(), configs[j], GLX_BUFFER_SIZE, &buffer_size);

            if (buffer_size != depth && (buffer_size - alpha_size) != depth)
                continue;

            if (depth == 32 && alpha_size != 8)
                continue;

            XVisualInfo *vi = glXGetVisualFromFBConfig(display(), configs[j]);
            if (vi == NULL)
                continue;

            int visual_depth = vi->depth;
            XFree(vi);

            if (visual_depth != depth)
                continue;

            int bind_rgb, bind_rgba;
            glXGetFBConfigAttrib(display(), configs[j], GLX_BIND_TO_TEXTURE_RGBA_EXT, &bind_rgba);
            glXGetFBConfigAttrib(display(), configs[j], GLX_BIND_TO_TEXTURE_RGB_EXT,  &bind_rgb);

            // Skip this config if it cannot be bound to a texture
            if (!bind_rgb && !bind_rgba)
                continue;

            int texture_format;
            if (depth == 32)
                texture_format = bind_rgba ? GLX_TEXTURE_FORMAT_RGBA_EXT : GLX_TEXTURE_FORMAT_RGB_EXT;
            else
                texture_format = bind_rgb ? GLX_TEXTURE_FORMAT_RGB_EXT : GLX_TEXTURE_FORMAT_RGBA_EXT;

            int y_inverted, texture_targets;
            glXGetFBConfigAttrib(display(), configs[j], GLX_BIND_TO_TEXTURE_TARGETS_EXT, &texture_targets);
            glXGetFBConfigAttrib(display(), configs[j], GLX_Y_INVERTED_EXT, &y_inverted);

            fbcdrawableinfo[depth].fbconfig            = configs[j];
            fbcdrawableinfo[depth].bind_texture_format = texture_format;
            fbcdrawableinfo[depth].texture_targets     = texture_targets;
            fbcdrawableinfo[depth].y_inverted          = y_inverted;
            fbcdrawableinfo[depth].mipmap              = 0;
            break;
        }
    }

    if (count)
        XFree(configs);

    if (fbcdrawableinfo[DefaultDepth(display(), DefaultScreen(display()))].fbconfig == NULL) {
        kError(1212) << "Could not find a framebuffer configuration for the default depth.";
        return false;
    }

    if (fbcdrawableinfo[32].fbconfig == NULL) {
        kError(1212) << "Could not find a framebuffer configuration for depth 32.";
        return false;
    }

    for (int i = 0; i <= 32; i++) {
        if (fbcdrawableinfo[i].fbconfig == NULL)
            continue;

        int vis_drawable = 0;
        glXGetFBConfigAttrib(display(), fbcdrawableinfo[i].fbconfig, GLX_VISUAL_ID, &vis_drawable);

        kDebug(1212) << "Drawable visual (depth " << i << "): 0x" << QString::number(vis_drawable, 16);
    }

    return true;
}

void GlxBackend::setSwapInterval(int interval)
{
    if (glXSwapIntervalEXT)
        glXSwapIntervalEXT(display(), glxWindow, interval);
    else if (glXSwapIntervalMESA)
        glXSwapIntervalMESA(interval);
    else if (glXSwapIntervalSGI)
        glXSwapIntervalSGI(interval);
}

#define VSYNC_DEBUG 0

void GlxBackend::waitSync()
{
    // NOTE that vsync has no effect with indirect rendering
    if (waitSyncAvailable()) {
#if VSYNC_DEBUG
        startRenderTimer();
#endif
        uint sync;
#if 0
        // TODO: why precisely is this important?
        // the sync counter /can/ perform multiple steps during glXGetVideoSync & glXWaitVideoSync
        // but this only leads to waiting for two frames??!?
        glXGetVideoSync(&sync);
        glXWaitVideoSync(2, (sync + 1) % 2, &sync);
#else
        glXWaitVideoSync(1, 0, &sync);
#endif
#if VSYNC_DEBUG
        static int waitTime = 0, waitCounter = 0, doubleSyncCounter = 0;
        if (renderTime() > 11)
            ++doubleSyncCounter;
        waitTime += renderTime();
        ++waitCounter;
        if (waitCounter > 99)
        {
            qDebug() << "mean vsync wait time:" << float((float)waitTime / (float)waitCounter) << doubleSyncCounter << "/100";
            doubleSyncCounter = waitTime = waitCounter = 0;
        }
#endif
    }
    startRenderTimer(); // yes, the framerate shall be constant anyway.
}

#undef VSYNC_DEBUG

void GlxBackend::present()
{
    const QRegion displayRegion(0, 0, displayWidth(), displayHeight());
    const bool fullRepaint = (lastDamage() == displayRegion);

    if (fullRepaint) {
        if (haveSwapInterval) {
            glXSwapBuffers(display(), glxWindow);
            startRenderTimer();
        } else {
            waitSync(); // calls startRenderTimer();
            glXSwapBuffers(display(), glxWindow);
        }
    } else if (glXCopySubBuffer) {
        waitSync();
        foreach (const QRect & r, lastDamage().rects()) {
            // convert to OpenGL coordinates
            int y = displayHeight() - r.y() - r.height();
            glXCopySubBuffer(display(), glxWindow, r.x(), y, r.width(), r.height());
        }
    } else { // Copy Pixels (horribly slow on Mesa)
        glDrawBuffer(GL_FRONT);
        waitSync();
        SceneOpenGL::copyPixels(lastDamage());
        glDrawBuffer(GL_BACK);
    }

    glXWaitGL();
    setLastDamage(QRegion());
    XFlush(display());
}

void GlxBackend::screenGeometryChanged(const QSize &size)
{
    glXMakeCurrent(display(), None, NULL);

    XMoveResizeWindow(display(), window, 0, 0, size.width(), size.height());
    overlayWindow()->setup(window);
    XSync(display(), false);

    glXMakeCurrent(display(), glxWindow, ctx);
    glViewport(0, 0, size.width(), size.height());
}

SceneOpenGL::TexturePrivate *GlxBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new GlxTexture(texture, this);
}

void GlxBackend::prepareRenderingFrame()
{
    if (!lastDamage().isEmpty())
        present();
    glXWaitX();
}

void GlxBackend::endRenderingFrame(const QRegion &damage)
{
    setLastDamage(damage);
    glFlush();

    if (overlayWindow()->window())  // show the window only after the first pass,
        overlayWindow()->show();   // since that pass may take long
}


/********************************************************
 * GlxTexture
 *******************************************************/
GlxTexture::GlxTexture(SceneOpenGL::Texture *texture, GlxBackend *backend)
    : SceneOpenGL::TexturePrivate()
    , q(texture)
    , m_backend(backend)
    , m_glxpixmap(None)
{
}

GlxTexture::~GlxTexture()
{
    if (m_glxpixmap != None) {
        if (!options->isGlStrictBinding()) {
            glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        }
        glXDestroyPixmap(display(), m_glxpixmap);
        m_glxpixmap = None;
    }
}

void GlxTexture::onDamage()
{
    if (options->isGlStrictBinding() && m_glxpixmap) {
        glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
    }
    GLTexturePrivate::onDamage();
}

void GlxTexture::findTarget()
{
    unsigned int new_target = 0;
    if (glXQueryDrawable && m_glxpixmap != None)
        glXQueryDrawable(display(), m_glxpixmap, GLX_TEXTURE_TARGET_EXT, &new_target);
    // HACK: this used to be a hack for Xgl.
    // without this hack the NVIDIA blob aborts when trying to bind a texture from
    // a pixmap icon
    if (new_target == 0) {
        if (GLTexture::NPOTTextureSupported() ||
            (isPowerOfTwo(m_size.width()) && isPowerOfTwo(m_size.height()))) {
            new_target = GLX_TEXTURE_2D_EXT;
        } else {
            new_target = GLX_TEXTURE_RECTANGLE_EXT;
        }
    }
    switch(new_target) {
    case GLX_TEXTURE_2D_EXT:
        m_target = GL_TEXTURE_2D;
        m_scale.setWidth(1.0f / m_size.width());
        m_scale.setHeight(1.0f / m_size.height());
        break;
    case GLX_TEXTURE_RECTANGLE_EXT:
        m_target = GL_TEXTURE_RECTANGLE_ARB;
        m_scale.setWidth(1.0f);
        m_scale.setHeight(1.0f);
        break;
    default:
        abort();
    }
}

bool GlxTexture::loadTexture(const Pixmap& pix, const QSize& size, int depth)
{
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad1");
#endif
    if (pix == None || size.isEmpty() || depth < 1)
        return false;
    if (m_backend->fbcdrawableinfo[ depth ].fbconfig == NULL) {
        kDebug(1212) << "No framebuffer configuration for depth " << depth
                     << "; not binding pixmap" << endl;
        return false;
    }

    m_size = size;
    // new texture, or texture contents changed; mipmaps now invalid
    q->setDirty();

#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad2");
#endif
    // tfp mode, simply bind the pixmap to texture
    glGenTextures(1, &m_texture);
    // The GLX pixmap references the contents of the original pixmap, so it doesn't
    // need to be recreated when the contents change.
    // The texture may or may not use the same storage depending on the EXT_tfp
    // implementation. When options->glStrictBinding is true, the texture uses
    // a different storage and needs to be updated with a call to
    // glXBindTexImageEXT() when the contents of the pixmap has changed.
    int attrs[] = {
        GLX_TEXTURE_FORMAT_EXT, m_backend->fbcdrawableinfo[ depth ].bind_texture_format,
        GLX_MIPMAP_TEXTURE_EXT, m_backend->fbcdrawableinfo[ depth ].mipmap > 0,
        None, None, None
    };
    // Specifying the texture target explicitly is reported to cause a performance
    // regression with R300G (see bug #256654).
    if (GLPlatform::instance()->driver() != Driver_R300G) {
        if ((m_backend->fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_2D_BIT_EXT) &&
                (GLTexture::NPOTTextureSupported() ||
                  (isPowerOfTwo(size.width()) && isPowerOfTwo(size.height())))) {
            attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
            attrs[ 5 ] = GLX_TEXTURE_2D_EXT;
        } else if (m_backend->fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_RECTANGLE_BIT_EXT) {
            attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
            attrs[ 5 ] = GLX_TEXTURE_RECTANGLE_EXT;
        }
    }
    m_glxpixmap = glXCreatePixmap(display(), m_backend->fbcdrawableinfo[ depth ].fbconfig, pix, attrs);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoadTFP1");
#endif
    findTarget();
    m_yInverted = m_backend->fbcdrawableinfo[ depth ].y_inverted ? true : false;
    m_canUseMipmaps = m_backend->fbcdrawableinfo[ depth ].mipmap > 0;
    q->setFilter(m_backend->fbcdrawableinfo[ depth ].mipmap > 0 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);
    glBindTexture(m_target, m_texture);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoadTFP2");
#endif
    glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad0");
#endif
    unbind();
    return true;
}

OpenGLBackend *GlxTexture::backend()
{
    return m_backend;
}

} // namespace
#endif
