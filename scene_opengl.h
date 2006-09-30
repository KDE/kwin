/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_SCENE_OPENGL_H
#define KWIN_SCENE_OPENGL_H

#include "scene.h"
#include "toplevel.h"

#include <GL/gl.h>
#include <GL/glx.h>

namespace KWinInternal
{

class SceneOpenGL
    : public Scene
    {
    public:
        SceneOpenGL( Workspace* ws );
        virtual ~SceneOpenGL();
        virtual void paint( QRegion damage, ToplevelList windows );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    private:
        typedef GLuint Texture;
        GC gcroot;
        Pixmap buffer;
        GLXFBConfig fbcroot;
        static GLXFBConfig fbcdrawable;
        GLXPixmap glxroot;
        static GLXContext context;
        class Window;
        QMap< Toplevel*, Window > windows;
    };

class SceneOpenGL::Window
    {
    public:
        Window( Toplevel* c );
        ~Window();
        void free(); // is often copied by value, use manually instead of dtor
        int glX() const; // remap to OpenGL coordinates
        int glY() const;
        int width() const;
        int height() const;
        GLXPixmap glxPixmap() const;
        void bindTexture();
        Window() {} // QMap sucks even in Qt4
    private:
        void discardPixmap();
        void discardTexture();
        Toplevel* toplevel;
        mutable GLXPixmap glxpixmap;
        Texture texture;
    };

inline
int SceneOpenGL::Window::glX() const
    {
    return toplevel->x();
    }
    
inline
int SceneOpenGL::Window::glY() const
    {
    return displayHeight() - toplevel->y() - toplevel->height();
    }

inline
int SceneOpenGL::Window::width() const
    {
    return toplevel->width();
    }
    
inline
int SceneOpenGL::Window::height() const
    {
    return toplevel->height();
    }
    
inline
void SceneOpenGL::Window::discardPixmap()
    {
    if( glxpixmap != None )
        glXDestroyPixmap( display(), glxpixmap );
    glxpixmap = None;
    }

inline
void SceneOpenGL::Window::discardTexture()
    {
    if( texture != None )
        glDeleteTextures( 1, &texture );
    texture = None;
    }

} // namespace

#endif
