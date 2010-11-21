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

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    , init_ok( false )
    , selfCheckDone( true )
    , m_sceneShader( NULL )
    {
    // TODO: EGL

    debug = qstrcmp( qgetenv( "KWIN_GL_DEBUG" ), "1" ) == 0;

    m_sceneShader = new GLShader( ":/resources/scene-vertex.glsl", ":/resources/scene-fragment.glsl" );
    if( m_sceneShader->isValid() )
        {
        m_sceneShader->bind();
        m_sceneShader->setUniform( "sample", 0 );
        m_sceneShader->setUniform( "displaySize", QVector2D(displayWidth(), displayHeight()));
        m_sceneShader->setUniform( "debug", debug ? 1 : 0 );
        m_sceneShader->bindAttributeLocation( 0, "vertex" );
        m_sceneShader->bindAttributeLocation( 1, "texCoord" );
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
    return false;
    }

bool SceneOpenGL::initBuffer()
    {
    return false;
    }

bool SceneOpenGL::initBufferConfigs()
    {
    return false;
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
    int mask = 0;
    paintScreen( &mask, &damage ); // call generic implementation
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
    // TODO: implement me
    }

void SceneOpenGL::paintGenericScreen( int mask, ScreenPaintData data )
    {
    // TODO: setup shader for transformed geometry
    Scene::paintGenericScreen( mask, data );
    }

void SceneOpenGL::paintBackground( QRegion region )
    {
    // TODO: implement me
    }

//****************************************
// SceneOpenGL::Texture
//****************************************

void SceneOpenGL::Texture::init()
    {
    damaged = true;
    }

void SceneOpenGL::Texture::release()
    {
    }

void SceneOpenGL::Texture::findTarget()
    {
    mTarget = GL_TEXTURE_2D;
    }

bool SceneOpenGL::Texture::load( const Pixmap& pix, const QSize& size,
    int depth, QRegion region )
    {
    // TODO: implement proper 
    return GLTexture::load(QPixmap::fromX11Pixmap(pix));
    }

void SceneOpenGL::Texture::bind()
    {
    GLTexture::bind();
    }

void SceneOpenGL::Texture::unbind()
    {
    GLTexture::unbind();
    }
