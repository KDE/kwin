/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.

Based on glcompmgr code by Felix Bellaby.
******************************************************************/



#include "scene_opengl.h"

#include "utils.h"
#include "client.h"

#include <dlfcn.h>

#include <X11/extensions/shape.h>

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

GLXFBConfig SceneOpenGL::fbcdrawable;
GLXContext SceneOpenGL::context;
GLXPixmap SceneOpenGL::glxroot;
bool SceneOpenGL::tfp_mode; // using glXBindTexImageEXT (texture_from_pixmap)

typedef void (*glXBindTexImageEXT_func)( Display* dpy, GLXDrawable drawable,
    int buffer, const int* attrib_list );
typedef void (*glXReleaseTexImageEXT_func)( Display* dpy, GLXDrawable drawable, int buffer );
typedef void (*glXFuncPtr)();
typedef glXFuncPtr (*glXGetProcAddress_func)( const GLubyte* );
glXBindTexImageEXT_func glXBindTexImageEXT;
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXGetProcAddress_func glXGetProcAddress;

static void checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) << endl;
    }

const int root_db_attrs[] =
    {
    GLX_DOUBLEBUFFER, True,
    GLX_DEPTH_SIZE, 16,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    None
    };

static const int root_buffer_attrs[] =
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 16,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    None
    };

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

static glXFuncPtr getProcAddress( const char* name )
    {
    glXFuncPtr ret = NULL;
    if( glXGetProcAddress != NULL )
        ret = glXGetProcAddress( ( const GLubyte* ) name );
    if( ret == NULL )
        ret = ( glXFuncPtr ) dlsym( RTLD_DEFAULT, name );
    return ret;
    }

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    {
    // TODO add checks where needed
    int dummy;
    if( !glXQueryExtension( display(), &dummy, &dummy ))
        return;
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddress" );
    if( glXGetProcAddress == NULL )
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddressARB" );
    glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress( "glXBindTexImageEXT" );
    glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress( "glXReleaseTexImageEXT" );
    tfp_mode = ( glXBindTexImageEXT != NULL && glXReleaseTexImageEXT != NULL );
    initBuffer();
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
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, displayWidth(), 0, displayHeight(), 0, 65535 );
    glEnable( GL_DEPTH_TEST );
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
        buffer = rootWindow();
        glxroot = glXCreateWindow( display(), fbcroot, buffer, NULL );
        }
    else
        {
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
        buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
            QX11Info::appDepth());
        glxroot = glXCreatePixmap( display(), fbcroot, buffer, NULL );
        }
    }

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

