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
        GLXContext context;
        class Window;
        QMap< Toplevel*, Window > windows;
    };

class SceneOpenGL::Window
    {
    public:
        Window( Toplevel* c );
        ~Window();
        void free(); // is often copied by value, use manually instead of dtor
        GLXPixmap glxPixmap() const;
        Texture texture() const;
        Window() {} // QMap sucks even in Qt4
    private:
        void discardPixmap();
        void discardTexture();
        Toplevel* toplevel;
        mutable GLXPixmap glxpixmap;
        mutable Texture gltexture;
    };

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
    if( gltexture != None )
        glDeleteTextures( 1, &gltexture );
    gltexture = None;
    }

} // namespace

#endif
