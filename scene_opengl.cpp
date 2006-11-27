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
        - OpenGL Redbook (http://opengl.org/documentation/red_book/)
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
    - the community fork of Compiz
    - http://beryl-project.org
    - svn co http://svn.beryl-project.org/trunk/

- libcm (metacity)
    - cvs -d :pserver:anonymous@anoncvs.gnome.org:/cvs/gnome co libcm
    - not much idea about it, the model differs a lot from KWin/Compiz/Beryl
    - does not seem to be very powerful or with that much development going on

*/

#include "scene_opengl.h"

#include <kxerrorhandler.h>

#include "utils.h"
#include "client.h"
#include "effects.h"
#include "glutils.h"

#include <sys/ipc.h>
#include <sys/shm.h>

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

// the config used for windows
GLXFBConfig SceneOpenGL::fbcdrawable;
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
bool SceneOpenGL::supports_saturation;
bool SceneOpenGL::shm_mode;
XShmSegmentInfo SceneOpenGL::shm;


// detect OpenGL error (add to various places in code to pinpoint the place)
static void checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) << endl;
    }

// attributes for finding a double-buffered destination window config
static const int buffer_db_attrs[] =
    {
    GLX_CONFIG_CAVEAT, GLX_NONE,
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    None
    };

// attributes for finding a non-double-buffered destination pixmap config
static const int buffer_nondb_attrs[] =
    {
    GLX_CONFIG_CAVEAT, GLX_NONE,
    GLX_DOUBLEBUFFER, False,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    None
    };

// attributes for finding config for windows
const int drawable_attrs[] = 
    {
    GLX_CONFIG_CAVEAT, GLX_NONE,
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    None
    };

// attributes for finding config for windows when using tfp
const int drawable_tfp_attrs[] = 
    {
    GLX_CONFIG_CAVEAT, GLX_NONE,
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_BIND_TO_TEXTURE_RGBA_EXT, True, // additional for tfp
    None
    };

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
    int vis_buffer, vis_drawable;
    glXGetFBConfigAttrib( display(), fbcbuffer, GLX_VISUAL_ID, &vis_buffer );
    glXGetFBConfigAttrib( display(), fbcdrawable, GLX_VISUAL_ID, &vis_drawable );
    kDebug( 1212 ) << "Buffer visual: 0x" << QString::number( vis_buffer, 16 ) << ", drawable visual: 0x"
        << QString::number( vis_drawable, 16 ) << endl;
    initRenderingContext();

    // Initialize OpenGL
    initGL();
    if( db )
        glDrawBuffer( GL_BACK );

    // Check whether certain features are supported
    supports_saturation = ((hasGLExtension("GL_ARB_texture_env_crossbar")
        && hasGLExtension("GL_ARB_texture_env_dot3")) || hasGLVersion(1, 4))
        && (glTextureUnitsCount >= 4) && glActiveTexture != NULL;

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
    for( QMap< Toplevel*, Window >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        (*it).free();
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
    if( !tfp_mode && !findConfig( drawable_attrs, &fbcdrawable ))
        assert( false );
    // use copy buffer hack from glcompmgr (called COPY_BUFFER there) - nvidia drivers older than
    // 1.0-9xxx don't update pixmaps properly, so do a copy first
    copy_buffer_hack = !tfp_mode && !shm_mode; // TODO detect that it's nvidia < 1.0-9xxx driver
    }

bool SceneOpenGL::initTfp()
    {
    if( glXBindTexImageEXT == NULL || glXReleaseTexImageEXT == NULL )
        return false;
    if( !findConfig( drawable_tfp_attrs, &fbcdrawable ))
        return false;
    return true;
    }

bool SceneOpenGL::initShm()
    {
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
    }

void SceneOpenGL::cleanupShm()
    {
    shmdt( shm.shmaddr );
#ifndef __linux__
    shmctl( shm.shmid, IPC_RMID, 0 );
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
        ctxdrawable = glXCreateNewContext( display(), fbcdrawable, GLX_RGBA_TYPE, ctxbuffer,
            direct_rendering ? GL_TRUE : GL_FALSE );
        }
    }

