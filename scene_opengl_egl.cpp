/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Martin Gräßlin <kde@martin-graesslin.com>

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

// This file is included in scene_opengl.cpp
//#include "scene_opengl.h"
#include <QX11Info>

EGLDisplay dpy;
EGLConfig config;
EGLSurface surface;
EGLContext ctx;
int surfaceHasSubPost;

SceneOpenGL::SceneOpenGL(Workspace* ws)
    : Scene(ws)
    , init_ok(false)
    , m_colorCorrection(new ColorCorrection(this))
{
    if (!initRenderingContext())
        return;

    initEGL();
    if (!hasGLExtension("EGL_KHR_image") &&
        (!hasGLExtension("EGL_KHR_image_base") ||
         !hasGLExtension("EGL_KHR_image_pixmap"))) {
        kError(1212) << "Required support for binding pixmaps to EGLImages not found, disabling compositing";
        return;
    }
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect();
    glPlatform->printResults();
    initGL();
    if (!hasGLExtension("GL_OES_EGL_image")) {
        kError(1212) << "Required extension GL_OES_EGL_image not found, disabling compositing";
        return;
    }
    debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;
    initColorCorrection();
    if (!ShaderManager::instance()->isValid()) {
        kError(1212) << "Shaders not valid, ES compositing not possible";
        return;
    }
    ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);

    if (checkGLError("Init")) {
        kError(1212) << "OpenGL compositing setup failed";
        return; // error
    }
    init_ok = true;

// TODO: activate once this is resolved. currently the explicit invocation seems pointless
#if 0
    // - internet rumors say: it doesn't work with TBDR
    // - eglSwapInterval has no impact on intel GMA chips
    has_waitSync = options->isGlVSync();
    if (has_waitSync) {
        has_waitSync = (eglSwapInterval(dpy, 1) == EGL_TRUE);
        if (!has_waitSync)
            kWarning(1212) << "Could not activate EGL v'sync on this system";
    }
    if (!has_waitSync)
        eglSwapInterval(dpy, 0); // deactivate syncing
#endif
}

SceneOpenGL::~SceneOpenGL()
{
    if (!init_ok) {
        // TODO this probably needs to clean up whatever has been created until the failure
        m_overlayWindow->destroy();
        return;
    }
    foreach (Window * w, windows)
    delete w;
    // do cleanup after initBuffer()
    cleanupGL();
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surface);
    eglTerminate(dpy);
    eglReleaseThread();
    SceneOpenGL::EffectFrame::cleanup();
    checkGLError("Cleanup");
    if (m_overlayWindow->window()) {
        m_overlayWindow->destroy();
    }
}

bool SceneOpenGL::initTfp()
{
    return false;
}

bool SceneOpenGL::initRenderingContext()
{
    dpy = eglGetDisplay(display());
    if (dpy == EGL_NO_DISPLAY)
        return false;
    EGLint major, minor;
    if (eglInitialize(dpy, &major, &minor) == EGL_FALSE)
        return false;
    eglBindAPI(EGL_OPENGL_ES_API);
    initBufferConfigs();
    if (!m_overlayWindow->create()) {
        kError(1212) << "Could not get overlay window";
        return false;
    } else {
        m_overlayWindow->setup(None);
    }
    surface = eglCreateWindowSurface(dpy, config, m_overlayWindow->window(), 0);

    eglSurfaceAttrib(dpy, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);

    eglQuerySurface(dpy, surface, EGL_POST_SUB_BUFFER_SUPPORTED_NV, &surfaceHasSubPost);

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
    if (ctx == EGL_NO_CONTEXT)
        return false;
    if (eglMakeCurrent(dpy, surface, surface, ctx) == EGL_FALSE)
        return false;
    kDebug(1212) << "EGL version: " << major << "." << minor;
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        kWarning(1212) << "Error occurred while creating context " << error;
        return false;
    }
    return true;
}

bool SceneOpenGL::initBuffer()
{
    return false;
}

