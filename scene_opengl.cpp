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
        - OpenGL Redbook
        - GLX docs
        - extensions docs

- glcompmgr
    - http://lists.freedesktop.org/archives/xorg/2006-July/017006.html ,
    - http://www.mail-archive.com/compiz%40lists.freedesktop.org/msg00023.html
    - simple and easy to understand
    - works even without texture_from_pixmap extension
    - claims to support several different gfx cards

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
    - svn co svn://metascape.afraid.org/svnroot/beryl

- libcm (metacity)
    - cvs -d :pserver:anonymous@anoncvs.gnome.org:/cvs/gnome co libcm
    - not much idea about it, the model differs a lot from KWin/Compiz/Beryl
    - does not seem to be very powerful or with that much development going on

*/

#include "scene_opengl.h"

#include "utils.h"
#include "client.h"
#include "effects.h"

#include <dlfcn.h>

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

// the config used for windows
GLXFBConfig SceneOpenGL::fbcdrawable;
// GLX content
GLXContext SceneOpenGL::context;
// the destination drawable where the compositing is done
GLXDrawable SceneOpenGL::glxroot;
bool SceneOpenGL::tfp_mode; // using glXBindTexImageEXT (texture_from_pixmap)
bool SceneOpenGL::root_db; // destination drawable is double-buffered
bool SceneOpenGL::copy_buffer_hack; // workaround for nvidia < 1.0-9xxx drivers

// finding of OpenGL extensions functions
typedef void (*glXFuncPtr)();
typedef glXFuncPtr (*glXGetProcAddress_func)( const GLubyte* );

static glXFuncPtr getProcAddress( const char* name )
    {
    glXFuncPtr ret = NULL;
    if( glXGetProcAddress != NULL )
        ret = glXGetProcAddress( ( const GLubyte* ) name );
    if( ret == NULL )
        ret = ( glXFuncPtr ) dlsym( RTLD_DEFAULT, name );
    return ret;
    }

// texture_from_pixmap extension functions
typedef void (*glXBindTexImageEXT_func)( Display* dpy, GLXDrawable drawable,
    int buffer, const int* attrib_list );
typedef void (*glXReleaseTexImageEXT_func)( Display* dpy, GLXDrawable drawable, int buffer );
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXGetProcAddress_func glXGetProcAddress;
glXBindTexImageEXT_func glXBindTexImageEXT;

// detect OpenGL error (add to various places in code to pinpoint the place)
static void checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) << endl;
    }

// attributes for finding a double-buffered root window config
const int root_db_attrs[] =
    {
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    None
    };

// attributes for finding a non-double-buffered root window config
static const int root_buffer_attrs[] =
    {
    GLX_DOUBLEBUFFER, False,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    None
    };

// attributes for finding config for windows
const int drawable_attrs[] = 
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    None
    };

// attributes for finding config for windows when using tfp
const int drawable_tfp_attrs[] = 
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
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
    // handle OpenGL extensions functions
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddress" );
    if( glXGetProcAddress == NULL )
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddressARB" );
    glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress( "glXBindTexImageEXT" );
    glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress( "glXReleaseTexImageEXT" );
    tfp_mode = ( glXBindTexImageEXT != NULL && glXReleaseTexImageEXT != NULL );
    // use copy buffer hack from glcompmgr (called COPY_BUFFER there) - nvidia drivers older than
    // 1.0-9xxx don't update pixmaps properly, so do a copy first
    copy_buffer_hack = !tfp_mode; // TODO detect that it's nvidia < 1.0-9xxx driver
    initBuffer(); // create destination buffer
    if( tfp_mode )
        {
        if( !findConfig( drawable_tfp_attrs, fbcdrawable ))
            {
            tfp_mode = false;
            if( !findConfig( drawable_attrs, fbcdrawable ))
                assert( false );
            }
        }
    else
        if( !findConfig( drawable_attrs, fbcdrawable ))
            assert( false );
    context = glXCreateNewContext( display(), fbcroot, GLX_RGBA_TYPE, NULL, GL_FALSE );
    glXMakeContextCurrent( display(), glxroot, glxroot, context );
    // OpenGL scene setup
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, displayWidth(), 0, displayHeight(), 0, 65535 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    checkGLError( "Init" );
    kDebug() << "Root DB:" << root_db << ", TFP:" << tfp_mode << endl;
    }

SceneOpenGL::~SceneOpenGL()
    {
    for( QMap< Toplevel*, Window >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        (*it).free();
    if( root_db )
        glXDestroyWindow( display(), glxroot );
    else
        {
        glXDestroyPixmap( display(), glxroot );
        XFreeGC( display(), gcroot );
        XFreePixmap( display(), buffer );
        }
    glXDestroyContext( display(), context );
    checkGLError( "Cleanup" );
    }

// create destination buffer
void SceneOpenGL::initBuffer()
    {
    XWindowAttributes attrs;
    XGetWindowAttributes( display(), rootWindow(), &attrs );
    if( findConfig( root_db_attrs, fbcroot, XVisualIDFromVisual( attrs.visual )))
        root_db = true;
    else
        {
        if( findConfig( root_buffer_attrs, fbcroot ))
            root_db = false;
        else
            assert( false );
        }
    if( root_db )
        {
        // root window is double-buffered, paint directly to it
        buffer = rootWindow();
        glxroot = glXCreateWindow( display(), fbcroot, buffer, NULL );
        glDrawBuffer( GL_BACK );
        }
    else
        {
        // no double-buffered root, paint to a buffer and copy to root window
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
        buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
            QX11Info::appDepth());
        glxroot = glXCreatePixmap( display(), fbcroot, buffer, NULL );
        }
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
            kDebug() << "ATTR: 0x" << QString::number( attrs[ pos ], 16 )
                << ": 0x" << QString::number( attrs[ pos + 1 ], 16 )
                << ": 0x" << QString::number( value, 16 ) << endl;
        else
            kDebug() << "ATTR FAIL: 0x" << QString::number( attrs[ pos ], 16 ) << endl;
        pos += 2;
        }
    }