// create destination buffer
void SceneOpenGL::initBuffer()
    {
    if( findConfig( buffer_db_attrs, &fbcbuffer ) && wspace->createOverlay())
        { // we have overlay, try to create double-buffered window in it
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
    else if( findConfig( buffer_nondb_attrs, &fbcbuffer ))
        { // cannot get any double-buffered drawable, will double-buffer using a pixmap
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

// print info about found configs
static void debugFBConfig( GLXFBConfig* fbconfigs, int i, const int* attrs )
    {
    int pos = 0;
    while( attrs[ pos ] != (int)None )
        {
        int value;
        if( glXGetFBConfigAttrib( display(), fbconfigs[ i ], attrs[ pos ], &value )
            == Success )
            kDebug( 1212 ) << "ATTR: 0x" << QString::number( attrs[ pos ], 16 )
                << ": 0x" << QString::number( attrs[ pos + 1 ], 16 )
                << ": 0x" << QString::number( value, 16 ) << endl;
        else
            kDebug( 1212 ) << "ATTR FAIL: 0x" << QString::number( attrs[ pos ], 16 ) << endl;
        pos += 2;
        }
    }

// find config matching the given attributes and possibly the given X visual
bool SceneOpenGL::findConfig( const int* attrs, GLXFBConfig* config, VisualID visual )
    {
    int cnt;
    GLXFBConfig* fbconfigs = glXChooseFBConfig( display(), DefaultScreen( display()),
        attrs, &cnt );
    if( fbconfigs != NULL )
        {
        if( visual == None )
            {
            *config = fbconfigs[ 0 ];
            kDebug( 1212 ) << "Found FBConfig" << endl;
            debugFBConfig( fbconfigs, 0, attrs );
            XFree( fbconfigs );
            return true;
            }
        else
            {
            for( int i = 0;
                 i < cnt;
                 ++i )
                {
                int value;
                glXGetFBConfigAttrib( display(), fbconfigs[ i ], GLX_VISUAL_ID, &value );
                if( value == (int)visual )
                    {
                    kDebug( 1212 ) << "Found FBConfig" << endl;
                    *config = fbconfigs[ i ];
                    debugFBConfig( fbconfigs, i, attrs );
                    XFree( fbconfigs );
                    return true;
                    }
                }
            }
        }
#if 0 // for debug
    fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display()), &cnt );
    for( int i = 0;
         i < cnt;
         ++i )
        {
        kDebug( 1212 ) << "Listing FBConfig:" << i << endl;
        debugFBConfig( fbconfigs, i, attrs );
        }
    if( fbconfigs != NULL )
        XFree( fbconfigs );
#endif
    return false;
    }

// the entry function for painting
void SceneOpenGL::paint( QRegion damage, ToplevelList toplevels )
    {
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( &windows[ c ] );
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
    bool vsync = options->glVSync;
    unsigned int sync;

    if( !vsync )
        return;
    if( glXGetVideoSync )
        {
        glFlush();
        glXGetVideoSync( &sync );
        glXWaitVideoSync( 2, ( sync + 1 ) % 2, &sync );
        }
    }

// actually paint to the screen (double-buffer swap or copy from pixmap buffer)
void SceneOpenGL::flushBuffer( int mask, QRegion damage )
    {
    if( mask & PAINT_SCREEN_REGION )//    make sure not to go outside visible screen
        damage &= QRegion( 0, 0, displayWidth(), displayHeight());
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
                foreach( QRect r, damage.rects())
                    {
                    // convert to OpenGL coordinates
                    int y = displayHeight() - r.y() - r.height();
                    glRasterPos2f( r.x(), r.y() + r.height());
                    glScissor( r.x(), y, r.width(), r.height());
                    glCopyPixels( r.x(), y, r.width(), r.height(), GL_COLOR );
                    }
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
    windows[ c ] = Window( c );
    }

void SceneOpenGL::windowDeleted( Toplevel* c )
    {
    assert( windows.contains( c ));
    windows[ c ].free();
    windows.remove( c );
    }

void SceneOpenGL::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid
        return;                 // by default
    Window& w = windows[ c ];
    w.discardShape();
    w.discardTexture();
    }

void SceneOpenGL::windowOpacityChanged( Toplevel* )
    {
#if 0 // not really needed, windows are painted on every repaint
      // and opacity is used when applying texture, not when
      // creating it
    if( !windows.contains( c )) // this is ok, texture is created
        return;                 // on demand
    Window& w = windows[ c ];
    w.discardTexture();
#endif
    }

