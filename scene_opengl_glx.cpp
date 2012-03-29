/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

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

// This file is included in scene_opengl.cpp

// the configs used for the destination
GLXFBConfig SceneOpenGL::fbcbuffer_db;
GLXFBConfig SceneOpenGL::fbcbuffer_nondb;
// the configs used for windows
SceneOpenGL::FBConfigInfo SceneOpenGL::fbcdrawableinfo[ 32 + 1 ];
// GLX content
GLXContext SceneOpenGL::ctxbuffer;
GLXContext SceneOpenGL::ctxdrawable;
// the destination drawable where the compositing is done
GLXDrawable SceneOpenGL::glxbuffer = None;
GLXDrawable SceneOpenGL::last_pixmap = None;

SceneOpenGL::SceneOpenGL(Workspace* ws)
    : Scene(ws)
    , m_resetModelViewProjectionMatrix(true)
    , init_ok(false)
{
    initGLX();
    // check for FBConfig support
    if (!hasGLExtension("GLX_SGIX_fbconfig") || !glXGetFBConfigAttrib || !glXGetFBConfigs ||
            !glXGetVisualFromFBConfig || !glXCreatePixmap || !glXDestroyPixmap ||
            !glXCreateWindow || !glXDestroyWindow) {
        kError(1212) << "GLX_SGIX_fbconfig or required GLX functions missing";
        return; // error
    }
    if (!selectMode())
        return; // error
    if (!initBuffer())  // create destination buffer
        return; // error
    if (!initRenderingContext())
        return; // error
    // Initialize OpenGL
    GLPlatform *glPlatform = GLPlatform::instance();
    glPlatform->detect();
    glPlatform->printResults();
    initGL();

    if (glPlatform->isSoftwareEmulation()) {
        kError(1212) << "OpenGL Software Rasterizer detected. Falling back to XRender.";
        QTimer::singleShot(0, Workspace::self(), SLOT(fallbackToXRenderCompositing()));
        return;
    }
    if (!hasGLExtension("GL_ARB_texture_non_power_of_two")
            && !hasGLExtension("GL_ARB_texture_rectangle")) {
        kError(1212) << "GL_ARB_texture_non_power_of_two and GL_ARB_texture_rectangle missing";
        return; // error
    }
    if (glPlatform->isMesaDriver() && glPlatform->mesaVersion() < kVersionNumber(7, 10)) {
        kError(1212) << "KWin requires at least Mesa 7.10 for OpenGL compositing.";
        return;
    }
    if (db)
        glDrawBuffer(GL_BACK);
    // Check whether certain features are supported
    has_waitSync = false;
    if (options->isGlVSync()) {
        if (glXGetVideoSync && glXSwapInterval && glXIsDirect(display(), ctxbuffer)) {
            unsigned int sync;
            if (glXGetVideoSync(&sync) == 0) {
                if (glXWaitVideoSync(1, 0, &sync) == 0) {
                    // NOTICE at this time we should actually check whether we can successfully
                    // deactivate the swapInterval "glXSwapInterval(0) == 0"
                    // (because we don't actually want it active unless we explicitly run a glXSwapBuffers)
                    // However mesa/dri will return a range error (6) because deactivating the
                    // swapinterval (as of today) seems completely unsupported
                    has_waitSync = true;
                    glXSwapInterval(0);
                }
                else
                    qWarning() << "NO VSYNC! glXWaitVideoSync(1,0,&uint) isn't 0 but" << glXWaitVideoSync(1, 0, &sync);
            } else
                qWarning() << "NO VSYNC! glXGetVideoSync(&uint) isn't 0 but" << glXGetVideoSync(&sync);
        } else
            qWarning() << "NO VSYNC! glXGetVideoSync, glXSwapInterval, glXIsDirect" <<
                        bool(glXGetVideoSync) << bool(glXSwapInterval) << glXIsDirect(display(), ctxbuffer);
    }

    debug = qstrcmp(qgetenv("KWIN_GL_DEBUG"), "1") == 0;

    // scene shader setup
    if (GLPlatform::instance()->supports(GLSL)) {
        if (!ShaderManager::instance()->isValid()) {
            kDebug(1212) << "No Scene Shaders available";
        } else {
            // push one shader on the stack so that one is always bound
            // consistency with GLES
            ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
        }
    }

    // OpenGL scene setup
    setupModelViewProjectionMatrix();
    if (checkGLError("Init")) {
        kError(1212) << "OpenGL compositing setup failed";
        return; // error
    }

    // set strict binding
    if (options->isGlStrictBindingFollowsDriver()) {
        options->setGlStrictBinding(!glPlatform->supports(LooseBinding));
    }
    kDebug(1212) << "DB:" << db << ", Direct:" << bool(glXIsDirect(display(), ctxbuffer)) << endl;
    init_ok = true;
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
    glXMakeCurrent(display(), None, NULL);
    glXDestroyContext(display(), ctxbuffer);
    if (m_overlayWindow->window()) {
        if (hasGLXVersion(1, 3))
            glXDestroyWindow(display(), glxbuffer);
        XDestroyWindow(display(), buffer);
        m_overlayWindow->destroy();
    } else {
        glXDestroyPixmap(display(), glxbuffer);
        XFreeGC(display(), gcroot);
        XFreePixmap(display(), buffer);
    }
    SceneOpenGL::EffectFrame::cleanup();
    checkGLError("Cleanup");
}

