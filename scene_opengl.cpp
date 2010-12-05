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


/*
 This is the OpenGL-based compositing code. It is the primary and most powerful
 compositing backend.
 
Sources and other compositing managers:
=======================================

- http://opengl.org
    - documentation
        - OpenGL Redbook (http://opengl.org/documentation/red_book/ - note it's only version 1.1)
        - GLX docs (http://opengl.org/documentation/specs/glx/glx1.4.pdf)
        - extensions docs (http://www.opengl.org/registry/)

- glcompmgr
    - http://lists.freedesktop.org/archives/xorg/2006-July/017006.html ,
    - http://www.mail-archive.com/compiz%40lists.freedesktop.org/msg00023.html
    - simple and easy to understand
    - works even without texture_from_pixmap extension
    - claims to support several different gfx cards
    - compile with something like
      "gcc -Wall glcompmgr-0.5.c `pkg-config --cflags --libs glib-2.0` -lGL -lXcomposite -lXdamage -L/usr/X11R6/lib"

- compiz
    - git clone git://anongit.freedesktop.org/git/xorg/app/compiz
    - the ultimate <whatever>
    - glxcompmgr
        - git clone git://anongit.freedesktop.org/git/xorg/app/glxcompgr
        - a rather old version of compiz, but also simpler and as such simpler
            to understand

- beryl
    - a fork of Compiz
    - http://beryl-project.org
    - git clone git://anongit.beryl-project.org/beryl/beryl-core (or beryl-plugins etc. ,
        the full list should be at git://anongit.beryl-project.org/beryl/)

- libcm (metacity)
    - cvs -d :pserver:anonymous@anoncvs.gnome.org:/cvs/gnome co libcm
    - not much idea about it, the model differs a lot from KWin/Compiz/Beryl
    - does not seem to be very powerful or with that much development going on

*/

#include "scene_opengl.h"

#include <kxerrorhandler.h>

// TODO: use <>
#include "lib/kwinglplatform.h"

#include "utils.h"
#include "client.h"
#include "deleted.h"
#include "effects.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>

// turns on checks for opengl errors in various places (for easier finding of them)
// normally only few of them are enabled
//#define CHECK_GL_ERROR

#ifdef KWIN_HAVE_OPENGL_COMPOSITING

#include <X11/extensions/Xcomposite.h>

#include <qpainter.h>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>

namespace KWin
{

extern int currentRefreshRate();

//****************************************
// SceneOpenGL
//****************************************

bool SceneOpenGL::tfp_mode; // using glXBindTexImageEXT (texture_from_pixmap)
bool SceneOpenGL::db; // destination drawable is double-buffered
bool SceneOpenGL::shm_mode;
#ifdef HAVE_XSHM
XShmSegmentInfo SceneOpenGL::shm;
#endif

#ifdef KWIN_HAVE_OPENGLES
#include "scene_opengl_egl.cpp"
#else
#include "scene_opengl_glx.cpp"
#endif


bool SceneOpenGL::setupSceneShaders()
{
    m_sceneShader = new GLShader(":/resources/scene-vertex.glsl", ":/resources/scene-fragment.glsl");
    if (m_sceneShader->isValid()) {
        m_sceneShader->bind();
        m_sceneShader->setUniform("sample", 0);
        m_sceneShader->setUniform("displaySize", QVector2D(displayWidth(), displayHeight()));
        m_sceneShader->setUniform("debug", debug ? 1 : 0);
        m_sceneShader->unbind();
        kDebug(1212) << "Scene Shader is valid";
    }
    else {
        delete m_sceneShader;
        m_sceneShader = NULL;
        kDebug(1212) << "Scene Shader is not valid";
        return false;
    }
    m_genericSceneShader = new GLShader( ":/resources/scene-generic-vertex.glsl", ":/resources/scene-fragment.glsl" );
    if (m_genericSceneShader->isValid()) {
        m_genericSceneShader->bind();
        m_genericSceneShader->setUniform("sample", 0);
        m_genericSceneShader->setUniform("debug", debug ? 1 : 0);
        QMatrix4x4 projection;
        float fovy = 60.0f;
        float aspect = 1.0f;
        float zNear = 0.1f;
        float zFar = 100.0f;
        float ymax = zNear * tan(fovy  * M_PI / 360.0f);
        float ymin = -ymax;
        float xmin =  ymin * aspect;
        float xmax = ymax * aspect;
        projection.frustum(xmin, xmax, ymin, ymax, zNear, zFar);
        m_genericSceneShader->setUniform("projection", projection);
        QMatrix4x4 modelview;
        float scaleFactor = 1.1 * tan( fovy * M_PI / 360.0f )/ymax;
        modelview.translate(xmin*scaleFactor, ymax*scaleFactor, -1.1);
        modelview.scale((xmax-xmin)*scaleFactor/displayWidth(), -(ymax-ymin)*scaleFactor/displayHeight(), 0.001);
        m_genericSceneShader->setUniform("modelview", modelview);
        m_genericSceneShader->unbind();
        kDebug(1212) << "Generic Scene Shader is valid";
    }
    else {
        delete m_genericSceneShader;
        m_genericSceneShader = NULL;
        delete m_sceneShader;
        m_sceneShader = NULL;
        kDebug(1212) << "Generic Scene Shader is not valid";
        return false;
    }
    m_colorShader = new GLShader(":/resources/scene-color-vertex.glsl", ":/resources/scene-color-fragment.glsl");
    if (m_colorShader->isValid()) {
        m_colorShader->bind();
        m_colorShader->setUniform("displaySize", QVector2D(displayWidth(), displayHeight()));
        m_colorShader->unbind();
        kDebug(1212) << "Color Shader is valid";
    } else {
        delete m_genericSceneShader;
        m_genericSceneShader = NULL;
        delete m_sceneShader;
        m_sceneShader = NULL;
        delete m_colorShader;
        m_colorShader = NULL;
        kDebug(1212) << "Color Scene Shader is not valid";
        return false;
    }
    return true;
}

bool SceneOpenGL::initFailed() const
    {
    return !init_ok;
    }

bool SceneOpenGL::selectMode()
    {
    // select mode - try TFP first, then SHM, otherwise fallback mode
    shm_mode = false;
    tfp_mode = false;
    if( options->glMode == Options::GLTFP )
        {
        if( initTfp())
            tfp_mode = true;
        else if( initShm())
            shm_mode = true;
        }
    else if( options->glMode == Options::GLSHM )
        {
        if( initShm())
            shm_mode = true;
        else if( initTfp())
            tfp_mode = true;
        }
    if( !initDrawableConfigs())
        return false;
    return true;
    }

bool SceneOpenGL::initShm()
    {
#ifdef HAVE_XSHM
    int major, minor;
    Bool pixmaps;
    if( !XShmQueryVersion( display(), &major, &minor, &pixmaps ) || !pixmaps )
        return false;
    if( XShmPixmapFormat( display()) != ZPixmap )
        return false;
    const int MAXSIZE = 4096 * 2048 * 4; // TODO check there are not larger windows
    // TODO check that bytes_per_line doesn't involve padding?
    shm.readOnly = False;
    shm.shmid = shmget( IPC_PRIVATE, MAXSIZE, IPC_CREAT | 0600 );
    if( shm.shmid < 0 )
        return false;
    shm.shmaddr = ( char* ) shmat( shm.shmid, NULL, 0 );
    if( shm.shmaddr == ( void * ) -1 )
        {
        shmctl( shm.shmid, IPC_RMID, 0 );
        return false;
        }
#ifdef __linux__
    // mark as deleted to automatically free the memory in case
    // of a crash (but this doesn't work e.g. on Solaris ... oh well)
    shmctl( shm.shmid, IPC_RMID, 0 );
#endif
    KXErrorHandler errs;
    XShmAttach( display(), &shm );
    if( errs.error( true ))
        {
#ifndef __linux__
        shmctl( shm.shmid, IPC_RMID, 0 );
#endif
        shmdt( shm.shmaddr );
        return false;
        }
    return true;
#else
    return false;
#endif
    }

void SceneOpenGL::cleanupShm()
    {
#ifdef HAVE_XSHM
    shmdt( shm.shmaddr );
#ifndef __linux__
    shmctl( shm.shmid, IPC_RMID, 0 );
#endif
#endif
    }

// Test if compositing actually _really_ works, by creating a texture from a testing
// window, drawing it on the screen, reading the contents back and comparing. This
// should test whether compositing really works.
// This function does the whole selfcheck, it can be done also in two parts
// during actual drawing (to avoid flicker, see selfCheck() call from the ctor).
bool SceneOpenGL::selfCheck()
    {
    QRegion reg = selfCheckRegion();
    if( wspace->overlayWindow())
        { // avoid covering the whole screen too soon
        wspace->setOverlayShape( reg );
        wspace->showOverlay();
        }
    selfCheckSetup();
    flushBuffer( PAINT_SCREEN_REGION, reg );
    bool ok = selfCheckFinish();
    if( wspace->overlayWindow())
        wspace->hideOverlay();
    return ok;
    }

void SceneOpenGL::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = new Window( c );
    c->effectWindow()->setSceneWindow( windows[ c ]);
    }

