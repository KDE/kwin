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

#include <X11/extensions/XShm.h>

namespace KWinInternal
{

class SceneOpenGL
    : public Scene
    {
    public:
        SceneOpenGL( Workspace* ws );
        virtual ~SceneOpenGL();
        virtual void paint( QRegion damage, ToplevelList windows );
        virtual void postPaint();
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowDeleted( Toplevel* );
    protected:
        virtual void paintGenericScreen( int mask, ScreenPaintData data );
        virtual void paintBackground( QRegion region );
    private:
        void selectMode();
        bool initTfp();
        bool initShm();
        void cleanupShm();
        void initBuffer();
        void initRenderingContext();
        bool findConfig( const int* attrs, GLXFBConfig* config, VisualID visual = None );
        void flushBuffer( int mask, const QRegion& damage );
        typedef GLuint Texture;
        GC gcroot;
        Drawable buffer;
        GLXFBConfig fbcbuffer;
        static bool db;
        static GLXFBConfig fbcdrawable;
        static GLXDrawable glxbuffer;
        static GLXContext ctxbuffer;
        static GLXContext ctxdrawable;
        static GLXDrawable last_pixmap; // for a workaround in bindTexture()
        static bool tfp_mode;
        static bool shm_mode;
        static bool strict_binding;
        static bool copy_buffer_hack;
        static bool supports_saturation;
        class Window;
        QMap< Toplevel*, Window > windows;
        static XShmSegmentInfo shm;
    };

class SceneOpenGL::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual void free();
        virtual void performPaint( int mask, QRegion region, WindowPaintData data );
        void bindTexture();
        void enableTexture();
        void disableTexture();
        void discardTexture();
        Window() {} // QMap sucks even in Qt4
    private:
        QRegion optimizeBindDamage( const QRegion& reg, int limit );
        Texture texture;
        bool texture_y_inverted; // texture has y inverted
        Pixmap bound_pixmap; // the pixmap the texture is bound to, only for tfp_mode
        GLXPixmap bound_glxpixmap; // the glx pixmap the texture is bound to, only for tfp_mode
    };

} // namespace

#endif