void SceneOpenGL::setupModelViewProjectionMatrix()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float fovy = 60.0f;
    float aspect = 1.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    float ymax = zNear * tan(fovy  * M_PI / 360.0f);
    float ymin = -ymax;
    float xmin =  ymin * aspect;
    float xmax = ymax * aspect;
    // swap top and bottom to have OpenGL coordinate system match X system
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float scaleFactor = 1.1 * tan(fovy * M_PI / 360.0f) / ymax;
    glTranslatef(xmin * scaleFactor, ymax * scaleFactor, -1.1);
    glScalef((xmax - xmin)*scaleFactor / displayWidth(), -(ymax - ymin)*scaleFactor / displayHeight(), 0.001);
    m_resetModelViewProjectionMatrix = false;
}

bool SceneOpenGL::initTfp()
{
    if (glXBindTexImageEXT == NULL || glXReleaseTexImageEXT == NULL)
        return false;
    return true;
}

bool SceneOpenGL::initRenderingContext()
{
    bool direct_rendering = options->isGlDirect();
    KXErrorHandler errs1;
    ctxbuffer = glXCreateNewContext(display(), fbcbuffer, GLX_RGBA_TYPE, NULL,
                                    direct_rendering ? GL_TRUE : GL_FALSE);
    bool failed = (ctxbuffer == NULL || !glXMakeCurrent(display(), glxbuffer, ctxbuffer));
    if (errs1.error(true))    // always check for error( having it all in one if () could skip
        failed = true;       // it due to evaluation short-circuiting
    if (failed) {
        if (!direct_rendering) {
            kDebug(1212).nospace() << "Couldn't initialize rendering context ("
                                   << KXErrorHandler::errorMessage(errs1.errorEvent()) << ")";
            return false;
        }
        glXMakeCurrent(display(), None, NULL);
        if (ctxbuffer != NULL)
            glXDestroyContext(display(), ctxbuffer);
        direct_rendering = false; // try again
        KXErrorHandler errs2;
        ctxbuffer = glXCreateNewContext(display(), fbcbuffer, GLX_RGBA_TYPE, NULL, GL_FALSE);
        bool failed = (ctxbuffer == NULL || !glXMakeCurrent(display(), glxbuffer, ctxbuffer));
        if (errs2.error(true))
            failed = true;
        if (failed) {
            kDebug(1212).nospace() << "Couldn't initialize rendering context ("
                                   << KXErrorHandler::errorMessage(errs2.errorEvent()) << ")";
            return false;
        }
    }
    return true;
}

