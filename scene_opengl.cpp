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

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

const int root_attrs[] =
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 16,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
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
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
    None
    };

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    {
    // TODO add checks where needed
    int dummy;
    if( !glXQueryExtension( display(), &dummy, &dummy ))
        return;
    XGCValues gcattr;
    gcattr.subwindow_mode = IncludeInferiors;
    gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
    buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
        QX11Info::appDepth());
    GLXFBConfig* fbconfigs = glXChooseFBConfig( display(), DefaultScreen( display()),
        root_attrs, &dummy );
    fbcroot = fbconfigs[ 0 ];
    XFree( fbconfigs );
    fbconfigs = glXChooseFBConfig( display(), DefaultScreen( display()),
        drawable_attrs, &dummy );
    fbcdrawable = fbconfigs[ 0 ];
    XFree( fbconfigs );
    glxroot = glXCreatePixmap( display(), fbcroot, buffer, NULL );
    context = glXCreateNewContext( display(), fbcroot, GLX_RGBA_TYPE, NULL, GL_FALSE );
    glXMakeContextCurrent( display(), glxroot, glxroot, context );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, displayWidth(), 0, displayHeight(), 0, 65535 );
// TODO    glEnable( GL_DEPTH_TEST );
    }

SceneOpenGL::~SceneOpenGL()
    {
    glXDestroyPixmap( display(), glxroot );
    XFreeGC( display(), gcroot );
    XFreePixmap( display(), buffer );
    glXDestroyContext( display(), context );
    }

static void quadDraw( int x, int y, int w, int h )
    {
    glTexCoord2i( x, y );
    glVertex2i( x, y );
    glTexCoord2i( x + w, y );
    glVertex2i( x + w, y );
    glTexCoord2i( x + w, y + h );
    glVertex2i( x + w, y + h );
    glTexCoord2i( x, y + h );
    glVertex2i( x, y + h );
    }

GLuint txts[ 100 ];
int txts_i = 0;
GLXDrawable drws[ 100 ];
int drws_i;
    
void SceneOpenGL::paint( XserverRegion, ToplevelList windows )
    {
    glClearColor( 0, 0, 0, 1 );
    glClear( GL_COLOR_BUFFER_BIT /* TODO| GL_DEPTH_BUFFER_BIT*/ );
    txts_i = 0;
    drws_i = 0;
    for( ToplevelList::ConstIterator it = windows.begin();
         it != windows.end();
         ++it )
        {
        QRect r = (*it)->geometry().intersect( QRect( 0, 0, displayWidth(), displayHeight()));
        if( !r.isEmpty())
            {
            GLXDrawable gldraw = glXCreatePixmap( display(), fbcdrawable,
                (*it)->windowPixmap(), NULL );
            glXMakeContextCurrent( display(), gldraw, gldraw, context );
            glReadBuffer( GL_FRONT );
            glDrawBuffer( GL_FRONT );
            // TODO grabXServer();
            glXWaitX();
            GLuint texture;
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            glCopyTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
                0, 0, (*it)->width(), (*it)->height(), 0 );
            //ungrabXServer();
            glXMakeContextCurrent( display(), glxroot, glxroot, context );
            glDrawBuffer( GL_BACK );
            glPushMatrix();
            // TODO Y axis in opengl grows up apparently
            glTranslatef( (*it)->x(), displayHeight() - (*it)->height() - (*it)->y(), 0 );
            glEnable( GL_TEXTURE_RECTANGLE_ARB );
            glBegin( GL_QUADS );
            quadDraw( 0, 0, (*it)->width(), (*it)->height());
            glEnd();
            glPopMatrix();
            glDisable( GL_TEXTURE_RECTANGLE_ARB );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );
            glXWaitGL();
            txts[ txts_i++ ] = texture;
            drws[ drws_i++ ] = gldraw;
            }
        }
    glFlush();
    XCopyArea( display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0 );
    XFlush( display());
    for( int i = 0;
         i < txts_i;
         ++i )
        glDeleteTextures( 1, &txts[ i ] );
    for( int i = 0;
         i < drws_i;
         ++i )
        glXDestroyPixmap( display(), drws[ i ] );
    }

} // namespace
