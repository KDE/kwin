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
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    private:
        void initBuffer();
        bool findConfig( const int* attrs, GLXFBConfig& config, VisualID visual = None );
        typedef GLuint Texture;
        GC gcroot;
        Drawable buffer;
        GLXFBConfig fbcroot;
        bool root_db;
        static GLXFBConfig fbcdrawable;
        static GLXDrawable glxroot;
        static GLXContext context;
        static bool tfp_mode;
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
        void setDepth( int depth );
        void draw();
        bool isVisible() const;
        bool isOpaque() const;
        void bindTexture();
        QRegion shape() const;
        void discardTexture();
        void discardShape();
        Window() {} // QMap sucks even in Qt4
    private:
        Toplevel* toplevel;
        Texture texture;
        bool texture_y_inverted;
        Pixmap bound_pixmap;
        GLXPixmap bound_glxpixmap; // only for tfp_mode
        mutable QRegion shape_region;
        mutable bool shape_valid;
        int depth;
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
    
} // namespace

#endif