// create destination buffer
bool SceneOpenGL::initBuffer()
{
    if (!initBufferConfigs())
        return false;
    if (fbcbuffer_db != NULL && m_overlayWindow->create()) {
        // we have overlay, try to create double-buffered window in it
        fbcbuffer = fbcbuffer_db;
        XVisualInfo* visual = glXGetVisualFromFBConfig(display(), fbcbuffer);
        XSetWindowAttributes attrs;
        attrs.colormap = XCreateColormap(display(), rootWindow(), visual->visual, AllocNone);
        buffer = XCreateWindow(display(), m_overlayWindow->window(), 0, 0, displayWidth(), displayHeight(),
                               0, visual->depth, InputOutput, visual->visual, CWColormap, &attrs);
        if (hasGLXVersion(1, 3))
            glxbuffer = glXCreateWindow(display(), fbcbuffer, buffer, NULL);
        else
            glxbuffer = buffer;
        m_overlayWindow->setup(buffer);
        db = true;
        XFree(visual);
    } else if (fbcbuffer_nondb != NULL) {
        // cannot get any double-buffered drawable, will double-buffer using a pixmap
        fbcbuffer = fbcbuffer_nondb;
        XVisualInfo* visual = glXGetVisualFromFBConfig(display(), fbcbuffer);
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC(display(), rootWindow(), GCSubwindowMode, &gcattr);
        buffer = XCreatePixmap(display(), rootWindow(), displayWidth(), displayHeight(),
                               visual->depth);
        glxbuffer = glXCreatePixmap(display(), fbcbuffer, buffer, NULL);
        db = false;
        XFree(visual);
    } else {
        kError(1212) << "Couldn't create output buffer (failed to create overlay window?) !";
        return false; // error
    }
    int vis_buffer;
    glXGetFBConfigAttrib(display(), fbcbuffer, GLX_VISUAL_ID, &vis_buffer);
    XVisualInfo* visinfo_buffer = glXGetVisualFromFBConfig(display(), fbcbuffer);
    kDebug(1212) << "Buffer visual (depth " << visinfo_buffer->depth << "): 0x" << QString::number(vis_buffer, 16);
    XFree(visinfo_buffer);
    return true;
}

// choose the best configs for the destination buffer
bool SceneOpenGL::initBufferConfigs()
{
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs(display(), DefaultScreen(display()), &cnt);
    fbcbuffer_db = NULL;
    fbcbuffer_nondb = NULL;

    for (int i = 0; i < 2; i++) {
        int back, stencil, depth, caveat, alpha;
        back = i > 0 ? INT_MAX : 1;
        stencil = INT_MAX;
        depth = INT_MAX;
        caveat = INT_MAX;
        alpha = 0;
        for (int j = 0; j < cnt; j++) {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig(display(), fbconfigs[ j ]);
            if (vi == NULL)
                continue;
            visual_depth = vi->depth;
            XFree(vi);
            if (visual_depth != DefaultDepth(display(), DefaultScreen(display())))
                continue;
            int value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_ALPHA_SIZE, &alpha);
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_BUFFER_SIZE, &value);
            if (value != visual_depth && (value - alpha) != visual_depth)
                continue;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_RENDER_TYPE, &value);
            if (!(value & GLX_RGBA_BIT))
                continue;
            int back_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_DOUBLEBUFFER, &back_value);
            if (i > 0) {
                if (back_value > back)
                    continue;
            } else {
                if (back_value < back)
                    continue;
            }
            int stencil_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_STENCIL_SIZE, &stencil_value);
            if (stencil_value > stencil)
                continue;
            int depth_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_DEPTH_SIZE, &depth_value);
            if (depth_value > depth)
                continue;
            int caveat_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_CONFIG_CAVEAT, &caveat_value);
            if (caveat_value > caveat)
                continue;
            back = back_value;
            stencil = stencil_value;
            depth = depth_value;
            caveat = caveat_value;
            if (i > 0)
                fbcbuffer_nondb = fbconfigs[ j ];
            else
                fbcbuffer_db = fbconfigs[ j ];
        }
    }
    if (cnt)
        XFree(fbconfigs);
    if (fbcbuffer_db == NULL && fbcbuffer_nondb == NULL) {
        kError(1212) << "Couldn't find framebuffer configuration for buffer!";
        return false;
    }
    for (int i = 0; i <= 32; i++) {
        if (fbcdrawableinfo[ i ].fbconfig == NULL)
            continue;
        int vis_drawable = 0;
        glXGetFBConfigAttrib(display(), fbcdrawableinfo[ i ].fbconfig, GLX_VISUAL_ID, &vis_drawable);
        kDebug(1212) << "Drawable visual (depth " << i << "): 0x" << QString::number(vis_drawable, 16);
    }
    return true;
}