//****************************************
// SceneOpenGL::Window
//****************************************

SceneOpenGL::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , texture( 0 )
    , texture_y_inverted( false )
    , bound_pixmap( None )
    , bound_glxpixmap( None )
    {
    }

void SceneOpenGL::Window::free()
    {
    discardTexture();
    }

// Bind the window pixmap to an OpenGL texture.
void SceneOpenGL::Window::bindTexture()
    {
    if( texture != 0 && toplevel->damage().isEmpty()
        && !options->glAlwaysRebind ) // interestingly with some gfx cards always rebinding is faster
        {
        // texture doesn't need updating, just bind it
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        return;
        }
    // Get the pixmap with the window contents
    Pixmap window_pix = toplevel->createWindowPixmap();
    Pixmap pix = window_pix;
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
    if( shm_mode )
        { // non-tfp case, copy pixmap contents to a texture
        if( texture == None )
            {
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            texture_y_inverted = false;
            glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, toplevel->width(), toplevel->height(),
                0, GL_BGRA, GL_UNSIGNED_BYTE, NULL );
            // TODO hasAlpha() ?
//            glCopyTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
//                toplevel->hasAlpha() ? GL_RGBA : GL_RGB,
//                0, 0, toplevel->width(), toplevel->height(), 0 );
            }
        else
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        if( !toplevel->damage().isEmpty())
            {
            XGCValues xgcv;
            xgcv.graphics_exposures = False;
            xgcv.subwindow_mode = IncludeInferiors;
            GC gc = XCreateGC( display(), pix, GCGraphicsExposures | GCSubwindowMode, &xgcv );
            QRegion damage = optimizeBindDamage( toplevel->damage(), 100 * 100 );
            foreach( QRect r, damage.rects())
                { // TODO for small areas it might be faster to not use SHM to avoid the XSync()
                Pixmap p = XShmCreatePixmap( display(), rootWindow(), shm.shmaddr, &shm,
                    r.width(), r.height(), toplevel->depth());
                XCopyArea( display(), pix, p, gc, r.x(), r.y(), r.width(), r.height(), 0, 0 );
                XSync( display(), False );
                glXWaitX();
                glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                    r.x(), r.y(), r.width(), r.height(), GL_BGRA, GL_UNSIGNED_BYTE, shm.shmaddr );
                glXWaitGL();
                XFreePixmap( display(), p );
                }
            XFreeGC( display(), gc );
            }
        // the pixmap is no longer needed, the texture will be updated
        // only when the window changes anyway, so no need to cache
        // the pixmap
        XFreePixmap( display(), pix );
        texture_y_inverted = true;
        toplevel->resetDamage( toplevel->rect());
        }
    else if( tfp_mode )
        { // tfp mode, simply bind the pixmap to texture
        if( texture == None )
            glGenTextures( 1, &texture );
        if( bound_pixmap != None && !strict_binding ) // release old if needed
            {
            glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            XFreePixmap( display(), bound_pixmap );
            }
        static const int attrs[] =
            {
            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
            None
            };
        bound_pixmap = pix;
        bound_glxpixmap = glXCreatePixmap( display(), fbcdrawable, pix, attrs );
        int value;
        glXGetFBConfigAttrib( display(), fbcdrawable, GLX_Y_INVERTED_EXT, &value );
        texture_y_inverted = value ? true : false;
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        if( !strict_binding )
            glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        toplevel->resetDamage( toplevel->rect());
        }
    else
        { // non-tfp case, copy pixmap contents to a texture
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawable, pix, NULL );
        glXMakeContextCurrent( display(), pixmap, pixmap, ctxdrawable );
        if( last_pixmap != None )
            glXDestroyPixmap( display(), last_pixmap );
        // workaround for ATI - it leaks/crashes when the pixmap is destroyed immediately
        // here (http://lists.kde.org/?l=kwin&m=116353772208535&w=2)
        last_pixmap = pixmap;
        glReadBuffer( GL_FRONT );
        glDrawBuffer( GL_FRONT );
        if( texture == None )
            {
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            texture_y_inverted = false;
            glCopyTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                toplevel->hasAlpha() ? GL_RGBA : GL_RGB,
                0, 0, toplevel->width(), toplevel->height(), 0 );
            }
        else
            {
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            QRegion damage = optimizeBindDamage( toplevel->damage(), 30 * 30 );
            foreach( QRect r, damage.rects())
                {
                // convert to OpenGL coordinates (this is mapping
                // the pixmap to a texture, this is not affected
                // by using glOrtho() for the OpenGL scene)
                int gly = toplevel->height() - r.y() - r.height();
                glCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                    r.x(), gly, r.x(), gly, r.width(), r.height());
                }
            }
        glXWaitGL();
        // the pixmap is no longer needed, the texture will be updated
        // only when the window changes anyway, so no need to cache
        // the pixmap
        XFreePixmap( display(), pix );
        if( db )
            glDrawBuffer( GL_BACK );
        glXMakeContextCurrent( display(), glxbuffer, glxbuffer, ctxbuffer );
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        texture_y_inverted = false;
        toplevel->resetDamage( toplevel->rect());
        }
    if( copy_buffer )
        XFreePixmap( display(), window_pix );
    }


