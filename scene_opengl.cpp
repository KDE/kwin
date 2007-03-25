/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.

Based on glcompmgr code by Felix Bellaby.
Using code from Compiz and Beryl.
******************************************************************/

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

#include "utils.h"
#include "client.h"
#include "deleted.h"
#include "effects.h"
#include "glutils.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>

#ifdef HAVE_OPENGL

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

// the configs used for the destination
GLXFBConfig SceneOpenGL::fbcbuffer_db;
GLXFBConfig SceneOpenGL::fbcbuffer_nondb;
// the configs used for windows
SceneOpenGL::FBConfigInfo SceneOpenGL::fbcdrawableinfo[ 32 + 1 ];
// GLX content
GLXContext SceneOpenGL::ctxbuffer;
GLXContext SceneOpenGL::ctxdrawable;
// the destination drawable where the compositing is done
GLXDrawable SceneOpenGL::glxbuffer;
GLXDrawable SceneOpenGL::last_pixmap;
bool SceneOpenGL::tfp_mode; // using glXBindTexImageEXT (texture_from_pixmap)
bool SceneOpenGL::strict_binding; // intended for AIGLX
bool SceneOpenGL::db; // destination drawable is double-buffered
bool SceneOpenGL::copy_buffer_hack; // workaround for nvidia < 1.0-9xxx drivers
bool SceneOpenGL::shm_mode;
#ifdef HAVE_XSHM
XShmSegmentInfo SceneOpenGL::shm;
#endif


// detect OpenGL error (add to various places in code to pinpoint the place)
static void checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) << endl;
    }

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    {
    // TODO add checks where needed
    int dummy;
    if( !glXQueryExtension( display(), &dummy, &dummy ))
        return;
    initGLX();
    // check for FBConfig support
    if( !hasGLXVersion( 1, 3 ) && !hasGLExtension( "GLX_SGIX_fbconfig" ))
        return;
    strict_binding = false; // not needed now
    selectMode();
    initBuffer(); // create destination buffer
    initRenderingContext();
    
    // Initialize OpenGL
    initGL();
    if( db )
        glDrawBuffer( GL_BACK );
    // Check whether certain features are supported
    has_waitSync = glXGetVideoSync ? true : false;

    if( !initDrawableConfigs())
        assert( false );
    int vis_buffer, vis_drawable;
    glXGetFBConfigAttrib( display(), fbcbuffer, GLX_VISUAL_ID, &vis_buffer );
    kDebug( 1212 ) << "Buffer visual (depth " << QX11Info::appDepth() << "): 0x" << QString::number( vis_buffer, 16 ) << endl;
    for( int i = 0; i <= 32; i++ )
        {
        if( fbcdrawableinfo[ i ].fbconfig == NULL )
            continue;
        glXGetFBConfigAttrib( display(), fbcdrawableinfo[ i ].fbconfig, GLX_VISUAL_ID, &vis_drawable );
        kDebug( 1212 ) << "Drawable visual (depth " << i << "): 0x" << QString::number( vis_drawable, 16 ) << endl;
        }

    // OpenGL scene setup
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    // swap top and bottom to have OpenGL coordinate system match X system
    glOrtho( 0, displayWidth(), displayHeight(), 0, 0, 65535 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    checkGLError( "Init" );
    kDebug( 1212 ) << "DB:" << db << ", TFP:" << tfp_mode << ", SHM:" << shm_mode
        << ", Direct:" << bool( glXIsDirect( display(), ctxbuffer )) << endl;
    }

SceneOpenGL::~SceneOpenGL()
    {
    foreach( Window* w, windows )
        delete w;
    // do cleanup after initBuffer()
    if( wspace->overlayWindow())
        {
        if( hasGLXVersion( 1, 3 ))
            glXDestroyWindow( display(), glxbuffer );
        XDestroyWindow( display(), buffer );
        wspace->destroyOverlay();
        }
    else
        {
        glXDestroyPixmap( display(), glxbuffer );
        XFreeGC( display(), gcroot );
        XFreePixmap( display(), buffer );
        }
    if( shm_mode )
        cleanupShm();
    if( !tfp_mode && !shm_mode )
        {
        if( last_pixmap != None )
            glXDestroyPixmap( display(), last_pixmap );
        glXDestroyContext( display(), ctxdrawable );
        }
    glXDestroyContext( display(), ctxbuffer );
    checkGLError( "Cleanup" );
    }

void SceneOpenGL::selectMode()
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
        assert( false );
    // use copy buffer hack from glcompmgr (called COPY_BUFFER there) - nvidia drivers older than
    // 1.0-9xxx don't update pixmaps properly, so do a copy first
    copy_buffer_hack = !tfp_mode && !shm_mode; // TODO detect that it's nvidia < 1.0-9xxx driver
    }