// make a list of the best configs for windows by depth
bool SceneOpenGL::initDrawableConfigs()
{
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs(display(), DefaultScreen(display()), &cnt);

    for (int i = 0; i <= 32; i++) {
        int back, stencil, depth, caveat, alpha, mipmap, rgba;
        back = INT_MAX;
        stencil = INT_MAX;
        depth = INT_MAX;
        caveat = INT_MAX;
        mipmap = 0;
        rgba = 0;
        fbcdrawableinfo[ i ].fbconfig = NULL;
        fbcdrawableinfo[ i ].bind_texture_format = 0;
        fbcdrawableinfo[ i ].texture_targets = 0;
        fbcdrawableinfo[ i ].y_inverted = 0;
        fbcdrawableinfo[ i ].mipmap = 0;
        for (int j = 0; j < cnt; j++) {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig(display(), fbconfigs[ j ]);
            if (vi == NULL)
                continue;
            visual_depth = vi->depth;
            XFree(vi);
            if (visual_depth != i)
                continue;
            int value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_ALPHA_SIZE, &alpha);
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_BUFFER_SIZE, &value);
            if (value != i && (value - alpha) != i)
                continue;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_RENDER_TYPE, &value);
            if (!(value & GLX_RGBA_BIT))
                continue;
            value = 0;
            if (i == 32) {
                glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                     GLX_BIND_TO_TEXTURE_RGBA_EXT, &value);
                if (value) {
                    // TODO I think this should be set only after the config passes all tests
                    rgba = 1;
                    fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGBA_EXT;
                }
            }
            if (!value) {
                if (rgba)
                    continue;
                glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                     GLX_BIND_TO_TEXTURE_RGB_EXT, &value);
                if (!value)
                    continue;
                fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGB_EXT;
            }
            int back_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_DOUBLEBUFFER, &back_value);
            if (back_value > back)
                continue;
            int stencil_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_STENCIL_SIZE, &stencil_value);
            if (stencil_value > stencil)
                continue;
            int depth_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_DEPTH_SIZE, &depth_value);
            if (depth_value > depth)
                continue;
            int mipmap_value = -1;
            if (GLTexture::framebufferObjectSupported()) {
                glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                     GLX_BIND_TO_MIPMAP_TEXTURE_EXT, &mipmap_value);
                if (mipmap_value < mipmap)
                    continue;
            }
            int caveat_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_CONFIG_CAVEAT, &caveat_value);
            if (caveat_value > caveat)
                continue;
            // ok, config passed all tests, it's the best one so far
            fbcdrawableinfo[ i ].fbconfig = fbconfigs[ j ];
            caveat = caveat_value;
            back = back_value;
            stencil = stencil_value;
            depth = depth_value;
            mipmap = mipmap_value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_BIND_TO_TEXTURE_TARGETS_EXT, &value);
            fbcdrawableinfo[ i ].texture_targets = value;
            glXGetFBConfigAttrib(display(), fbconfigs[ j ],
                                 GLX_Y_INVERTED_EXT, &value);
            fbcdrawableinfo[ i ].y_inverted = value;
            fbcdrawableinfo[ i ].mipmap = mipmap;
        }
    }
    if (cnt)
        XFree(fbconfigs);
    if (fbcdrawableinfo[ DefaultDepth(display(), DefaultScreen(display()))].fbconfig == NULL) {
        kError(1212) << "Couldn't find framebuffer configuration for default depth!";
        return false;
    }
    if (fbcdrawableinfo[ 32 ].fbconfig == NULL) {
        kError(1212) << "Couldn't find framebuffer configuration for depth 32 (no ARGB GLX visual)!";
        return false;
    }
    return true;
}