// find config matching the given attributes and possibly the given X visual
bool SceneOpenGL::findConfig( const int* attrs, GLXFBConfig& config, VisualID visual )
    {
    int cnt;
    GLXFBConfig* fbconfigs = glXChooseFBConfig( display(), DefaultScreen( display()),
        attrs, &cnt );
    if( fbconfigs != NULL )
        {
        if( visual == None )
            {
            config = fbconfigs[ 0 ];
            kDebug() << "Found FBConfig" << endl;
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
                    kDebug() << "Found FBConfig" << endl;
                    config = fbconfigs[ i ];
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
        kDebug() << "Listing FBConfig:" << i << endl;
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
    glClearColor( 0, 0, 0, 1 );
    glClear( GL_COLOR_BUFFER_BIT );
    // OpenGL has (0,0) in the bottom-left corner while X has it in the top-left corner,
    // which is annoying and confusing. Therefore flip the whole OpenGL scene upside down
    // and move it up, so that it actually uses the same coordinate system like X.
    glScalef( 1, -1, 1 );
    glTranslatef( 0, -displayHeight(), 0 );
    int mask = 0;
    paintScreen( &mask, &damage ); // call generic implementation
    glPopMatrix();
    // TODO only partial repaint for mask & PAINT_SCREEN_REGION
    if( root_db )
        glXSwapBuffers( display(), glxroot );
    else
        {
        glFlush();
        glXWaitGL();
        XCopyArea( display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0 );
        XFlush( display());
        }
    ungrabXServer();
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

// the optimized case without any transformations at all
void SceneOpenGL::paintSimpleScreen( int mask, QRegion region )
    {
    // TODO repaint only damaged areas (means also don't do glXSwapBuffers and similar)
    // For now always force redrawing of the whole area.
    region = QRegion( 0, 0, displayWidth(), displayHeight());
    Scene::paintSimpleScreen( mask, region );
    }

void SceneOpenGL::paintBackground( QRegion )
    {
// TODO?
    }

void SceneOpenGL::postPaint()
    {
    checkGLError( "PostPaint" );
    Scene::postPaint();
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
        && !tfp_mode ) // TODO interestingly this makes tfp slower with some gfx cards
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
        glXWaitX();
        }
    if( tfp_mode )
        { // tfp mode, simply bind the pixmap to texture
        if( texture == None )
            glGenTextures( 1, &texture );
        if( bound_pixmap != None )
            { // release old if needed
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
        glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    else
        { // non-tfp case, copy pixmap contents to a texture
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawable, pix, NULL );
        glXMakeContextCurrent( display(), pixmap, pixmap, context );
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
            if( !toplevel->damage().isEmpty())
                {
                foreach( QRect r, toplevel->damage().rects())
                    {
                    // convert to OpenGL coordinates (this is mapping
                    // the pixmap to a texture, this is not affected
                    // by transforming the OpenGL scene)
                    int gly = toplevel->height() - r.y() - r.height();
                    texture_y_inverted = false;
                    glCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                        r.x(), gly, r.x(), gly, r.width(), r.height());
                    }
                }
            }
        // the pixmap is no longer needed, the texture will be updated
        // only when the window changes anyway, so no need to cache
        // the pixmap
        glXDestroyPixmap( display(), pixmap );
        XFreePixmap( display(), pix );
        if( root_db )
            glDrawBuffer( GL_BACK );
        glXMakeContextCurrent( display(), glxroot, glxroot, context );
        }
    if( copy_buffer )
        XFreePixmap( display(), window_pix );
    }

void SceneOpenGL::Window::discardTexture()
    {
    if( texture != 0 )
        {
        if( tfp_mode )
            {
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
    bool was_blend = glIsEnabled( GL_BLEND );
    if( !opaque )
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
    if( data.opacity != 1.0 )
        {
        // the window is additionally configured to have its opacity adjusted,
        // do it
        if( toplevel->hasAlpha())
            {
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
            glColor4f( data.opacity, data.opacity, data.opacity,
                data.opacity);
            }
        else
            {
            float constant_alpha[] = { 0, 0, 0, data.opacity };
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT );
            glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_alpha );
            }
        }
    glEnable( GL_TEXTURE_RECTANGLE_ARB );
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
    if( data.opacity != 1.0 )
        {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0, 0, 0, 0 );
        }
    if( !was_blend )
        glDisable( GL_BLEND );
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );
    }

} // namespace