void SceneOpenGL::windowClosed( Toplevel* c, Deleted* deleted )
    {
    assert( windows.contains( c ));
    if( deleted != NULL )
        { // replace c with deleted
        Window* w = windows.take( c );
        w->updateToplevel( deleted );
        windows[ deleted ] = w;
        }
    else
        {
        delete windows.take( c );
        c->effectWindow()->setSceneWindow( NULL );
        }
    }

void SceneOpenGL::windowDeleted( Deleted* c )
    {
    assert( windows.contains( c ));
    delete windows.take( c );
    c->effectWindow()->setSceneWindow( NULL );
    }

void SceneOpenGL::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid
        return;                 // by default
    Window* w = windows[ c ];
    w->discardShape();
    w->checkTextureSize();
    }

void SceneOpenGL::windowOpacityChanged( Toplevel* )
    {
#if 0 // not really needed, windows are painted on every repaint
      // and opacity is used when applying texture, not when
      // creating it
    if( !windows.contains( c )) // this is ok, texture is created
        return;                 // on demand
    Window* w = windows[ c ];
    w->discardTexture();
#endif
    }

GLShader* SceneOpenGL::sceneShader() const
    {
    return m_sceneShader;
    }

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::Texture::Texture() : GLTexture()
    {
    init();
    }

SceneOpenGL::Texture::Texture( const Pixmap& pix, const QSize& size, int depth ) : GLTexture()
    {
    init();
    load( pix, size, depth );
    }

SceneOpenGL::Texture::~Texture()
    {
    discard();
    }

void SceneOpenGL::Texture::createTexture()
    {
    glGenTextures( 1, &mTexture );
    }

void SceneOpenGL::Texture::discard()
    {
    if( mTexture != None )
        release();
    GLTexture::discard();
    }

QRegion SceneOpenGL::Texture::optimizeBindDamage( const QRegion& reg, int limit )
    {
    if( reg.rects().count() <= 1 )
        return reg;
    // try to reduce the number of rects, as especially with SHM mode every rect
    // causes X roundtrip, even for very small areas - so, when the size difference
    // between all the areas and the bounding rectangle is small, simply use
    // only the bounding rectangle
    int size = 0;
    foreach( const QRect &r, reg.rects())
        size += r.width() * r.height();
    if( reg.boundingRect().width() * reg.boundingRect().height() - size < limit )
        return reg.boundingRect();
    return reg;
    }

bool SceneOpenGL::Texture::load( const Pixmap& pix, const QSize& size,
    int depth )
    {
    return load( pix, size, depth,
        QRegion( 0, 0, size.width(), size.height()));
    }

bool SceneOpenGL::Texture::load( const QImage& image, GLenum target )
    {
    if( image.isNull())
        return false;
    return load( QPixmap::fromImage( image ), target );
    }

