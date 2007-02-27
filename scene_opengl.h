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

#include "glutils.h"

#ifdef HAVE_OPENGL

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif

namespace KWinInternal
{

class SceneOpenGL
    : public Scene
    {
    public:
        class Window;
        SceneOpenGL( Workspace* ws );
        virtual ~SceneOpenGL();
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
        void selectMode();
        bool initTfp();
        bool initShm();
        void cleanupShm();
        void initBuffer();
        void initRenderingContext();
        bool initBufferConfigs();
        bool initDrawableConfigs();
        void waitSync();
        void flushBuffer( int mask, QRegion damage );
        typedef GLuint Texture;
        typedef GLenum Target;
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
        static bool strict_binding;
        static bool copy_buffer_hack;
        static bool supports_npot_textures;
        static bool supports_fbo;
        static bool supports_saturation;
        QMap< Toplevel*, Window > windows;
#ifdef HAVE_XSHM
        static XShmSegmentInfo shm;
#endif
    };

class SceneOpenGL::Window
    : public Scene::Window
    {
    public:
        Window( Toplevel* c );
        virtual void free();
        virtual void performPaint( int mask, QRegion region, WindowPaintData data );
        virtual void prepareForPainting();
        void findTextureTarget();
        bool bindTexture();
        void enableTexture();
        void disableTexture();
        void discardTexture();
        void discardVertices();
        Window() {} // QMap sucks even in Qt4

        /**
         * @short Vertex class
         * Vertex has position and texture coordinate which are equal at first,
         *  however effects can e.g. modify position to move the window or part of it.
         **/
        class Vertex
        {
            public:
                Vertex()  {}
                Vertex(float x, float y)
                {
                    pos[0] = texcoord[0] = x; pos[1] = texcoord[1] = y; pos[2] = 0.0f;
                }
                Vertex(float x, float y, float u, float v)
                {
                    pos[0] = x; pos[1] = y; pos[2] = 0.0f; texcoord[0] = u; texcoord[1] = v;
                }
                float pos[3];
                float texcoord[2];
        };
        // Returns list of vertices
        QVector<Vertex>& vertices()  { return verticeslist; }
        // Can be called in pre-paint pass. Makes sure that all quads that the
        //  window consists of are not bigger than maxquadsize x maxquadsize
        //  (in pixels) in the following paint pass.
        void requestVertexGrid(int maxquadsize);
        // Marks vertices of the window as dirty. Call this if you change
        //  position of the vertices
        void markVerticesDirty()  { verticesDirty = true; }
    protected:
        // Makes sure that vertex grid requests are fulfilled and that vertices
        //  aren't dirty. Call this before paint pass
        void prepareVertices();
        void createVertexGrid(int xres, int yres);
        void resetVertices();  // Resets positions of vertices

        void prepareRenderStates( int mask, WindowPaintData data );
        void renderGeometry( int mask, QRegion region );
        void restoreRenderStates( int mask, WindowPaintData data );

    private:
        QRegion optimizeBindDamage( const QRegion& reg, int limit );
        Texture texture;
        Target texture_target;
        float texture_scale_x, texture_scale_y; // to un-normalize GL_TEXTURE_2D
        bool texture_y_inverted; // texture has y inverted
        bool texture_can_use_mipmaps;
        bool texture_has_valid_mipmaps;
        bool texture_filter_trilinear;
        GLXPixmap bound_glxpixmap; // the glx pixmap the texture is bound to, only for tfp_mode

        QVector<Vertex> verticeslist;
        // Maximum size of the biggest quad that window currently has, in pixels
        int currentXResolution;
        int currentYResolution;
        // Requested maximum size of the biggest quad that window would have
        //  during the next paint pass, in pixels
        int requestedXResolution;
        int requestedYResolution;
        bool verticesDirty;  // vertices have been modified in some way
    };

} // namespace

#endif

#endif