bool SceneOpenGL::initTfp()
    {
    if( glXBindTexImageEXT == NULL || glXReleaseTexImageEXT == NULL )
        return false;
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

void SceneOpenGL::initRenderingContext()
    {
    bool direct_rendering = options->glDirect;
    if( !tfp_mode && !shm_mode )
        direct_rendering = false; // fallback doesn't seem to work with direct rendering
    KXErrorHandler errs;
    ctxbuffer = glXCreateNewContext( display(), fbcbuffer, GLX_RGBA_TYPE, NULL,
        direct_rendering ? GL_TRUE : GL_FALSE );
    if( ctxbuffer == NULL || !glXMakeContextCurrent( display(), glxbuffer, glxbuffer, ctxbuffer )
        || errs.error( true ))
        { // failed
        if( !direct_rendering )
            assert( false );
        glXDestroyContext( display(), ctxbuffer );
        direct_rendering = false; // try again
        ctxbuffer = glXCreateNewContext( display(), fbcbuffer, GLX_RGBA_TYPE, NULL, GL_FALSE );
        if( ctxbuffer == NULL || !glXMakeContextCurrent( display(), glxbuffer, glxbuffer, ctxbuffer ))
            assert( false );
        }
    if( !tfp_mode && !shm_mode )
        {
        ctxdrawable = glXCreateNewContext( display(), fbcdrawableinfo[ QX11Info::appDepth() ].fbconfig, GLX_RGBA_TYPE, ctxbuffer,
            direct_rendering ? GL_TRUE : GL_FALSE );
        }
    }

// create destination buffer
void SceneOpenGL::initBuffer()
    {
    initBufferConfigs();
    if( fbcbuffer_db != NULL && wspace->createOverlay())
        { // we have overlay, try to create double-buffered window in it
        fbcbuffer = fbcbuffer_db;
        XVisualInfo* visual = glXGetVisualFromFBConfig( display(), fbcbuffer );
        XSetWindowAttributes attrs;
        attrs.colormap = XCreateColormap( display(), rootWindow(), visual->visual, AllocNone );
        buffer = XCreateWindow( display(), wspace->overlayWindow(), 0, 0, displayWidth(), displayHeight(),
            0, QX11Info::appDepth(), InputOutput, visual->visual, CWColormap, &attrs );
        if( hasGLXVersion( 1, 3 ))
            glxbuffer = glXCreateWindow( display(), fbcbuffer, buffer, NULL );
        else
            glxbuffer = buffer;
        wspace->setupOverlay( buffer );
        db = true;
        XFree( visual );
        }
    else if( fbcbuffer_nondb != NULL )
        { // cannot get any double-buffered drawable, will double-buffer using a pixmap
        fbcbuffer = fbcbuffer_nondb;
        db = false;
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
        buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
            QX11Info::appDepth());
        glxbuffer = glXCreatePixmap( display(), fbcbuffer, buffer, NULL );
        }
    else
        assert( false );
    }

// choose the best configs for the destination buffer
bool SceneOpenGL::initBufferConfigs()
    {
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display() ), &cnt );
    fbcbuffer_db = NULL;
    fbcbuffer_nondb = NULL;

    for( int i = 0; i < 2; i++ )
        {
        int back, stencil, depth, caveat, alpha;
        if( i > 0 )
            back = INT_MAX;
        else
            back = 1;
        stencil = 0;
        depth = 0;
        caveat = INT_MAX;
        alpha = 0;
        for( int j = 0; j < cnt; j++ )
            {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig( display(), fbconfigs[ j ] );
            if( vi == NULL )
                continue;
            visual_depth = vi->depth;
            XFree( vi );
            if( visual_depth != QX11Info::appDepth() )
                continue;
            int value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_ALPHA_SIZE, &alpha );
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_BUFFER_SIZE, &value );
            if( value != QX11Info::appDepth() && ( value - alpha ) != QX11Info::appDepth() )
                continue;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DOUBLEBUFFER, &value );
            if( i > 0 )
                {
                if( value > back )
                    continue;
                }
            else
                {
                if( value < back )
                    continue;
                }
            back = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_STENCIL_SIZE, &value );
            if( value < stencil )
                continue;
            stencil = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DEPTH_SIZE, &value );
            if( value < depth )
                continue;
            depth = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_CONFIG_CAVEAT, &value );
            if( value > caveat )
                continue;
            caveat = value;
            if( i > 0 )
                fbcbuffer_nondb = fbconfigs[ j ];
            else
                fbcbuffer_db = fbconfigs[ j ];
            }
        }
    if( cnt )
        XFree( fbconfigs );
    if( fbcbuffer_db == NULL && fbcbuffer_nondb == NULL )
        {
        kDebug( 1212 ) << "Couldn't find framebuffer configuration for buffer!" << endl;
        return false;
        }
    return true;
    }