bool SceneOpenGL::Texture::load( const QPixmap& pixmap, GLenum target )
    {
    Q_UNUSED( target ); // SceneOpenGL::Texture::findTarget() detects the target
    if( pixmap.isNull())
        return false;
    return load( pixmap.handle(), pixmap.size(), pixmap.depth());
    }

//****************************************
// SceneOpenGL::Window
//****************************************
GLVertexBuffer* SceneOpenGL::Window::decorationVertices = NULL;

SceneOpenGL::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , texture()
    , topTexture()
    , leftTexture()
    , rightTexture()
    , bottomTexture()
    , vertexBuffer( NULL )
    {
    }

SceneOpenGL::Window::~Window()
    {
    discardTexture();
    delete vertexBuffer;
    }

// Bind the window pixmap to an OpenGL texture.
bool SceneOpenGL::Window::bindTexture()
    {
#ifndef KWIN_HAVE_OPENGLES
    if( texture.texture() != None && toplevel->damage().isEmpty())
        {
        // texture doesn't need updating, just bind it
        glBindTexture( texture.target(), texture.texture());
        return true;
        }
#endif
    // Get the pixmap with the window contents
    Pixmap pix = toplevel->windowPixmap();
    if( pix == None )
        return false;
    bool success = texture.load( pix, toplevel->size(), toplevel->depth(),
        toplevel->damage());
    if( success )
        toplevel->resetDamage( QRect( toplevel->clientPos(), toplevel->clientSize() ) );
    else
        kDebug( 1212 ) << "Failed to bind window";
    return success;
    }

void SceneOpenGL::Window::discardTexture()
    {
    texture.discard();
    topTexture.discard();
    leftTexture.discard();
    rightTexture.discard();
    bottomTexture.discard();
    }

// This call is used in SceneOpenGL::windowGeometryShapeChanged(),
// which originally called discardTexture(), however this was causing performance
// problems with the launch feedback icon - large number of texture rebinds.
// Since the launch feedback icon does not resize, only changes shape, it
// is not necessary to rebind the texture (with no strict binding), therefore
// discard the texture only if size changes.
void SceneOpenGL::Window::checkTextureSize()
    {
    if( texture.size() != size())
        discardTexture();
    }

// when the window's composite pixmap is discarded, undo binding it to the texture
void SceneOpenGL::Window::pixmapDiscarded()
    {
    texture.release();
    }

// paint the window
void SceneOpenGL::Window::performPaint( int mask, QRegion region, WindowPaintData data )
    {
    // check if there is something to paint (e.g. don't paint if the window
    // is only opaque and only PAINT_WINDOW_TRANSLUCENT is requested)
    /* HACK: It seems this causes painting glitches, disable temporarily
    bool opaque = isOpaque() && data.opacity == 1.0;
    if(( mask & PAINT_WINDOW_OPAQUE ) ^ ( mask & PAINT_WINDOW_TRANSLUCENT ))
        { // We are only painting either opaque OR translucent windows, not both
        if( mask & PAINT_WINDOW_OPAQUE && !opaque )
            return; // Only painting opaque and window is translucent
        if( mask & PAINT_WINDOW_TRANSLUCENT && opaque )
            return; // Only painting translucent and window is opaque
        }*/
    // paint only requested areas
    if( region != infiniteRegion()) // avoid integer overflow
        region.translate( -x(), -y());
    if( region.isEmpty())
        return;
    if( !bindTexture())
        return;
    // set texture filter
    if( options->glSmoothScale != 0 ) // default to yes
        {
        if( mask & PAINT_WINDOW_TRANSFORMED )
            filter = ImageFilterGood;
        else if( mask & PAINT_SCREEN_TRANSFORMED )
            filter = ImageFilterGood;
        else
            filter = ImageFilterFast;
        }
    else
        filter = ImageFilterFast;
    if( filter == ImageFilterGood )
        texture.setFilter( GL_LINEAR );
    else
        texture.setFilter( GL_NEAREST );
    // do required transformations
    int x = toplevel->x();
    int y = toplevel->y();
    double z = 0.0;
    bool sceneShader = false;
    if (!data.shader && static_cast<SceneOpenGL*>(scene)->hasSceneShader()) {
        // set the shader for uniform initialising in paint decoration
        if ((mask & PAINT_WINDOW_TRANSFORMED) || (mask & PAINT_SCREEN_TRANSFORMED)) {
            data.shader = static_cast<SceneOpenGL*>(scene)->m_genericSceneShader;
            data.shader->bind();
        } else {
            data.shader = static_cast<SceneOpenGL*>(scene)->m_sceneShader;
            data.shader->bind();
            data.shader->setUniform("geometry", QVector4D(x, y, toplevel->width(), toplevel->height()));
        }
        sceneShader = true;
    }
    if (mask & PAINT_WINDOW_TRANSFORMED) {
        x += data.xTranslate;
        y += data.yTranslate;
        z += data.zTranslate;
        QMatrix4x4 windowTransformation;
        windowTransformation.translate(x, y, z);
        if ((mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 || data.zScale != 1)) {
            windowTransformation.scale(data.xScale, data.yScale, data.zScale);
        }
        if ((mask & PAINT_WINDOW_TRANSFORMED) && data.rotation) {
            windowTransformation.translate(data.rotation->xRotationPoint, data.rotation->yRotationPoint, data.rotation->zRotationPoint);
            qreal xAxis = 0.0;
            qreal yAxis = 0.0;
            qreal zAxis = 0.0;
            switch( data.rotation->axis )
                {
                case RotationData::XAxis:
                    xAxis = 1.0;
                    break;
                case RotationData::YAxis:
                    yAxis = 1.0;
                    break;
                case RotationData::ZAxis:
                    zAxis = 1.0;
                    break;
                }
            windowTransformation.rotate(data.rotation->angle, xAxis, yAxis, zAxis);
            windowTransformation.translate(-data.rotation->xRotationPoint, -data.rotation->yRotationPoint, -data.rotation->zRotationPoint);
        }
        if (sceneShader) {
            data.shader->setUniform("windowTransformation", windowTransformation);
        }
    }
    if( !sceneShader )
        {
#ifndef KWIN_HAVE_OPENGLES
        glPushMatrix();
        glTranslatef( x, y, z );
        if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 || data.zScale != 1 ))
            glScalef( data.xScale, data.yScale, data.zScale );
        if(( mask & PAINT_WINDOW_TRANSFORMED ) && data.rotation )
            {
            glTranslatef( data.rotation->xRotationPoint, data.rotation->yRotationPoint, data.rotation->zRotationPoint );
            float xAxis = 0.0;
            float yAxis = 0.0;
            float zAxis = 0.0;
            switch( data.rotation->axis )
                {
                case RotationData::XAxis:
                    xAxis = 1.0;
                    break;
                case RotationData::YAxis:
                    yAxis = 1.0;
                    break;
                case RotationData::ZAxis:
                    zAxis = 1.0;
                    break;
                }
            glRotatef( data.rotation->angle, xAxis, yAxis, zAxis );
            glTranslatef( -data.rotation->xRotationPoint, -data.rotation->yRotationPoint, -data.rotation->zRotationPoint );
            }