void SceneOpenGL::paint( QRegion damage, ToplevelList windows )
    {
    grabXServer();
    glXWaitX();
    glPushMatrix();
    glClearColor( 0, 0, 0, 1 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glScalef( 1, -1, 1 );
    glTranslatef( 0, -displayHeight(), 0 );
    if( /*generic case*/false )
        paintGenericScreen( windows );
    else
        paintSimpleScreen( damage, windows );
    glPopMatrix();
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
    checkGLError( "PostPaint" );
    }
    
// the generic drawing code that should eventually handle even
// transformations
void SceneOpenGL::paintGenericScreen( ToplevelList windows )
    {
    int depth = 0;
    foreach( Toplevel* c, windows ) // bottom to top
        {
        assert( this->windows.contains( c ));
        Window& w = this->windows[ c ];
        w.setDepth( --depth );
        if( !w.isVisible())
            continue;
        w.bindTexture();
        if( !w.isOpaque())
            {
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            }
        w.draw();
        glDisable( GL_BLEND );
        }
    }

// the optimized case without any transformations at all
void SceneOpenGL::paintSimpleScreen( QRegion, ToplevelList windows )
    {
    int depth = 0;
    QList< Window* > phase2;
    for( int i = windows.count() - 1; // top to bottom
         i >= 0;
         --i )
        {
        Toplevel* c = windows[ i ];
        assert( this->windows.contains( c ));
        Window& w = this->windows[ c ];
        w.setDepth( --depth );
        if( !w.isVisible())
            continue;
        if( !w.isOpaque())
            {
            phase2.prepend( &w );
            continue;
            }
        w.bindTexture();
        w.draw();
        }
    foreach( Window* w2, phase2 )
        {
        Window& w = *w2;
        w.bindTexture();
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        w.draw();
        glDisable( GL_BLEND );
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

SceneOpenGL::Window::Window( Toplevel* c )
    : toplevel( c )
    , texture( 0 )
    , texture_y_inverted( false )
    , bound_pixmap( None )
    , bound_glxpixmap( None )
    , shape_valid( false )
    , depth( 0 )
    {
    }

SceneOpenGL::Window::~Window()
    {
    }
    
void SceneOpenGL::Window::free()
    {
    discardTexture();
    }

// for relative window positioning
void SceneOpenGL::Window::setDepth( int d )
    {
    depth = d;
    }

void SceneOpenGL::Window::bindTexture()
    {
    if( texture != 0 && toplevel->damage().isEmpty()
        && !tfp_mode ) // interestingly this makes tfp slower
        {
        // texture doesn't need updating, just bind it
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        return;
        }
    // TODO cache pixmaps here if possible
    Pixmap window_pix = toplevel->createWindowPixmap();
    Pixmap pix = window_pix;
    // HACK
    // When a window uses ARGB visual and has a decoration, the decoration
    // does use ARGB visual. When converting such window to a texture
    // the alpha for the decoration part is broken for some reason (undefined?).
    // I wasn't lucky converting KWin to use ARGB visuals for decorations,
    // so instead simply set alpha in those parts to opaque.
    // Without ALPHA_CLEAR_COPY the setting is done directly in the window
    // pixmap, which seems to be ok, but let's not risk trouble right now.
    // TODO check if this isn't a performance problem and how it can be done better
    Client* c = dynamic_cast< Client* >( toplevel );
    bool alpha_clear = c != NULL && c->hasAlpha() && !c->noBorder();
#define ALPHA_CLEAR_COPY
#ifdef ALPHA_CLEAR_COPY
    if( alpha_clear )
        {
        Pixmap p2 = XCreatePixmap( display(), pix, c->width(), c->height(), 32 );
        GC gc = XCreateGC( display(), pix, 0, NULL );
        XCopyArea( display(), pix, p2, gc, 0, 0, c->width(), c->height(), 0, 0 );
        pix = p2;
        XFreeGC( display(), gc );
        }
#endif
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
        {
        if( texture == None )
            glGenTextures( 1, &texture );
        if( bound_pixmap != None )
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
        // this is swapped in order to get a conversion of OpenGL coordinates
        // (binding to a texture is not affected by transforming the OpenGL scene)
        texture_y_inverted = value ? false : true;
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    else
        {
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawable, pix, NULL );
        glXMakeContextCurrent( display(), pixmap, pixmap, context );
        glReadBuffer( GL_FRONT );
        glDrawBuffer( GL_FRONT );
        if( texture == None )
            {
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            texture_y_inverted = true; // conversion to OpenGL coordinates
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
                    texture_y_inverted = true;
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
        }
#ifdef ALPHA_CLEAR_COPY
    if( alpha_clear )
        XFreePixmap( display(), window_pix );
#endif
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


void SceneOpenGL::Window::discardShape()
    {
    shape_valid = false;
    }

QRegion SceneOpenGL::Window::shape() const
    {
    if( !shape_valid )
        {
        Client* c = dynamic_cast< Client* >( toplevel );
        if( toplevel->shape() || ( c != NULL && !c->mask().isEmpty()))
            {
            int count, order;
            XRectangle* rects = XShapeGetRectangles( display(), toplevel->handle(),
                ShapeBounding, &count, &order );
            if(rects)
                {
                shape_region = QRegion();
                for( int i = 0;
                     i < count;
                     ++i )
                    shape_region += QRegion( rects[ i ].x, rects[ i ].y,
                        rects[ i ].width, rects[ i ].height );
                XFree(rects);
                }
            else
                shape_region = QRegion( 0, 0, width(), height());
            }
        else
            shape_region = QRegion( 0, 0, width(), height());
        shape_valid = true;
        }
    return shape_region;
    }

static void quadDraw( int x1, int y1, int x2, int y2, bool invert_y )
    {
    glTexCoord2i( x1, invert_y ? y2 : y1 );
    glVertex2i( x1, y1 );
    glTexCoord2i( x2, invert_y ? y2 : y1 );
    glVertex2i( x2, y1 );
    glTexCoord2i( x2, invert_y ? y1 : y2 );
    glVertex2i( x2, y2 );
    glTexCoord2i( x1, invert_y ? y1 : y2 );
    glVertex2i( x1, y2 );
    }

void SceneOpenGL::Window::draw()
    {
// TODO for double-buffered root            glDrawBuffer( GL_BACK );
    glXMakeContextCurrent( display(), glxroot, glxroot, context );
    glPushMatrix();
    glTranslatef( x(), y(), depth );
    if( toplevel->opacity() != 1.0 )
        {
        if( toplevel->hasAlpha())
            {
            glEnable( GL_BLEND );
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
            glColor4f( toplevel->opacity(), toplevel->opacity(), toplevel->opacity(),
                toplevel->opacity());
            }
        else
            {
            float constant_alpha[] = { 0, 0, 0, toplevel->opacity() };
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT );
            glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_alpha );
            }
        }
    glEnable( GL_TEXTURE_RECTANGLE_ARB );
    glBegin( GL_QUADS );
    foreach( QRect r, shape().rects())
        {
        quadDraw( r.x(), r.y(), r.x() + r.width(), r.y() + r.height(),
            texture_y_inverted );
        }
    glEnd();
    glPopMatrix();
    if( toplevel->opacity() != 1.0 )
        {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0, 0, 0, 0 );
        glDisable( GL_BLEND );
        }
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );
    }

bool SceneOpenGL::Window::isVisible() const
    {
    // TODO mapping state?
    return !toplevel->geometry()
        .intersect( QRect( 0, 0, displayWidth(), displayHeight()))
        .isEmpty();
    }

bool SceneOpenGL::Window::isOpaque() const
    {
    return toplevel->opacity() == 1.0 && !toplevel->hasAlpha();
    }

} // namespace
