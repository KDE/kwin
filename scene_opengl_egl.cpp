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

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    , init_ok( false )
    , selfCheckDone( true )
    , m_sceneShader( NULL )
    {
    if( !initRenderingContext() )
        return;

    initGL();
    debug = qstrcmp( qgetenv( "KWIN_GL_DEBUG" ), "1" ) == 0;

    m_sceneShader = new GLShader( ":/resources/scene-vertex.glsl", ":/resources/scene-fragment.glsl" );
    if( m_sceneShader->isValid() )
        {
        m_sceneShader->bind();
        m_sceneShader->setUniform( "sample", 0 );
        m_sceneShader->setUniform( "displaySize", QVector2D(displayWidth(), displayHeight()));
        m_sceneShader->setUniform( "debug", debug ? 1 : 0 );
        m_sceneShader->unbind();
        kDebug(1212) << "Scene Shader is valid";
        }
    else
        {
        delete m_sceneShader;
        m_sceneShader = NULL;
        kDebug(1212) << "Scene Shader is not valid";
        return;
        }

    if( checkGLError( "Init" ))
        {
        kError( 1212 ) << "OpenGL compositing setup failed";
        return; // error
        }
    init_ok = true;
    }

SceneOpenGL::~SceneOpenGL()
    {
    foreach( Window* w, windows )
        delete w;
    // do cleanup after initBuffer()
    eglMakeCurrent( dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroyContext( dpy, ctx );
    eglDestroySurface( dpy, surface );
    eglTerminate( dpy );
    eglReleaseThread();
    delete m_sceneShader;
    SceneOpenGL::EffectFrame::cleanup();
    checkGLError( "Cleanup" );
    }

bool SceneOpenGL::initTfp()
    {
    return false;
    }

bool SceneOpenGL::initRenderingContext()
    {
    dpy = eglGetDisplay( display() );
    if( dpy == EGL_NO_DISPLAY )
        return false;
    EGLint major, minor;
    if( eglInitialize( dpy, &major, &minor ) == EGL_FALSE )
        return false;
    eglBindAPI( EGL_OPENGL_ES_API );
    initBufferConfigs();
    if( !wspace->createOverlay() )
        {
        kError( 1212 ) << "Could not get overlay window";
        return false;
        }
    surface = eglCreateWindowSurface( dpy, config, wspace->overlayWindow(), 0 );

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    ctx = eglCreateContext( dpy, config, EGL_NO_CONTEXT, context_attribs );
    if( ctx == EGL_NO_CONTEXT )
        return false;
    if( eglMakeCurrent( dpy, surface, surface, ctx ) == EGL_FALSE )
        return false;
    kDebug( 1212 ) << "EGL version: " << major << "." << minor;
    EGLint error = eglGetError();
    if( error != EGL_SUCCESS )
        {
        kWarning( 1212 ) << "Error occurred while creating context " << error;
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
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
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
    for (int i = 0; i < count; i++)
        {
        EGLint val;
        eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &val);
        if (visualId == val)
            {
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

void SceneOpenGL::selfCheckSetup()
    {
    // not used in EGL
    }

bool SceneOpenGL::selfCheckFinish()
    {
    // not used in EGL
    return true;
    }

// the entry function for painting
void SceneOpenGL::paint( QRegion damage, ToplevelList toplevels )
    {
    QTime t = QTime::currentTime();
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( windows[ c ] );
        }
    grabXServer();
    int mask = 0;
    paintScreen( &mask, &damage ); // call generic implementation
    ungrabXServer(); // ungrab before flushBuffer(), it may wait for vsync
    if( wspace->overlayWindow()) // show the window only after the first pass, since
        wspace->showOverlay();   // that pass may take long
    lastRenderTime = t.elapsed();
    flushBuffer( mask, damage );
    // do cleanup
    stacking_order.clear();
    checkGLError( "PostPaint" );
    }

void SceneOpenGL::waitSync()
    {
    // not used in EGL
    }

void SceneOpenGL::flushBuffer( int mask, QRegion damage )
    {
    glFlush();
    if( mask & PAINT_SCREEN_REGION )
        {
        // TODO: implement me properly
        eglSwapBuffers( dpy, surface );
        }
    else
        {
        eglSwapBuffers( dpy, surface );
        }
    eglWaitGL();
    // TODO: remove for wayland
    XFlush( display());
    }

void SceneOpenGL::paintGenericScreen( int mask, ScreenPaintData data )
    {
    // TODO: setup shader for transformed geometry
    Scene::paintGenericScreen( mask, data );
    }

void SceneOpenGL::paintBackground( QRegion region )
    {
    // TODO: implement me
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    }

//****************************************
// SceneOpenGL::Texture
//****************************************

void SceneOpenGL::Texture::init()
    {
    damaged = true;
    findTarget();
    }

void SceneOpenGL::Texture::release()
    {
    mTexture = None;
    }

void SceneOpenGL::Texture::findTarget()
    {
    mTarget = GL_TEXTURE_2D;
    }

bool SceneOpenGL::Texture::load( const Pixmap& pix, const QSize& size,
    int depth, QRegion region )
    {
    if( mTexture == None )
        {
        createTexture();
        bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const EGLint attribs[] = {
            EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
            EGL_NONE
        };
        EGLImageKHR image = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
                                (EGLClientBuffer)pix, attribs);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)image);
        eglDestroyImageKHR( dpy, image );
        unbind();
        checkGLError("load texture");
        }
    return true;
    }

void SceneOpenGL::Texture::bind()
    {
    GLTexture::bind();
    }

void SceneOpenGL::Texture::unbind()
    {
    GLTexture::unbind();
    }