#endif
        }
    region.translate( toplevel->x(), toplevel->y() );  // Back to screen coords

    WindowQuadList decoration = data.quads.select( WindowQuadDecoration );


    // decorations
    Client *client = dynamic_cast<Client*>(toplevel);
    Deleted *deleted = dynamic_cast<Deleted*>(toplevel);
    if( client || deleted )
        {
        bool noBorder = true;
        bool updateDeco = false;
        const QPixmap *left = NULL;
        const QPixmap *top = NULL;
        const QPixmap *right = NULL;
        const QPixmap *bottom = NULL;
        QRect topRect, leftRect, rightRect, bottomRect;
        if( client && !client->noBorder() )
            {
            noBorder = false;
            updateDeco = client->decorationPixmapRequiresRepaint();
            client->ensureDecorationPixmapsPainted();

            client->layoutDecorationRects(leftRect, topRect, rightRect, bottomRect, Client::WindowRelative);

            left   = client->leftDecoPixmap();
            top    = client->topDecoPixmap();
            right  = client->rightDecoPixmap();
            bottom = client->bottomDecoPixmap();
            }
        if( deleted && !deleted->noBorder() )
            {
            noBorder = false;
            left   = deleted->leftDecoPixmap();
            top    = deleted->topDecoPixmap();
            right  = deleted->rightDecoPixmap();
            bottom = deleted->bottomDecoPixmap();
            deleted->layoutDecorationRects(leftRect, topRect, rightRect, bottomRect);
            }
        if( !noBorder )
            {
            WindowQuadList topList, leftList, rightList, bottomList;

            foreach( const WindowQuad& quad, decoration )
                {
                if( topRect.contains( QPoint( quad.originalLeft(), quad.originalTop() ) ) )
                    {
                    topList.append( quad );
                    continue;
                    }
                if( bottomRect.contains( QPoint( quad.originalLeft(), quad.originalTop() ) ) )
                    {
                    bottomList.append( quad );
                    continue;
                    }
                if( leftRect.contains( QPoint( quad.originalLeft(), quad.originalTop() ) ) )
                    {
                    leftList.append( quad );
                    continue;
                    }
                if( rightRect.contains( QPoint( quad.originalLeft(), quad.originalTop() ) ) )
                    {
                    rightList.append( quad );
                    continue;
                    }
                }

            if( !SceneOpenGL::Window::decorationVertices )
                SceneOpenGL::Window::decorationVertices = new GLVertexBuffer( GLVertexBuffer::Stream );
            SceneOpenGL::Window::decorationVertices->setUseShader( sceneShader );
            paintDecoration( top, DecorationTop, region, topRect, data, topList, updateDeco );
            paintDecoration( left, DecorationLeft, region, leftRect, data, leftList, updateDeco );
            paintDecoration( right, DecorationRight, region, rightRect, data, rightList, updateDeco );
            paintDecoration( bottom, DecorationBottom, region, bottomRect, data, bottomList, updateDeco );
            }
        }

    // paint the content
    if ( !(mask & PAINT_DECORATION_ONLY) )
        {
        if( !vertexBuffer )
            vertexBuffer = new GLVertexBuffer( GLVertexBuffer::Stream );
        vertexBuffer->setUseShader( sceneShader );
        texture.bind();
        texture.enableUnnormalizedTexCoords();
        prepareStates( Content, data.opacity * data.contents_opacity, data.brightness, data.saturation, data.shader );
        renderQuads( mask, region, data.quads.select( WindowQuadContents ));
        restoreStates( Content, data.opacity * data.contents_opacity, data.brightness, data.saturation, data.shader );
        texture.disableUnnormalizedTexCoords();
        texture.unbind();
#ifndef KWIN_HAVE_OPENGLES
        if( static_cast<SceneOpenGL*>(scene)->debug )
            {
            glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
            renderQuads( mask, region, data.quads.select( WindowQuadContents ));
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
            }
#endif
        }

    if( sceneShader )
        {
        data.shader->unbind();
        data.shader = NULL;
        }
#ifndef KWIN_HAVE_OPENGLES
    else
        glPopMatrix();
#endif
    }