// make a list of the best configs for windows by depth
bool SceneOpenGL::initDrawableConfigs()
    {
    int cnt;
    GLXFBConfig *fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display() ), &cnt );

    for( int i = 0; i <= 32; i++ )
        {
        int back, stencil, depth, caveat, alpha, mipmap, rgba;
        back = INT_MAX;
        stencil = INT_MAX;
        depth = INT_MAX;
        caveat = INT_MAX;
        mipmap = 0;
        rgba = 0;
        fbcdrawableinfo[ i ].fbconfig = NULL;
        fbcdrawableinfo[ i ].bind_texture_format = 0;
        fbcdrawableinfo[ i ].y_inverted = 0;
        fbcdrawableinfo[ i ].mipmap = 0;
        for( int j = 0; j < cnt; j++ )
            {
            XVisualInfo *vi;
            int visual_depth;
            vi = glXGetVisualFromFBConfig( display(), fbconfigs[ j ] );
            if( vi == NULL )
                continue;
            visual_depth = vi->depth;
            XFree( vi );
            if( visual_depth != i )
                continue;
            int value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_ALPHA_SIZE, &alpha );
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_BUFFER_SIZE, &value );
            if( value != i && ( value - alpha ) != i )
                continue;
            if( tfp_mode )
                {
                value = 0;
                if( i == 32 )
                    {
                    glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                          GLX_BIND_TO_TEXTURE_RGBA_EXT, &value );
                    if( value )
                        {
                        rgba = 1;
                        fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGBA_EXT;
                        }
                    }
                if( !value )
                    {
                    if( rgba )
                        continue;
                    glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                          GLX_BIND_TO_TEXTURE_RGB_EXT, &value );
                    if( !value )
                        continue;
                    fbcdrawableinfo[ i ].bind_texture_format = GLX_TEXTURE_FORMAT_RGB_EXT;
                    }
                }
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DOUBLEBUFFER, &value );
            if( value > back )
                continue;
            back = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_STENCIL_SIZE, &value );
            if( value > stencil )
                continue;
            stencil = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_DEPTH_SIZE, &value );
            if( value > depth )
                continue;
            depth = value;
            if( tfp_mode && GLTexture::framebufferObjectSupported())
                {
                glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                      GLX_BIND_TO_MIPMAP_TEXTURE_EXT, &value );
                if( value < mipmap )
                    continue;
                mipmap = value;
                }
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_CONFIG_CAVEAT, &value );
            if( value > caveat )
                continue;
            caveat = value;
            glXGetFBConfigAttrib( display(), fbconfigs[ j ],
                                  GLX_Y_INVERTED_EXT, &value );

            fbcdrawableinfo[ i ].fbconfig = fbconfigs[ j ];
            fbcdrawableinfo[ i ].y_inverted = value;
            fbcdrawableinfo[ i ].mipmap = mipmap;
            }
        }
    if( cnt )
        XFree( fbconfigs );
    if( fbcdrawableinfo[ QX11Info::appDepth() ].fbconfig == NULL )
        {
        kDebug( 1212 ) << "Couldn't find framebuffer configuration for default depth!" << endl;
        return false;
        }
    return true;
    }

// the entry function for painting
void SceneOpenGL::paint( QRegion damage, ToplevelList toplevels )
    {
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( windows[ c ] );
        }
    grabXServer();
    glXWaitX();
    glPushMatrix();
    int mask = 0;
    paintScreen( &mask, &damage ); // call generic implementation
    glPopMatrix();
    ungrabXServer(); // ungrab before flushBuffer(), it may wait for vsync
    flushBuffer( mask, damage );
    // do cleanup
    stacking_order.clear();
    checkGLError( "PostPaint" );
    }

// wait for vblank signal before painting
void SceneOpenGL::waitSync()
    { // NOTE that vsync has no effect with indirect rendering
    if( waitSyncAvailable() && options->glVSync )
        {
        unsigned int sync;

        glFlush();
        glXGetVideoSync( &sync );
        glXWaitVideoSync( 2, ( sync + 1 ) % 2, &sync );
        }
    }

// actually paint to the screen (double-buffer swap or copy from pixmap buffer)
void SceneOpenGL::flushBuffer( int mask, QRegion damage )
    {
    if( db )
        {
        if( mask & PAINT_SCREEN_REGION )
            {
            waitSync();
            if( glXCopySubBuffer )
                {
                foreach( QRect r, damage.rects())
                    {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    glXCopySubBuffer( display(), glxbuffer, r.x(), y, r.width(), r.height());
                    }
                }
            else
                { // no idea why glScissor() is used, but Compiz has it and it doesn't seem to hurt
                glEnable( GL_SCISSOR_TEST );
                glDrawBuffer( GL_FRONT );
                int xpos = 0;
                int ypos = 0;
                foreach( QRect r, damage.rects())
                    {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    // Move raster position relatively using glBitmap() rather
                    // than using glRasterPos2f() - the latter causes drawing
                    // artefacts at the bottom screen edge with some gfx cards
//                    glRasterPos2f( r.x(), r.y() + r.height());
                    glBitmap( 0, 0, 0, 0, r.x() - xpos, y - ypos, NULL );
                    xpos = r.x();
                    ypos = y;
                    glScissor( r.x(), y, r.width(), r.height());
                    glCopyPixels( r.x(), y, r.width(), r.height(), GL_COLOR );
                    }
                glBitmap( 0, 0, 0, 0, -xpos, -ypos, NULL ); // move position back to 0,0
                glDrawBuffer( GL_BACK );
                glDisable( GL_SCISSOR_TEST );
                }
            }
        else
            {
            waitSync();
            glXSwapBuffers( display(), glxbuffer );
            }
        glXWaitGL();
        XFlush( display());
        }
    else
        {
        glFlush();
        glXWaitGL();
        waitSync();
        if( mask & PAINT_SCREEN_REGION )
            foreach( QRect r, damage.rects())
                XCopyArea( display(), buffer, rootWindow(), gcroot, r.x(), r.y(), r.width(), r.height(), r.x(), r.y());
        else
            XCopyArea( display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0 );
        XFlush( display());
        }
    }