// the entry function for painting
int SceneOpenGL::paint(QRegion damage, ToplevelList toplevels)
{
    if (!m_lastDamage.isEmpty())
        flushBuffer(m_lastMask, m_lastDamage);

    // actually paint the frame, flushed with the NEXT frame
    foreach (Toplevel * c, toplevels) {
        assert(windows.contains(c));
        stacking_order.append(windows[ c ]);
    }

    grabXServer();
    glXWaitX();
    if (m_resetModelViewProjectionMatrix) {
        // reset model view projection matrix if required
        setupModelViewProjectionMatrix();
    }
    int mask = 0;
#ifdef CHECK_GL_ERROR
    checkGLError("Paint1");
#endif
    paintScreen(&mask, &damage);   // call generic implementation
    m_lastMask = mask;
    m_lastDamage = damage;
#ifdef CHECK_GL_ERROR
    checkGLError("Paint2");
#endif

    glFlush();

    ungrabXServer();
    if (m_overlayWindow->window())  // show the window only after the first pass,
        m_overlayWindow->show();   // since that pass may take long

    // do cleanup
    stacking_order.clear();
    checkGLError("PostPaint");
    return m_renderTimer.elapsed();
}

#define VSYNC_DEBUG 0

// wait for vblank signal before painting
void SceneOpenGL::waitSync()
{
    // NOTE that vsync has no effect with indirect rendering
    if (waitSyncAvailable()) {
#if VSYNC_DEBUG
        m_renderTimer.start();
#endif
        uint sync;
#if 0
        // TODO: why precisely is this important?
        // the sync counter /can/ perform multiple steps during glXGetVideoSync & glXWaitVideoSync
        // but this only leads to waiting for two frames??!?
        glXGetVideoSync(&sync);
        glXWaitVideoSync(2, (sync + 1) % 2, &sync);
#endif
        glXWaitVideoSync(1, 0, &sync);
#if VSYNC_DEBUG
        static int waitTime = 0, waitCounter = 0, crapCounter = 0;
        if (m_renderTimer.elapsed() > 11)
            ++crapCounter;
        waitTime += m_renderTimer.elapsed();
        ++waitCounter;
        if (waitCounter > 99)
        {
            qDebug() << "mean vsync wait time:" << float((float)waitTime / (float)waitCounter) << crapCounter << "/100";
            crapCounter = waitTime = waitCounter = 0;
        }
#endif
    }
    m_renderTimer.start(); // yes, the framerate shall be constant anyway.
}

#undef VSYNC_DEBUG