QRegion SceneOpenGL::Window::optimizeBindDamage( const QRegion& reg, int limit )
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

void SceneOpenGL::Window::enableTexture()
    {
    glEnable( GL_TEXTURE_RECTANGLE_ARB );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
    if( tfp_mode && strict_binding )
        {
        assert( bound_pixmap != None );
        glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    }

void SceneOpenGL::Window::disableTexture()
    {
    if( tfp_mode && strict_binding )
        {
        assert( bound_pixmap != None );
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
        }
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    }

void SceneOpenGL::Window::discardTexture()
    {
    if( texture != 0 )
        {
        if( tfp_mode )
            {
            if( !strict_binding )
                glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            XFreePixmap( display(), bound_pixmap );
            bound_pixmap = None;
            bound_glxpixmap = None;
            }
        glDeleteTextures( 1, &texture );
        }
    texture = 0;
    }

// paint a quad (rectangle), ty1/ty2 are texture coordinates (for handling
// swapped y coordinate, see below)
static void quadPaint( int x1, int y1, int x2, int y2, int ty1, int ty2 )
    {
    glTexCoord2i( x1, ty1 );
    glVertex2i( x1, y1 );
    glTexCoord2i( x2, ty1 );
    glVertex2i( x2, y1 );
    glTexCoord2i( x2, ty2 );
    glVertex2i( x2, y2 );
    glTexCoord2i( x1, ty2 );
    glVertex2i( x1, y2 );
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
    region &= shape();
    if( region.isEmpty())
        return;
    bindTexture();
    glPushMatrix();
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
    // setup blending of transparent windows
    glPushAttrib( GL_ENABLE_BIT );
    if( !opaque )
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
    if( data.saturation != 1.0 && supports_saturation )
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
        enableTexture();

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
        enableTexture();

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
        enableTexture();

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
            enableTexture();
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
    enableTexture();
    // actually paint the window
    glBegin( GL_QUADS );
    foreach( QRect r, region.rects())
        {
        int y1 = r.y();
        int y2 = r.y() + r.height();
        int ty1 = y1;
        int ty2 = y2;
        // tfp can result in the texture having y coordinate inverted (because
        // of the internal format), so do the inversion if needed
        if( !texture_y_inverted ) // "!" because of converting to OpenGL coords
            {
            ty1 = height() - y1;
            ty2 = height() - y2;
            }
        quadPaint( r.x(), y1, r.x() + r.width(), y2, ty1, ty2 );
        }
    glEnd();
    glPopMatrix();
    if( data.opacity != 1.0 || data.saturation != 1.0 || data.brightness != 1.0f )
        {
        if( data.saturation != 1.0 && supports_saturation )
            {
            glActiveTexture(GL_TEXTURE3);
            glDisable( GL_TEXTURE_RECTANGLE_ARB );
            glActiveTexture(GL_TEXTURE2);
            glDisable( GL_TEXTURE_RECTANGLE_ARB );
            glActiveTexture(GL_TEXTURE1);
            glDisable( GL_TEXTURE_RECTANGLE_ARB );
            glActiveTexture(GL_TEXTURE0);
            }
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0, 0, 0, 0 );
        }
    glPopAttrib();
    disableTexture();
    }

} // namespace