void SceneOpenGL::paintGenericScreen( int mask, ScreenPaintData data )
    {
    if( mask & PAINT_SCREEN_TRANSFORMED )
        { // apply screen transformations
        glPushMatrix();
        glTranslatef( data.xTranslate, data.yTranslate, 0 );
        glScalef( data.xScale, data.yScale, 1 );
        }
    Scene::paintGenericScreen( mask, data );
    if( mask & PAINT_SCREEN_TRANSFORMED )
        glPopMatrix();
    }

void SceneOpenGL::paintBackground( QRegion region )
    {
    if( region == infiniteRegion())
        {
        glClearColor( 1, 1, 1, 1 ); // white
        glClear( GL_COLOR_BUFFER_BIT );
        }
    else
        {
        glColor4f( 1, 1, 1, 1 ); // white
        glBegin( GL_QUADS );
        foreach( QRect r, region.rects())
            {
            glVertex2i( r.x(), r.y());
            glVertex2i( r.x() + r.width(), r.y());
            glVertex2i( r.x() + r.width(), r.y() + r.height());
            glVertex2i( r.x(), r.y() + r.height());
            }
        glEnd();
        }
    }

void SceneOpenGL::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = new Window( c );
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
        }
    }

void SceneOpenGL::windowDeleted( Deleted* c )
    {
    assert( windows.contains( c ));
    delete windows.take( c );
    }

void SceneOpenGL::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid
        return;                 // by default
    Window* w = windows[ c ];
    w->discardShape();
    w->discardTexture();
    w->discardVertices();
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

//****************************************
// SceneOpenGL::Texture
//****************************************

SceneOpenGL::Texture::Texture()
    {
    init();
    }

SceneOpenGL::Texture::Texture( const Pixmap& pix, const QSize& size, int depth )
    {
    init();
    load( pix, size, depth );
    }

void SceneOpenGL::Texture::init()
    {
    bound_glxpixmap = None;
    }

void SceneOpenGL::Texture::discard()
    {
    if( mTexture != None )
        {
        if( tfp_mode )
            {
            if( !strict_binding )
                glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            bound_glxpixmap = None;
            }
        }
    GLTexture::discard();
    }

void SceneOpenGL::Texture::findTarget()
    {
    unsigned int new_target = 0;
    if( tfp_mode && glXQueryDrawable && bound_glxpixmap != None )
        glXQueryDrawable( display(), bound_glxpixmap, GLX_TEXTURE_TARGET_EXT, &new_target );
    else
        {
        if( NPOTTextureSupported() ||
            ( isPowerOfTwo( mSize.width()) && isPowerOfTwo( mSize.height())))
            new_target = GLX_TEXTURE_2D_EXT;
        else
            new_target = GLX_TEXTURE_RECTANGLE_EXT;
        }
    switch( new_target )
        {
        case GLX_TEXTURE_2D_EXT:
            mTarget = GL_TEXTURE_2D;
            mScale.setWidth( 1.0f / mSize.width());
            mScale.setHeight( 1.0f / mSize.height());
          break;
        case GLX_TEXTURE_RECTANGLE_EXT:
            mTarget = GL_TEXTURE_RECTANGLE_ARB;
            mScale.setWidth( 1.0f );
            mScale.setHeight( 1.0f );
          break;
        default:
            assert( false );
        }
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
    foreach( QRect r, reg.rects())
        size += r.width() * r.height();
    if( reg.boundingRect().width() * reg.boundingRect().height() - size < limit )
        return reg.boundingRect();
    return reg;
    }