bool SceneOpenGL::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT|EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      EGL_OPENGL_ES2_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    eglChooseConfig(dpy, config_attribs, configs, 1024, &count);

    EGLint visualId = XVisualIDFromVisual((Visual*)QX11Info::appVisual());

    config = configs[0];
    for (int i = 0; i < count; i++) {
        EGLint val;
        eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &val);
        if (visualId == val) {
            config = configs[i];
            break;
        }
    }
    return true;
}

bool SceneOpenGL::initDrawableConfigs()
{
    return false;
}

// the entry function for painting
int SceneOpenGL::paint(QRegion damage, ToplevelList toplevels)
{
    if (!m_lastDamage.isEmpty())
        flushBuffer(m_lastMask, m_lastDamage);
    m_renderTimer.start();

    foreach (Toplevel * c, toplevels) {
        assert(windows.contains(c));
        stacking_order.append(windows[ c ]);
    }

    XSync(display(), false);
    int mask = 0;
    paintScreen(&mask, &damage);   // call generic implementation
    m_lastMask = mask;
    m_lastDamage = damage;
    glFlush();

    if (m_overlayWindow->window())  // show the window only after the first pass, since
        m_overlayWindow->show();   // that pass may take long

    // do cleanup
    stacking_order.clear();
    checkGLError("PostPaint");
    return m_renderTimer.elapsed();
}

void SceneOpenGL::waitSync()
{
    // not used in EGL
}

void SceneOpenGL::flushBuffer(int mask, QRegion damage)
{
    if (mask & PAINT_SCREEN_REGION && surfaceHasSubPost && eglPostSubBufferNV) {
        QRect damageRect = damage.boundingRect();

        eglPostSubBufferNV(dpy, surface, damageRect.left(), displayHeight() - damageRect.bottom() - 1, damageRect.width(), damageRect.height());
    } else {
        eglSwapBuffers(dpy, surface);
    }
    eglWaitGL();
    // TODO: remove for wayland
    XFlush(display());
}

void SceneOpenGL::screenGeometryChanged(const QSize &size)
{
    glViewport(0,0, size.width(), size.height());
    Scene::screenGeometryChanged(size);
    ShaderManager::instance()->resetAllShaders();
}

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::TexturePrivate::TexturePrivate()
{
    m_target = GL_TEXTURE_2D;
    m_image = EGL_NO_IMAGE_KHR;
}

SceneOpenGL::TexturePrivate::~TexturePrivate()
{
    if (m_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(dpy, m_image);
    }
}

void SceneOpenGL::Texture::findTarget()
{
    Q_D(Texture);
    d->m_target = GL_TEXTURE_2D;
}

bool SceneOpenGL::Texture::load(const Pixmap& pix, const QSize& size,
                                int depth, QRegion region)
{
    // decrease the reference counter for the old texture
    d_ptr = new TexturePrivate();

    Q_D(Texture);
    Q_UNUSED(depth)
    Q_UNUSED(region)

    if (pix == None)
        return false;

    glGenTextures(1, &d->m_texture);
    setWrapMode(GL_CLAMP_TO_EDGE);
    setFilter(GL_LINEAR);
    bind();
    const EGLint attribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    d->m_image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                          (EGLClientBuffer)pix, attribs);

    if (EGL_NO_IMAGE_KHR == d->m_image) {
        kDebug(1212) << "failed to create egl image";
        unbind();
        discard();
        return false;
    }
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)d->m_image);
    unbind();
    checkGLError("load texture");
    setYInverted(true);
    d->m_size = size;
    return true;
}

void SceneOpenGL::TexturePrivate::onDamage()
{
    if (options->isGlStrictBinding()) {
        // This is just implemented to be consistent with
        // the example in mesa/demos/src/egl/opengles1/texture_from_pixmap.c
        eglWaitNative(EGL_CORE_NATIVE_ENGINE);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES) m_image);
    }
    GLTexturePrivate::onDamage();
}