void SceneOpenGL::Window::paintDecoration( const QPixmap* decoration, TextureType decorationType, const QRegion& region, const QRect& rect, const WindowPaintData& data, const WindowQuadList& quads, bool updateDeco )
    {
    if( quads.isEmpty())
        return;
    SceneOpenGL::Texture* decorationTexture;
    switch( decorationType )
        {
        case DecorationTop:
            decorationTexture = &topTexture;
            break;
        case DecorationLeft:
            decorationTexture = &leftTexture;
            break;
        case DecorationRight:
            decorationTexture = &rightTexture;
            break;
        case DecorationBottom:
            decorationTexture = &bottomTexture;
            break;
        default:
            return;
        }
    if( decorationTexture->texture() != None && !updateDeco )
        {
        // texture doesn't need updating, just bind it
        glBindTexture( decorationTexture->target(), decorationTexture->texture());
        }
    else if( !decoration->isNull() )
        {
        bool success = decorationTexture->load( decoration->handle(), decoration->size(), decoration->depth() );
        if( !success )
            {
            kDebug( 1212 ) << "Failed to bind decoartion";
            return;
            }
        }
    else
        return;
    if( filter == ImageFilterGood )
        decorationTexture->setFilter( GL_LINEAR );
    else
        decorationTexture->setFilter( GL_NEAREST );
    decorationTexture->setWrapMode( GL_CLAMP_TO_EDGE );
    decorationTexture->bind();

    prepareStates( decorationType, data.opacity * data.decoration_opacity, data.brightness, data.saturation, data.shader );
    makeDecorationArrays( quads, rect );
    if( data.shader )
        {
        data.shader->setUniform("textureWidth", 1.0f);
        data.shader->setUniform("textureHeight", 1.0f);
        }
    SceneOpenGL::Window::decorationVertices->render( region, GL_TRIANGLES );
    restoreStates( decorationType, data.opacity * data.decoration_opacity, data.brightness, data.saturation, data.shader );
    decorationTexture->unbind();
#ifndef KWIN_HAVE_OPENGLES
    if( static_cast<SceneOpenGL*>(scene)->debug )
        {
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
        SceneOpenGL::Window::decorationVertices->render( region, GL_TRIANGLES );
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        }
#endif
    }

void SceneOpenGL::Window::makeDecorationArrays( const WindowQuadList& quads, const QRect& rect ) const
    {
    QVector<float> vertices;
    QVector<float> texcoords;
    vertices.reserve( quads.count() * 6 * 2 );
    texcoords.reserve( quads.count() * 6 * 2 );
    float width = rect.width();
    float height = rect.height();
    foreach( const WindowQuad& quad, quads )
        {
        vertices << quad[ 1 ].x();
        vertices << quad[ 1 ].y();
        vertices << quad[ 0 ].x();
        vertices << quad[ 0 ].y();
        vertices << quad[ 3 ].x();
        vertices << quad[ 3 ].y();
        vertices << quad[ 3 ].x();
        vertices << quad[ 3 ].y();
        vertices << quad[ 2 ].x();
        vertices << quad[ 2 ].y();
        vertices << quad[ 1 ].x();
        vertices << quad[ 1 ].y();

        texcoords << (float)(quad.originalRight()-rect.x())/width;
        texcoords << (float)(quad.originalTop()-rect.y())/height;
        texcoords << (float)(quad.originalLeft()-rect.x())/width;
        texcoords << (float)(quad.originalTop()-rect.y())/height;
        texcoords << (float)(quad.originalLeft()-rect.x())/width;
        texcoords << (float)(quad.originalBottom()-rect.y())/height;
        texcoords << (float)(quad.originalLeft()-rect.x())/width;
        texcoords << (float)(quad.originalBottom()-rect.y())/height;
        texcoords << (float)(quad.originalRight()-rect.x())/width;
        texcoords << (float)(quad.originalBottom()-rect.y())/height;
        texcoords << (float)(quad.originalRight()-rect.x())/width;
        texcoords << (float)(quad.originalTop()-rect.y())/height;
        }
    SceneOpenGL::Window::decorationVertices->setData( quads.count() * 6, 2, vertices.data(), texcoords.data() );
    }

void SceneOpenGL::Window::renderQuads( int, const QRegion& region, const WindowQuadList& quads )
    {
    if( quads.isEmpty())
        return;
    // Render geometry
    float* vertices;
    float* texcoords;
    quads.makeArrays( &vertices, &texcoords );
    vertexBuffer->setData( quads.count() * 6, 2, vertices, texcoords );
    vertexBuffer->render( region, GL_TRIANGLES );
    delete[] vertices;
    delete[] texcoords;
    }

void SceneOpenGL::Window::prepareStates( TextureType type, double opacity, double brightness, double saturation, GLShader* shader )
    {
    if(shader)
        prepareShaderRenderStates( type, opacity, brightness, saturation, shader );
    else
        prepareRenderStates( type, opacity, brightness, saturation );
    }

void SceneOpenGL::Window::prepareShaderRenderStates( TextureType type, double opacity, double brightness, double saturation, GLShader* shader )
    {
    // setup blending of transparent windows
#ifndef KWIN_HAVE_OPENGLES
    glPushAttrib( GL_ENABLE_BIT );
#endif
    bool opaque = isOpaque() && opacity == 1.0;
    bool alpha = toplevel->hasAlpha() || type != Content;
    if( type != Content )
        opaque = false;
    if (!opaque) {
        glEnable(GL_BLEND);
        if (alpha) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glBlendColor((float)opacity, (float)opacity, (float)opacity, (float)opacity);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_CONSTANT_ALPHA);
        }
    }
    shader->setUniform("opacity", (float)opacity);
    shader->setUniform("saturation", (float)saturation);
    shader->setUniform("brightness", (float)brightness);

    // setting texture width and heiht stored in shader
    // only set if it is set by an effect that is not negative
    float texw = shader->textureWidth();
    if( texw >= 0.0f )
        shader->setUniform("textureWidth", texw);
    else
        shader->setUniform("textureWidth", (float)toplevel->width());
    float texh = shader->textureHeight();
    if( texh >= 0.0f )
        shader->setUniform("textureHeight", texh);
    else
        shader->setUniform("textureHeight", (float)toplevel->height());
    }