bool SceneOpenGL::Texture::load( const Pixmap& pix, const QSize& size,
    int depth, QRegion region )
    {
    if( pix == None || size.isEmpty() || depth < 1 )
        return false;
    if( tfp_mode )
        {
        if( fbcdrawableinfo[ depth ].fbconfig == NULL )
            {
            kDebug( 1212 ) << "No framebuffer configuration for depth " << depth
                           << "; not binding pixmap" << endl;
            return false;
            }
        }

    mSize = size;
    if( mTexture == None || !region.isEmpty())
        { // new texture, or texture contents changed; mipmaps now invalid
        setDirty();
        }

    if( tfp_mode )
        { // tfp mode, simply bind the pixmap to texture
        if( mTexture == None )
            glGenTextures( 1, &mTexture );
        if( bound_glxpixmap != None && !strict_binding ) // release old if needed
            {
            glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            }
        static const int attrs[] =
            {
            GLX_TEXTURE_FORMAT_EXT, fbcdrawableinfo[ depth ].bind_texture_format,
            GLX_MIPMAP_TEXTURE_EXT, fbcdrawableinfo[ depth ].mipmap,
            None
            };
        // the GLXPixmap will reference the X pixmap, so it will be freed automatically
        // when no longer needed
        bound_glxpixmap = glXCreatePixmap( display(), fbcdrawableinfo[ depth ].fbconfig, pix, attrs );
        findTarget();
        y_inverted = fbcdrawableinfo[ depth ].y_inverted ? true : false;
        can_use_mipmaps = fbcdrawableinfo[ depth ].mipmap ? true : false;
        glBindTexture( mTarget, mTexture );
        if( !strict_binding )
            glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    else if( shm_mode )
        { // copy pixmap contents to a texture via shared memory
#ifdef HAVE_XSHM
        findTarget();
        if( mTexture == None )
            {
            glGenTextures( 1, &mTexture );
            glBindTexture( mTarget, mTexture );
            y_inverted = false;
            glTexImage2D( mTarget, 0, GL_RGBA, mSize.width(), mSize.height(),
                0, GL_BGRA, GL_UNSIGNED_BYTE, NULL );
            // TODO hasAlpha() ?
//            glCopyTexImage2D( mTarget, 0,
//                depth == 32 ? GL_RGBA : GL_RGB,
//                0, 0, mSize.width(), mSize.height(), 0 );
            }
        else
            glBindTexture( mTarget, mTexture );
        if( !region.isEmpty())
            {
            XGCValues xgcv;
            xgcv.graphics_exposures = False;
            xgcv.subwindow_mode = IncludeInferiors;
            GC gc = XCreateGC( display(), pix, GCGraphicsExposures | GCSubwindowMode, &xgcv );
            QRegion damage = optimizeBindDamage( region, 100 * 100 );
            foreach( QRect r, damage.rects())
                { // TODO for small areas it might be faster to not use SHM to avoid the XSync()
                Pixmap p = XShmCreatePixmap( display(), rootWindow(), shm.shmaddr, &shm,
                    r.width(), r.height(), depth );
                XCopyArea( display(), pix, p, gc, r.x(), r.y(), r.width(), r.height(), 0, 0 );
                XSync( display(), False );
                glXWaitX();
                glTexSubImage2D( mTarget, 0,
                    r.x(), r.y(), r.width(), r.height(), GL_BGRA, GL_UNSIGNED_BYTE, shm.shmaddr );
                glXWaitGL();
                XFreePixmap( display(), p );
                }
            XFreeGC( display(), gc );
            }
        y_inverted = true;
        can_use_mipmaps = true;
#endif
        }
    else
        { // fallback, copy pixmap contents to a texture
        // note that if depth is not QX11Info::appDepth(), this may
        // not work (however, it does seem to work with nvidia)
        findTarget();
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawableinfo[ QX11Info::appDepth() ].fbconfig, pix, NULL );
        glXMakeContextCurrent( display(), pixmap, pixmap, ctxdrawable );
        if( last_pixmap != None )
            glXDestroyPixmap( display(), last_pixmap );
        // workaround for ATI - it leaks/crashes when the pixmap is destroyed immediately
        // here (http://lists.kde.org/?l=kwin&m=116353772208535&w=2)
        last_pixmap = pixmap;
        glReadBuffer( GL_FRONT );
        glDrawBuffer( GL_FRONT );
        if( mTexture == None )
            {
            glGenTextures( 1, &mTexture );
            glBindTexture( mTarget, mTexture );
            y_inverted = false;
            glCopyTexImage2D( mTarget, 0,
                depth == 32 ? GL_RGBA : GL_RGB,
                0, 0, mSize.width(), mSize.height(), 0 );
            }
        else
            {
            glBindTexture( mTarget, mTexture );
            QRegion damage = optimizeBindDamage( region, 30 * 30 );
            foreach( QRect r, damage.rects())
                {
                // convert to OpenGL coordinates (this is mapping
                // the pixmap to a texture, this is not affected
                // by using glOrtho() for the OpenGL scene)
                int gly = mSize.height() - r.y() - r.height();
                glCopyTexSubImage2D( mTarget, 0,
                    r.x(), gly, r.x(), gly, r.width(), r.height());
                }
            }
        glXWaitGL();
        if( db )
            glDrawBuffer( GL_BACK );
        glXMakeContextCurrent( display(), glxbuffer, glxbuffer, ctxbuffer );
        glBindTexture( mTarget, mTexture );
        y_inverted = false;
        can_use_mipmaps = true;
        }
    return true;
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

void SceneOpenGL::Texture::bind()
    {
    glEnable( mTarget );
    glBindTexture( mTarget, mTexture );
    if( tfp_mode && strict_binding )
        {
        assert( bound_glxpixmap != None );
        glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    enableFilter();
    }

void SceneOpenGL::Texture::unbind()
    {
    if( tfp_mode && strict_binding )
        {
        assert( bound_glxpixmap != None );
        glBindTexture( mTarget, mTexture );
        glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
        }
    GLTexture::unbind();
    }

//****************************************
// SceneOpenGL::Window
//****************************************

SceneOpenGL::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , texture()
    , currentXResolution( -1 )
    , currentYResolution( -1 )
    , requestedXResolution( 0 )
    , requestedYResolution( 0 )
    , verticesDirty( false )
    {
    }

SceneOpenGL::Window::~Window()
    {
    discardTexture();
    discardVertices();
    }

void SceneOpenGL::Window::requestVertexGrid(int maxquadsize)
    {
    requestedXResolution = (requestedXResolution <= 0) ? maxquadsize : qMin(maxquadsize, requestedXResolution);
    requestedYResolution = (requestedYResolution <= 0) ? maxquadsize : qMin(maxquadsize, requestedYResolution);
    }

void SceneOpenGL::Window::createVertexGrid(int xres, int yres)
    {
    int oldcount = verticeslist.count();
    verticeslist.clear();
    foreach( QRect r, shape().rects())
        {
        // First calculate number of columns/rows that this rect will be
        //  divided into
        int cols = (xres <= 0) ? 1 : (int)ceilf( r.width() / (float)xres );
        int rows = (yres <= 0) ? 1 : (int)ceilf( r.height() / (float)yres );
        // Now calculate actual size of each cell
        int cellw = r.width() / cols;
        int cellh = r.height() / rows;
        int maxx = r.x() + r.width();
        int maxy = r.y() + r.height();
        for( int x1 = r.x(); x1 < maxx; x1 += cellw )
            {
            int x2 = qMin(x1 + cellw, maxx);
            for( int y1 = r.y(); y1 < maxy; y1 += cellh )
                {
                int y2 = qMin(y1 + cellh, maxy);
                // Add this quad to vertices' list
                verticeslist.append( Vertex( x1, y1 ));
                verticeslist.append( Vertex( x1, y2 ));
                verticeslist.append( Vertex( x2, y2 ));
                verticeslist.append( Vertex( x2, y1 ));
                }
            }
        }
    Client* c = qobject_cast<Client *>(window());
    kDebug( 1212 ) << k_funcinfo << "'" << (c ? c->caption() : "") << "': Resized vertex grid from " <<
            oldcount/4 << " quads (minreso: " << currentXResolution << "x" << currentYResolution <<
            ") to " << verticeslist.count()/4 << " quads (minreso: " << xres << "x" << yres << ")" << endl;

    currentXResolution = xres;
    currentYResolution = yres;
    verticesDirty = false;
    }

void SceneOpenGL::Window::resetVertices()
    {
    // This assumes that texcoords of the vertices are unchanged. If they are,
    //  we need to do this in some other way (or maybe the effects should then
    // clean things up themselves)
    for(int i = 0; i < verticeslist.count(); i++)
        {
        verticeslist[i].pos[0] = verticeslist[i].texcoord[0];
        verticeslist[i].pos[1] = verticeslist[i].texcoord[1];
        }
    verticesDirty = false;
    }

void SceneOpenGL::Window::prepareVertices()
    {
    if( requestedXResolution != currentXResolution || requestedYResolution != currentYResolution )
        createVertexGrid( requestedXResolution, requestedYResolution );
    else if( verticesDirty )
        resetVertices();

    // Reset requests for the next painting
    requestedXResolution = 0;
    requestedYResolution = 0;

    // Reset shader. If effect wants to use shader, it has to set it in paint pass
    shader = 0;
    }

void SceneOpenGL::Window::prepareForPainting()
    {
    prepareVertices();
    // We should also bind texture here so that effects could access it in the
    //  paint pass
    }

// Bind the window pixmap to an OpenGL texture.
bool SceneOpenGL::Window::bindTexture()
    {
    if( texture.texture() != None && toplevel->damage().isEmpty()
        && !options->glAlwaysRebind ) // interestingly with some gfx cards always rebinding is faster
        {
        // texture doesn't need updating, just bind it
        glBindTexture( texture.target(), texture.texture());
        return true;
        }
    // Get the pixmap with the window contents
    Pixmap pix = toplevel->windowPixmap();
    if( pix == None )
        return false;
    // HACK
    // When a window uses ARGB visual and has a decoration, the decoration
    // does use ARGB visual. When converting such window to a texture
    // the alpha for the decoration part is broken for some reason (undefined?).
    // I wasn't lucky converting KWin to use ARGB visuals for decorations,
    // so instead simply set alpha in those parts to opaque.
    // Without alpha_clear_copy the setting is done directly in the window
    // pixmap, which seems to be ok, but let's not risk trouble right now.
    // TODO check if this isn't a performance problem and how it can be done better
    Client* c = dynamic_cast< Client* >( toplevel );
    bool alpha_clear = c != NULL && c->hasAlpha() && !c->noBorder();
    bool alpha_clear_copy = true;
    bool copy_buffer = (( alpha_clear && alpha_clear_copy ) || copy_buffer_hack );
    if( copy_buffer )
        {
        Pixmap p2 = XCreatePixmap( display(), pix, toplevel->width(), toplevel->height(), toplevel->depth());
        GC gc = XCreateGC( display(), pix, 0, NULL );
        XCopyArea( display(), pix, p2, gc, 0, 0, toplevel->width(), toplevel->height(), 0, 0 );
        pix = p2;
        XFreeGC( display(), gc );
        }
    if( alpha_clear )
        {
        XGCValues gcv;
        gcv.foreground = 0xff000000;
        gcv.plane_mask = 0xff000000;
        GC gc = XCreateGC( display(), pix, GCPlaneMask | GCForeground, &gcv );
        XFillRectangle( display(), pix, gc, 0, 0, c->width(), c->clientPos().y());
        XFillRectangle( display(), pix, gc, 0, 0, c->clientPos().x(), c->height());
        int tw = c->clientPos().x() + c->clientSize().width();
        int th = c->clientPos().y() + c->clientSize().height();
        XFillRectangle( display(), pix, gc, 0, th, c->width(), c->height() - th );
        XFillRectangle( display(), pix, gc, tw, 0, c->width() - tw, c->height());
        XFreeGC( display(), gc );
        }
    if( copy_buffer || alpha_clear )
        glXWaitX();

    bool success = texture.load( pix, toplevel->size(), toplevel->depth(),
        toplevel->damage());
    if( success )
        toplevel->resetDamage( toplevel->rect());
    else
        kDebug( 1212 ) << "Failed to bind window" << endl;
    // if using copy_buffer, the pixmap is no longer needed (either referenced
    // by GLXPixmap in the tfp case or not needed at all in non-tfp cases)
    if( copy_buffer )
        XFreePixmap( display(), pix );
    return success;
    }

void SceneOpenGL::Window::discardTexture()
    {
    texture.discard();
    }

void SceneOpenGL::Window::discardVertices()
{
    // Causes list of vertices to be recreated before next rendering pass
    currentXResolution = -1;
    currentYResolution = -1;
}

// paint the window
void SceneOpenGL::Window::performPaint( int mask, QRegion region, WindowPaintData data )
    {
    // check if there is something to paint (e.g. don't paint if the window
    // is only opaque and only PAINT_WINDOW_TRANSLUCENT is requested)
    bool opaque = isOpaque() && data.opacity == 1.0;
    if( mask & ( PAINT_WINDOW_OPAQUE | PAINT_WINDOW_TRANSLUCENT ))
        {}
    else if( mask & PAINT_WINDOW_OPAQUE )
        {
        if( !opaque )
            return;
        }
    else if( mask & PAINT_WINDOW_TRANSLUCENT )
        {
        if( opaque )
            return;
        }
    // paint only requested areas
    if( region != infiniteRegion()) // avoid integer overflow
        region.translate( -x(), -y());
    if(( mask & ( PAINT_SCREEN_TRANSFORMED | PAINT_WINDOW_TRANSFORMED )) == 0 )
        region &= shape();
    if( region.isEmpty())
        return;
    if( !bindTexture())
        return;
    glPushMatrix();
    // set texture filter
    if( options->smoothScale != 0 ) // default to yes
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
        {
        // avoid unneeded mipmap generation by only using trilinear
        // filtering when it actually makes a difference, that is with
        // minification or changed vertices
        if( options->smoothScale == 2
            && ( verticesDirty || data.xScale < 1 || data.yScale < 1 ))
            {
            texture.setFilter( GL_LINEAR_MIPMAP_LINEAR );
            }
        else
            texture.setFilter( GL_LINEAR );
        }
    else
        texture.setFilter( GL_NEAREST );
    // do required transformations
    int x = toplevel->x();
    int y = toplevel->y();
    if( mask & PAINT_WINDOW_TRANSFORMED )
        {
        x += data.xTranslate;
        y += data.yTranslate;
        }
    glTranslatef( x, y, 0 );
    if(( mask & PAINT_WINDOW_TRANSFORMED ) && ( data.xScale != 1 || data.yScale != 1 ))
        glScalef( data.xScale, data.yScale, 1 );

    if(shader)
        prepareShaderRenderStates( mask, data );
    else
        prepareRenderStates( mask, data );
    texture.bind();
    texture.enableUnnormalizedTexCoords();

    // Render geometry
    renderGeometry( mask, region );

    texture.disableUnnormalizedTexCoords();
    glPopMatrix();

    if(shader)
        restoreShaderRenderStates( mask, data );
    else
        restoreRenderStates( mask, data );
    texture.unbind();
    }

void SceneOpenGL::Window::prepareShaderRenderStates( int mask, WindowPaintData data )
    {
    Q_UNUSED( mask );
    // setup blending of transparent windows
    glPushAttrib( GL_ENABLE_BIT );
    bool opaque = isOpaque() && data.opacity == 1.0;
    if( !opaque )
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
    shader->setUniform("opacity", (float)data.opacity);
    shader->setUniform("saturation", (float)data.saturation);
    shader->setUniform("brightness", (float)data.brightness);
    }

void SceneOpenGL::Window::prepareRenderStates( int mask, WindowPaintData data )
    {
    Q_UNUSED( mask );
    // setup blending of transparent windows
    glPushAttrib( GL_ENABLE_BIT );
    bool opaque = isOpaque() && data.opacity == 1.0;
    if( !opaque )
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
    if( data.saturation != 1.0 && texture.saturationSupported())
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
        texture.bind();

        // Then we take dot product of the result of previous pass and
        //  saturation_constant. This gives us completely unsaturated
        //  (greyscale) image
        // Note that both operands have to be in range [0.5; 1] since opengl
        //  automatically substracts 0.5 from them
        glActiveTexture( GL_TEXTURE1 );
        float saturation_constant[] = { 0.5 + 0.5*0.30, 0.5 + 0.5*0.59, 0.5 + 0.5*0.11, data.saturation };
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
        glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
        glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT );
        glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
        glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, saturation_constant );
        texture.bind();

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
        glColor4f( data.opacity, data.opacity, data.opacity, data.opacity );
        texture.bind();

        if( toplevel->hasAlpha() || data.brightness != 1.0f )
            {
            glActiveTexture( GL_TEXTURE3 );
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR );
            glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR );
            if( toplevel->hasAlpha() )
                {
                // The color has to be multiplied by both opacity and brightness
                float opacityByBrightness = data.opacity * data.brightness;
                glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness, data.opacity );
                // Also multiply original texture's alpha by our opacity
                glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE0 );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA );
                }
            else
                {
                // Color has to be multiplied only by brightness
                glColor4f( data.brightness, data.brightness, data.brightness, data.opacity );
                // Alpha will be taken from previous stage
                glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
                glTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS );
                glTexEnvi( GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA );
                }
            texture.bind();
            }

        glActiveTexture(GL_TEXTURE0 );
        }
    else if( data.opacity != 1.0 || data.brightness != 1.0 )
        {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        if( toplevel->hasAlpha())
            {
            float opacityByBrightness = data.opacity * data.brightness;
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
            glColor4f( opacityByBrightness, opacityByBrightness, opacityByBrightness,
                data.opacity);
            }
        else
            {
            // Multiply color by brightness and replace alpha by opacity
            float constant[] = { data.brightness, data.brightness, data.brightness, data.opacity };
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
    }

void SceneOpenGL::Window::renderGeometry( int mask, QRegion region )
    {
    // Enable arrays
    glEnableClientState( GL_VERTEX_ARRAY );
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), verticeslist[0].pos);
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), verticeslist[0].texcoord);

    // Render
    if( mask & ( PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED ))
        // Just draw the entire window, no clipping
        glDrawArrays( GL_QUADS, 0, verticeslist.count() );
    else
        {
        // Make sure there's only a single quad (no transformed vertices)
        // Clip using scissoring
        glEnable( GL_SCISSOR_TEST );
        region.translate( toplevel->x(), toplevel->y() );  // Back to screen coords
        int dh = displayHeight();
        foreach( QRect r, region.rects())
            {
            // Scissor rect has to be given in OpenGL coords
            glScissor(r.x(), dh - r.y() - r.height(), r.width(), r.height());
            glDrawArrays( GL_QUADS, 0, verticeslist.count() );
            }
        glDisable( GL_SCISSOR_TEST );
        }

    // Disable arrays
    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    }

void SceneOpenGL::Window::restoreShaderRenderStates( int mask, WindowPaintData data )
    {
    Q_UNUSED( mask );
    Q_UNUSED( data );
    glPopAttrib();  // ENABLE_BIT
    }

void SceneOpenGL::Window::restoreRenderStates( int mask, WindowPaintData data )
    {
    Q_UNUSED( mask );
    if( data.opacity != 1.0 || data.saturation != 1.0 || data.brightness != 1.0f )
        {
        if( data.saturation != 1.0 && texture.saturationSupported())
            {
            glActiveTexture(GL_TEXTURE3);
            glDisable( texture.target());
            glActiveTexture(GL_TEXTURE2);
            glDisable( texture.target());
            glActiveTexture(GL_TEXTURE1);
            glDisable( texture.target());
            glActiveTexture(GL_TEXTURE0);
            }
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0, 0, 0, 0 );
        }

    glPopAttrib();  // ENABLE_BIT
    }

} // namespace

#endif