// actually paint to the screen (double-buffer swap or copy from pixmap buffer)
void SceneOpenGL::flushBuffer(int mask, QRegion damage)
{
    if (db) {
        if (mask & PAINT_SCREEN_REGION) {
            waitSync();
            if (glXCopySubBuffer) {
                foreach (const QRect & r, damage.rects()) {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    glXCopySubBuffer(display(), glxbuffer, r.x(), y, r.width(), r.height());
                }
            } else {
                // if a shader is bound or the texture unit is enabled, copy pixels results in a black screen
                // therefore unbind the shader and restore after copying the pixels
                GLint shader = 0;
                if (ShaderManager::instance()->isShaderBound()) {
                    glGetIntegerv(GL_CURRENT_PROGRAM, &shader);
                    glUseProgram(0);
                }
                bool reenableTexUnit = false;
                if (glIsEnabled(GL_TEXTURE_2D)) {
                    glDisable(GL_TEXTURE_2D);
                    reenableTexUnit = true;
                }
                // no idea why glScissor() is used, but Compiz has it and it doesn't seem to hurt
                glEnable(GL_SCISSOR_TEST);
                glDrawBuffer(GL_FRONT);
                int xpos = 0;
                int ypos = 0;
                foreach (const QRect & r, damage.rects()) {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    // Move raster position relatively using glBitmap() rather
                    // than using glRasterPos2f() - the latter causes drawing
                    // artefacts at the bottom screen edge with some gfx cards
//                    glRasterPos2f( r.x(), r.y() + r.height());
                    glBitmap(0, 0, 0, 0, r.x() - xpos, y - ypos, NULL);
                    xpos = r.x();
                    ypos = y;
                    glScissor(r.x(), y, r.width(), r.height());
                    glCopyPixels(r.x(), y, r.width(), r.height(), GL_COLOR);
                }
                glBitmap(0, 0, 0, 0, -xpos, -ypos, NULL);   // move position back to 0,0
                glDrawBuffer(GL_BACK);
                glDisable(GL_SCISSOR_TEST);
                if (reenableTexUnit) {
                    glEnable(GL_TEXTURE_2D);
                }
                // rebind previously bound shader
                if (ShaderManager::instance()->isShaderBound()) {
                    glUseProgram(shader);
                }
            }
        } else {
            if (glXSwapInterval) {
                glXSwapInterval(1);
                glXSwapBuffers(display(), glxbuffer);
                glXSwapInterval(0);
                m_renderTimer.start(); // this is important so we don't assume to be loosing frames in the compositor timing calculation
            } else {
                waitSync();
                glXSwapBuffers(display(), glxbuffer);
            }
        }
        glXWaitGL();
    } else {
        glXWaitGL();
        if (mask & PAINT_SCREEN_REGION)
            foreach (const QRect & r, damage.rects())
                XCopyArea(display(), buffer, rootWindow(), gcroot, r.x(), r.y(), r.width(), r.height(), r.x(), r.y());
        else
            XCopyArea(display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0);
    }
    XFlush(display());
}

void SceneOpenGL::screenGeometryChanged(const QSize &size)
{
    Scene::screenGeometryChanged(size);
    glViewport(0,0, size.width(), size.height());
    if (m_overlayWindow->window() == None) {
        glXMakeCurrent(display(), None, NULL);
        glXDestroyPixmap(display(), glxbuffer);
        XFreePixmap(display(), buffer);
        XVisualInfo* visual = glXGetVisualFromFBConfig(display(), fbcbuffer);
        buffer = XCreatePixmap(display(), rootWindow(), size.width(), size.height(), visual->depth);
        XFree(visual);
        glxbuffer = glXCreatePixmap(display(), fbcbuffer, buffer, NULL);
        glXMakeCurrent(display(), glxbuffer, ctxbuffer);
        // TODO: there seems some bug, some clients become black until an eg. un/remap - could be a general pixmap buffer issue, though
    }
    else {
        glXMakeCurrent(display(), None, NULL); // deactivate context ////
        XMoveResizeWindow(display(), buffer, 0,0, size.width(), size.height());
        m_overlayWindow->setup(buffer);
        XSync(display(), false);  // ensure X11 stuff has applied ////
        glXMakeCurrent(display(), glxbuffer, ctxbuffer); // reactivate context ////
        glViewport(0,0, size.width(), size.height()); // adjust viewport last - should btw. be superflous on the Pixmap buffer - iirc glXCreatePixmap sets the context anyway. ////
    }
    ShaderManager::instance()->resetAllShaders();
    m_resetModelViewProjectionMatrix = true;
}

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::TexturePrivate::TexturePrivate()
{
    m_glxpixmap = None;
}

SceneOpenGL::TexturePrivate::~TexturePrivate()
{
    if (m_glxpixmap != None) {
        if (!options->isGlStrictBinding()) {
            glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        }
        glXDestroyPixmap(display(), m_glxpixmap);
        m_glxpixmap = None;
    }
}