void SceneOpenGL::Window::prepareRenderStates( TextureType type, double opacity, double brightness, double saturation )
    {
#ifndef KWIN_HAVE_OPENGLES
    Texture* tex;
    bool alpha = false;
    bool opaque = true;
    switch( type )
        {
        case Content:
            tex = &texture;
            alpha = toplevel->hasAlpha();
            opaque = isOpaque() && opacity == 1.0;
            break;
        case DecorationTop:
            tex = &topTexture;
            alpha = true;
            opaque = false;
            break;
        case DecorationLeft:
            tex = &leftTexture;
            alpha = true;
            opaque = false;
            break;
        case DecorationRight:
            tex = &rightTexture;
            alpha = true;
            opaque = false;
            break;
        case DecorationBottom:
            tex = &bottomTexture;
            alpha = true;
            opaque = false;
            break;
        default:
            return;
        }
    // setup blending of transparent windows
    glPushAttrib( GL_ENABLE_BIT );
    if( !opaque )
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
        }
    if( saturation != 1.0 && tex->saturationSupported())
        {
        // First we need to get the color from [0; 1] range to [0.5; 1] range
        glActiveTexture( GL_TEXTURE0 );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA );
        const float scale_constant[] = { 1.0, 1.0, 1.0, 0.5};
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, scale_constant );
        tex->bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture( GL_TEXTURE1 );
        float saturation_constant[] = { 0.5 + 0.5*0.30, 0.5 + 0.5*0.59, 0.5 + 0.5*0.11, saturation };
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant );
        tex->bind();

        // Finally we need to interpolate between the original image and the
        //  greyscale image to get wanted level of saturation
        glActiveTexture( GL_TEXTURE2 );
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0 );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant );
        // Also replace alpha by primary color's alpha here
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
        // And make primary color contain the wanted opacity
        glColor4f( opacity, opacity, opacity, opacity );
        tex->bind();

        if( alpha || brightness != 1.0f )
            {
            glActiveTexture( GL_TEXTURE3 );
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
            // The color has to be multiplied by both opacity and brightness
            float opacityByBrightness = opacity * brightness;
            glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity );
            if( alpha )
                {
                // Multiply original texture's alpha by our opacity
                glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0 );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA );
                }
            else
                {
                // Alpha will be taken from previous stage
                glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
                }
            tex->bind();
            }

        glActiveTexture(GL_TEXTURE0 );
        }
    else if( opacity != 1.0 || brightness != 1.0 )
        {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        float opacityByBrightness = opacity * brightness;
        if( alpha)
            {
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
            glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness,
                opacity);
            }
        else
            {
            // Multiply color by brightness and replace alpha by opacity
            float constant[] = { opacityByBrightness, opacityByBrightness, opacityByBrightness, opacity };
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT );
            glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant );
            }
        }
    else if( !alpha && opaque )
        {
        float constant[] = { 1.0, 1.0, 1.0, 1.0 };
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_CONSTANT );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant );
        }
#endif
    }

void SceneOpenGL::Window::restoreStates( TextureType type, double opacity, double brightness, double saturation, GLShader* shader )
    {
    if(shader)
        restoreShaderRenderStates( type, opacity, brightness, saturation, shader );
    else
        restoreRenderStates( type, opacity, brightness, saturation );
    }

void SceneOpenGL::Window::restoreShaderRenderStates( TextureType type, double opacity, double brightness, double saturation, GLShader* shader )
    {
    Q_UNUSED( brightness );
    Q_UNUSED( saturation );
    Q_UNUSED( shader );
    bool opaque = isOpaque() && opacity == 1.0;
    if( type != Content )
        opaque = false;
    if( !opaque )
        {
        glDisable( GL_BLEND );
        }
#ifndef KWIN_HAVE_OPENGLES
    glPopAttrib();  // ENABLE_BIT
#endif
    }

void SceneOpenGL::Window::restoreRenderStates( TextureType type, double opacity, double brightness, double saturation )
    {
#ifndef KWIN_HAVE_OPENGLES
    Texture* tex;
    switch( type )
        {
        case Content:
            tex = &texture;
            break;
        case DecorationTop:
            tex = &topTexture;
            break;
        case DecorationLeft:
            tex = &leftTexture;
            break;
        case DecorationRight:
            tex = &rightTexture;
            break;
        case DecorationBottom:
            tex = &bottomTexture;
            break;
        default:
            return;
        }
    if( opacity != 1.0 || saturation != 1.0 || brightness != 1.0f )
        {
        if( saturation != 1.0 && tex->saturationSupported())
            {
            glActiveTexture(GL_TEXTURE3);
            glDisable( tex->target());
            glActiveTexture(GL_TEXTURE2);
            glDisable( tex->target());
            glActiveTexture(GL_TEXTURE1);
            glDisable( tex->target());
            glActiveTexture(GL_TEXTURE0);
            }
        }
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    glColor4f( 0, 0, 0, 0 );

    glPopAttrib();  // ENABLE_BIT
#endif
    }

//****************************************
// SceneOpenGL::EffectFrame
//****************************************

SceneOpenGL::Texture* SceneOpenGL::EffectFrame::m_unstyledTexture = NULL;
QPixmap* SceneOpenGL::EffectFrame::m_unstyledPixmap = NULL;

SceneOpenGL::EffectFrame::EffectFrame( EffectFrameImpl* frame )
    : Scene::EffectFrame( frame )
    , m_texture( NULL )
    , m_textTexture( NULL )
    , m_textPixmap( NULL )
    , m_oldTextTexture( NULL )
    , m_iconTexture( NULL )
    , m_oldIconTexture( NULL )
    , m_selectionTexture( NULL )
    , m_unstyledVBO( NULL )
    {
    if( m_effectFrame->style() == EffectFrameUnstyled && !m_unstyledTexture )
        {
        updateUnstyledTexture();
        }
    }

SceneOpenGL::EffectFrame::~EffectFrame()
    {
    delete m_texture;
    delete m_textTexture;
    delete m_textPixmap;
    delete m_oldTextTexture;
    delete m_iconTexture;
    delete m_oldIconTexture;
    delete m_selectionTexture;
    delete m_unstyledVBO;
    }

