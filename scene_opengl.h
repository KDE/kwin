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

#include "kwinglutils.h"

#ifdef HAVE_OPENGL

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif

namespace KWin
{

class SceneOpenGL
    : public Scene
    {
    public:
        class Texture;
        class Window;
        SceneOpenGL( Workspace* ws );
        virtual ~SceneOpenGL();
        virtual bool initFailed() const;
        virtual CompositingType compositingType() const { return OpenGLCompositing; }
        virtual void paint( QRegion damage, ToplevelList windows );
        virtual void windowGeometryShapeChanged( Toplevel* );
        virtual void windowOpacityChanged( Toplevel* );
        virtual void windowAdded( Toplevel* );
        virtual void windowClosed( Toplevel*, Deleted* );
        virtual void windowDeleted( Deleted* );
    protected:
        virtual void paintGenericScreen( int mask, ScreenPaintData data );
        virtual void paintBackground( QRegion region );
    private:
        bool selectMode();
        bool initTfp();
        bool initShm();
        void cleanupShm();
        bool initBuffer();
        bool initRenderingContext();
        bool initBufferConfigs();
        bool initDrawableConfigs();
        void waitSync();
        void flushBuffer( int mask, QRegion damage );
        GC gcroot;
        class FBConfigInfo
        {
            public:
                GLXFBConfig fbconfig;
                int bind_texture_format;
                int y_inverted;
                int mipmap;
        };
        Drawable buffer;
        GLXFBConfig fbcbuffer;
        static bool db;
        static GLXFBConfig fbcbuffer_db;
        static GLXFBConfig fbcbuffer_nondb;
        static FBConfigInfo fbcdrawableinfo[ 32 + 1 ];
        static GLXDrawable glxbuffer;
        static GLXContext ctxbuffer;
        static GLXContext ctxdrawable;
        static GLXDrawable last_pixmap; // for a workaround in bindTexture()
        static bool tfp_mode;
        static bool shm_mode;
        QHash< Toplevel*, Window* > windows;
#ifdef HAVE_XSHM
        static XShmSegmentInfo shm;
#endif
        bool init_ok;
    };

class SceneOpenGL::Texture
    : public GLTexture
    {
    public:
        Texture();
        Texture( const Pixmap& pix, const QSize& size, int depth );
        virtual ~Texture();

        using GLTexture::load;
        virtual bool load( const Pixmap& pix, const QSize& size, int depth,
            QRegion region );
        virtual bool load( const Pixmap& pix, const QSize& size, int depth );
        virtual bool load( const QImage& image, GLenum target = GL_TEXTURE_2D );
        virtual bool load( const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D );
        virtual void discard();
        virtual void release(); // undo the tfp_mode binding
        virtual void bind();
        virtual void unbind();

    protected:
        void findTarget();
        QRegion optimizeBindDamage( const QRegion& reg, int limit );

    private:
        void init();

        GLXPixmap bound_glxpixmap; // the glx pixmap the texture is bound to, only for tfp_mode
    };

class SceneOpenGL::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual ~Window();
        virtual void performPaint( int mask, QRegion region, WindowPaintData data );
        virtual void prepareForPainting();
        virtual void pixmapDiscarded();
        bool bindTexture();
        void discardTexture();
        void setShader( GLShader* s )  { shader = s; }

    protected:
        void renderQuads( int mask, const QRegion& region, const WindowQuadList& quads );
        void prepareStates( double opacity, double brightness, double saturation );
        void prepareRenderStates( double opacity, double brightness, double saturation );
        void prepareShaderRenderStates( double opacity, double brightness, double saturation );
        void restoreStates( double opacity, double brightness, double saturation );
        void restoreRenderStates( double opacity, double brightness, double saturation );
        void restoreShaderRenderStates( double opacity, double brightness, double saturation );

    private:
        Texture texture;
        GLShader* shader;  // shader to be used for rendering, if any
    };

} // namespace

#endif

#endif