void SceneOpenGL::Texture::findTarget()
{
    Q_D(Texture);
    unsigned int new_target = 0;
    if (glXQueryDrawable && d->m_glxpixmap != None)
        glXQueryDrawable(display(), d->m_glxpixmap, GLX_TEXTURE_TARGET_EXT, &new_target);
    // HACK: this used to be a hack for Xgl.
    // without this hack the NVIDIA blob aborts when trying to bind a texture from
    // a pixmap icon
    if (new_target == 0) {
        if (NPOTTextureSupported() ||
            (isPowerOfTwo(d->m_size.width()) && isPowerOfTwo(d->m_size.height()))) {
            new_target = GLX_TEXTURE_2D_EXT;
        } else {
            new_target = GLX_TEXTURE_RECTANGLE_EXT;
        }
    }
    switch(new_target) {
    case GLX_TEXTURE_2D_EXT:
        d->m_target = GL_TEXTURE_2D;
        d->m_scale.setWidth(1.0f / d->m_size.width());
        d->m_scale.setHeight(1.0f / d->m_size.height());
        break;
    case GLX_TEXTURE_RECTANGLE_EXT:
        d->m_target = GL_TEXTURE_RECTANGLE_ARB;
        d->m_scale.setWidth(1.0f);
        d->m_scale.setHeight(1.0f);
        break;
    default:
        abort();
    }
}

bool SceneOpenGL::Texture::load(const Pixmap& pix, const QSize& size,
                                int depth, QRegion region)
{
    Q_UNUSED(region)
    // decrease the reference counter for the old texture
    d_ptr = new TexturePrivate();

    Q_D(Texture);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad1");
#endif
    if (pix == None || size.isEmpty() || depth < 1)
        return false;
    if (fbcdrawableinfo[ depth ].fbconfig == NULL) {
        kDebug(1212) << "No framebuffer configuration for depth " << depth
                     << "; not binding pixmap" << endl;
        return false;
    }

    d->m_size = size;
    // new texture, or texture contents changed; mipmaps now invalid
    setDirty();

#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad2");
#endif
    // tfp mode, simply bind the pixmap to texture
    glGenTextures(1, &d->m_texture);
    // The GLX pixmap references the contents of the original pixmap, so it doesn't
    // need to be recreated when the contents change.
    // The texture may or may not use the same storage depending on the EXT_tfp
    // implementation. When options->glStrictBinding is true, the texture uses
    // a different storage and needs to be updated with a call to
    // glXBindTexImageEXT() when the contents of the pixmap has changed.
    int attrs[] = {
        GLX_TEXTURE_FORMAT_EXT, fbcdrawableinfo[ depth ].bind_texture_format,
        GLX_MIPMAP_TEXTURE_EXT, fbcdrawableinfo[ depth ].mipmap,
        None, None, None
    };
    // Specifying the texture target explicitly is reported to cause a performance
    // regression with R300G (see bug #256654).
    if (GLPlatform::instance()->driver() != Driver_R300G) {
        if ((fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_2D_BIT_EXT) &&
                (GLTexture::NPOTTextureSupported() ||
                  (isPowerOfTwo(size.width()) && isPowerOfTwo(size.height())))) {
            attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
            attrs[ 5 ] = GLX_TEXTURE_2D_EXT;
        } else if (fbcdrawableinfo[ depth ].texture_targets & GLX_TEXTURE_RECTANGLE_BIT_EXT) {
            attrs[ 4 ] = GLX_TEXTURE_TARGET_EXT;
            attrs[ 5 ] = GLX_TEXTURE_RECTANGLE_EXT;
        }
    }
    d->m_glxpixmap = glXCreatePixmap(display(), fbcdrawableinfo[ depth ].fbconfig, pix, attrs);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoadTFP1");
#endif
    findTarget();
    d->m_yInverted = fbcdrawableinfo[ depth ].y_inverted ? true : false;
    d->m_canUseMipmaps = fbcdrawableinfo[ depth ].mipmap ? true : false;
    glBindTexture(d->m_target, d->m_texture);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoadTFP2");
#endif
    glXBindTexImageEXT(display(), d->m_glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
#ifdef CHECK_GL_ERROR
    checkGLError("TextureLoad0");
#endif
    unbind();
    return true;
}

void SceneOpenGL::TexturePrivate::onDamage()
{
    if (options->isGlStrictBinding() && m_glxpixmap) {
        glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
        glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
    }
    GLTexturePrivate::onDamage();
}