void SceneOpenGL::EffectFrame::free()
    {
    delete m_texture;
    m_texture = NULL;
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
    delete m_iconTexture;
    m_iconTexture = NULL;
    delete m_selectionTexture;
    m_selectionTexture = NULL;
    delete m_unstyledVBO;
    m_unstyledVBO = NULL;
    delete m_oldIconTexture;
    m_oldIconTexture = NULL;
    delete m_oldTextTexture;
    m_oldTextTexture = NULL;
    }

void SceneOpenGL::EffectFrame::freeIconFrame()
    {
    delete m_iconTexture;
    m_iconTexture = NULL;
    }

void SceneOpenGL::EffectFrame::freeTextFrame()
    {
    delete m_textTexture;
    m_textTexture = NULL;
    delete m_textPixmap;
    m_textPixmap = NULL;
    }

void SceneOpenGL::EffectFrame::freeSelection()
    {
    delete m_selectionTexture;
    m_selectionTexture = NULL;
    }

void SceneOpenGL::EffectFrame::crossFadeIcon()
    {
    delete m_oldIconTexture;
    m_oldIconTexture = m_iconTexture;
    m_iconTexture = NULL;
    }

void SceneOpenGL::EffectFrame::crossFadeText()
    {
    delete m_oldTextTexture;
    m_oldTextTexture = m_textTexture;
    m_textTexture = NULL;
    }

void SceneOpenGL::EffectFrame::render( QRegion region, double opacity, double frameOpacity )
    {
    if( m_effectFrame->geometry().isEmpty() )
        return; // Nothing to display

    region = infiniteRegion(); // TODO: Old region doesn't seem to work with OpenGL

    GLShader* shader = m_effectFrame->shader();
    bool sceneShader = false;
    if( !shader && static_cast<SceneOpenGL*>(scene)->hasSceneShader() )
        {
        shader = static_cast<SceneOpenGL*>(scene)->m_sceneShader;
        sceneShader = true;
        }
    if( shader )
        {
        shader->bind();
        if( sceneShader )
            shader->setUniform("geometry", QVector4D(0, 0, 0, 0));
        shader->setUniform("saturation", 1.0f);
        shader->setUniform("brightness", 1.0f);

        shader->setUniform("textureWidth", 1.0f);
        shader->setUniform("textureHeight", 1.0f);
        }

#ifndef KWIN_HAVE_OPENGLES
    glPushAttrib( GL_CURRENT_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );
#endif
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
#ifndef KWIN_HAVE_OPENGLES
    if( !shader )
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

    // TODO: drop the push matrix
    glPushMatrix();
#endif

    // Render the actual frame
    if( m_effectFrame->style() == EffectFrameUnstyled )
        {
        if( !m_unstyledVBO )
            {
            m_unstyledVBO = new GLVertexBuffer( GLVertexBuffer::Static );
            QRect area = m_effectFrame->geometry();
            area.moveTo(0,0);
            area.adjust( -5, -5, 5, 5 );
            
            const int roundness = 5;
            QVector<float> verts, texCoords;
            verts.reserve( 84 );
            texCoords.reserve( 84 );

            // top left
            verts << area.left() << area.top();
            texCoords << 0.0f << 0.0f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top
            verts << area.left() + roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.left() + roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            // top right
            verts << area.right() - roundness << area.top();
            texCoords << 0.5f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            verts << area.right() - roundness << area.top() + roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top();
            texCoords << 1.0f << 0.0f;
            // bottom left
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.left() << area.bottom();
            texCoords << 0.0f << 1.0f;
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom
            verts << area.left() + roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.left() + roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            // bottom right
            verts << area.right() - roundness << area.bottom() - roundness;
            texCoords << 0.5f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() - roundness << area.bottom();
            texCoords << 0.5f << 1.0f;
            verts << area.right() << area.bottom();
            texCoords << 1.0f << 1.0f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            // center
            verts << area.left() << area.top() + roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.left() << area.bottom() - roundness;
            texCoords << 0.0f << 0.5f;
            verts << area.right() << area.bottom() - roundness;
            texCoords << 1.0f << 0.5f;
            verts << area.right() << area.top() + roundness;
            texCoords << 1.0f << 0.5f;

            m_unstyledVBO->setData( verts.count() / 2, 2, verts.data(), texCoords.data() );
            }

        if( shader )
            shader->setUniform( "opacity", (float)(opacity * frameOpacity) );
#ifndef KWIN_HAVE_OPENGLES
        else
            glColor4f( 0.0, 0.0, 0.0, opacity * frameOpacity );
#endif

        m_unstyledTexture->bind();
        const QPoint pt = m_effectFrame->geometry().topLeft();
        if (sceneShader) {
            // TODO: move geometry
        } else {
#ifndef KWIN_HAVE_OPENGLES
            glTranslatef( pt.x(), pt.y(), 0.0f );
#endif
        }
        m_unstyledVBO->setUseShader( sceneShader );
        m_unstyledVBO->render( region, GL_TRIANGLES );
#ifndef KWIN_HAVE_OPENGLES
        if (!sceneShader) {
            glTranslatef( -pt.x(), -pt.y(), 0.0f );
        }
#endif
        m_unstyledTexture->unbind();
        }
    else if( m_effectFrame->style() == EffectFrameStyled )
        {
        if( !m_texture ) // Lazy creation
            updateTexture();

        if( shader )
            shader->setUniform( "opacity", (float)(opacity * frameOpacity) );
#ifndef KWIN_HAVE_OPENGLES
        else
            glColor4f( 1.0, 1.0, 1.0, opacity * frameOpacity );
#endif
        m_texture->bind();
        qreal left, top, right, bottom;
        m_effectFrame->frame().getMargins( left, top, right, bottom ); // m_geometry is the inner geometry
        m_texture->render( region, m_effectFrame->geometry().adjusted( -left, -top, right, bottom ), sceneShader );
        m_texture->unbind();

        if( !m_effectFrame->selection().isNull() )
            {
            if( !m_selectionTexture ) // Lazy creation
                {
                QPixmap pixmap = m_effectFrame->selectionFrame().framePixmap();
                m_selectionTexture = new Texture( pixmap.handle(), pixmap.size(), pixmap.depth() );
                m_selectionTexture->setYInverted(true);
                }
            glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
            m_selectionTexture->bind();
            m_selectionTexture->render( region, m_effectFrame->selection(), sceneShader );
            m_selectionTexture->unbind();
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            }
        }

    // Render icon
    if( !m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty() )
        {
        QPoint topLeft( m_effectFrame->geometry().x(),
                        m_effectFrame->geometry().center().y() - m_effectFrame->iconSize().height() / 2 );

        if( m_effectFrame->isCrossFade() && m_oldIconTexture )
            {
            if( shader )
                shader->setUniform( "opacity", (float)opacity * (1.0f - (float)m_effectFrame->crossFadeProgress()) );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity * (1.0 - m_effectFrame->crossFadeProgress()) );
#endif

            m_oldIconTexture->bind();
            m_oldIconTexture->render( region, QRect( topLeft, m_effectFrame->iconSize() ));
            m_oldIconTexture->unbind();
            if( shader )
                shader->setUniform( "opacity", (float)opacity * (float)m_effectFrame->crossFadeProgress() );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress() );
#endif
            }
        else
            {
            if( shader )
                shader->setUniform( "opacity", (float)opacity );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity );
#endif
            }

        if( !m_iconTexture ) // lazy creation
            {
            m_iconTexture = new Texture( m_effectFrame->icon().handle(),
                                         m_effectFrame->icon().size(),
                                         m_effectFrame->icon().depth() );
            m_iconTexture->setYInverted(true);
            }
        m_iconTexture->bind();
        m_iconTexture->render( region, QRect( topLeft, m_effectFrame->iconSize() ), sceneShader );
        m_iconTexture->unbind();
        }

    // Render text
    if( !m_effectFrame->text().isEmpty() )
        {
        if( m_effectFrame->isCrossFade() && m_oldTextTexture )
            {
            if( shader )
                shader->setUniform( "opacity", (float)opacity * (1.0f - (float)m_effectFrame->crossFadeProgress()) );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity * (1.0 - m_effectFrame->crossFadeProgress()) );
#endif

            m_oldTextTexture->bind();
            m_oldTextTexture->render( region, m_effectFrame->geometry(), sceneShader );
            m_oldTextTexture->unbind();
            if( shader )
                shader->setUniform( "opacity", (float)opacity * (float)m_effectFrame->crossFadeProgress() );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity * m_effectFrame->crossFadeProgress() );
#endif
            }
        else
            {
            if( shader )
                shader->setUniform( "opacity", (float)opacity );
#ifndef KWIN_HAVE_OPENGLES
            else
                glColor4f( 1.0, 1.0, 1.0, opacity );
#endif
            }
        if( !m_textTexture ) // Lazy creation
            updateTextTexture();
        m_textTexture->bind();
        m_textTexture->render( region, m_effectFrame->geometry(), sceneShader  );
        m_textTexture->unbind();
        }

    if( shader )
        shader->unbind();
    glDisable( GL_BLEND );
#ifndef KWIN_HAVE_OPENGLES
    glPopMatrix();
    glPopAttrib();
#endif
    }

void SceneOpenGL::EffectFrame::updateTexture()
    {
    delete m_texture;
    if( m_effectFrame->style() == EffectFrameStyled )
        {
        QPixmap pixmap = m_effectFrame->frame().framePixmap();
        m_texture = new Texture( pixmap.handle(), pixmap.size(), pixmap.depth() );
        m_texture->setYInverted(true);
        }
    }

void SceneOpenGL::EffectFrame::updateTextTexture()
    {
    delete m_textTexture;
    delete m_textPixmap;

    if( m_effectFrame->text().isEmpty() )
        return;

    // Determine position on texture to paint text
    QRect rect( QPoint( 0, 0 ), m_effectFrame->geometry().size() );
    if( !m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty() )
        rect.setLeft( m_effectFrame->iconSize().width() );

    // If static size elide text as required
    QString text = m_effectFrame->text();
    if( m_effectFrame->isStatic() )
        {
        QFontMetrics metrics( m_effectFrame->font() );
        text = metrics.elidedText( text, Qt::ElideRight, rect.width() );
        }

    m_textPixmap = new QPixmap( m_effectFrame->geometry().size() );
    m_textPixmap->fill( Qt::transparent );
    QPainter p( m_textPixmap );
    p.setFont( m_effectFrame->font() );
    if( m_effectFrame->style() == EffectFrameStyled )
        p.setPen( m_effectFrame->styledTextColor() );
    else // TODO: What about no frame? Custom color setting required
        p.setPen( Qt::white );
    p.drawText( rect, m_effectFrame->alignment(), text );
    p.end();
    m_textTexture = new Texture( m_textPixmap->handle(), m_textPixmap->size(), m_textPixmap->depth() );
    m_textTexture->setYInverted(true);
    }

void SceneOpenGL::EffectFrame::updateUnstyledTexture()
    {
    delete m_unstyledTexture;
    delete m_unstyledPixmap;
    // Based off circle() from kwinxrenderutils.cpp
#define CS 8
    m_unstyledPixmap = new QPixmap( 2 * CS, 2 * CS );
    m_unstyledPixmap->fill( Qt::transparent );
    QPainter p( m_unstyledPixmap );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( Qt::black );
    p.drawEllipse( m_unstyledPixmap->rect() );
    p.end();
#undef CS
    m_unstyledTexture = new Texture( m_unstyledPixmap->handle(), m_unstyledPixmap->size(), m_unstyledPixmap->depth() );
    m_unstyledTexture->setYInverted(true);
    }

void SceneOpenGL::EffectFrame::cleanup()
    {
    delete m_unstyledTexture;
    m_unstyledTexture = NULL;
    delete m_unstyledPixmap;
    m_unstyledPixmap = NULL;
    }

} // namespace

#endif
