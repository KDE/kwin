/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_GLUTILS_H
#define KWIN_GLUTILS_H

#include <kwinconfig.h> // KWIN_HAVE_OPENGL

#ifdef KWIN_HAVE_OPENGL
#include <kwinglutils_funcs.h>

#include <QtGui/QPixmap>

#include <QtGui/QImage>
#include <QtCore/QSize>
#include <QtCore/QSharedData>

/** @addtogroup kwineffects */
/** @{ */


template< class K, class V > class QHash;


namespace KWin
{


class GLTexture;


// Initializes GLX function pointers
void KWIN_EXPORT initGLX();
// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
void KWIN_EXPORT initGL();


// Number of supported texture units
extern KWIN_EXPORT int glTextureUnitsCount;


bool KWIN_EXPORT hasGLVersion(int major, int minor, int release = 0);
bool KWIN_EXPORT hasGLXVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool KWIN_EXPORT hasGLExtension(const QString& extension);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool KWIN_EXPORT checkGLError( const char* txt );

inline bool KWIN_EXPORT isPowerOfTwo( int x ) { return (( x & ( x - 1 )) == 0 ); }
/**
 * @return power of two integer _greater or equal to_ x.
 *  E.g. nearestPowerOfTwo(513) = nearestPowerOfTwo(800) = 1024
 **/
int KWIN_EXPORT nearestPowerOfTwo( int x );

/**
 * Renders quads using given vertices.
 * If texture is not 0, each texture coordinate much have two components (st).
 * If color is not 0, each color much have four components (rgba).
 * Note that texture coordinates must match texture type (normalized/unnormalized
 * for GL_TEXTURE_2D/GL_TEXTURE_ARB).
 *
 * @param count number of vertices to use.
 * @param dim number of components per vertex coordinate in vertices array.
 * @param stride byte offset of consecutive elements in arrays. If 0, then
 *  arrays must be tighly packed. Stride must be a multiple of sizeof(float)!
 **/
KWIN_EXPORT void renderGLGeometry( const QRegion& region, int count,
    const float* vertices, const float* texture = 0, const float* color = 0,
    int dim = 2, int stride = 0 );
/**
 * Same as above, renders without specified region
 **/
KWIN_EXPORT void renderGLGeometry( int count,
    const float* vertices, const float* texture = 0, const float* color = 0,
    int dim = 2, int stride = 0 );


KWIN_EXPORT void renderGLGeometryImmediate( int count,
    const float* vertices, const float* texture = 0, const float* color = 0,
    int dim = 2, int stride = 0 );


KWIN_EXPORT void addQuadVertices( QVector<float>& verts, float x1, float y1, float x2, float y2 );


/**
 * @deprecated
 * @see EffectFrame
 */
KWIN_EXPORT void renderRoundBox( const QRect& area, float roundness = 10.0f, GLTexture* texture = 0 );
/**
 * @deprecated
 * @see EffectFrame
 */
KWIN_EXPORT void renderRoundBoxWithEdge( const QRect& area, float roundness = 10.0f );


class KWIN_EXPORT GLTexture
    : public QSharedData
    {
    public:
        GLTexture();
        explicit GLTexture( const QImage& image, GLenum target = GL_TEXTURE_2D );
        explicit GLTexture( const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D );
        GLTexture( const QString& fileName );
        GLTexture( int width, int height );
        virtual ~GLTexture();

        bool isNull() const;
        QSize size() const;

        virtual bool load( const QImage& image, GLenum target = GL_TEXTURE_2D );
        virtual bool load( const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D );
        virtual bool load( const QString& fileName );
        virtual void discard();
        virtual void bind();
        virtual void unbind();
        void render( QRegion region, const QRect& rect );
        /**
         * Set up texture transformation matrix to automatically map unnormalized
         * texture coordinates (i.e. 0 to width, 0 to height, (0,0) is top-left)
         * to OpenGL coordinates. Automatically adjusts for different texture
         * types. This can be done only for one texture at a time, repeated
         * calls for the same texture are allowed though, requiring the same
         * amount of calls to disableUnnormalizedTexCoords().
         */
        void enableUnnormalizedTexCoords();
        /**
         * Disables transformation set up using enableUnnormalizedTexCoords().
         */
        void disableUnnormalizedTexCoords();
        /**
         * Set up texture transformation matrix to automatically map normalized
         * texture coordinates (i.e. 0 to 1, 0 to 1, (0,0) is bottom-left)
         * to OpenGL coordinates. Automatically adjusts for different texture
         * types. This can be done only for one texture at a time, repeated
         * calls for the same texture are allowed though, requiring the same
         * amount of calls to disableNormalizedTexCoords().
         */
        void enableNormalizedTexCoords();
        /**
         * Disables transformation set up using enableNormalizedTexCoords().
         */
        void disableNormalizedTexCoords();

        GLuint texture() const;
        GLenum target() const;
        GLenum filter() const;
        virtual bool isDirty() const;
        void setTexture( GLuint texture );
        void setTarget( GLenum target );
        void setFilter( GLenum filter );
        void setWrapMode( GLenum mode );
        virtual void setDirty();

        static void initStatic();
        static bool NPOTTextureSupported()  { return mNPOTTextureSupported; }
        static bool framebufferObjectSupported()  { return mFramebufferObjectSupported; }
        static bool saturationSupported()  { return mSaturationSupported; }

    protected:
        void enableFilter();
        QImage convertToGLFormat( const QImage& img ) const;

        GLuint mTexture;
        GLenum mTarget;
        GLenum mFilter;
        QSize mSize;
        QSizeF mScale; // to un-normalize GL_TEXTURE_2D
        bool y_inverted; // texture has y inverted
        bool can_use_mipmaps;
        bool has_valid_mipmaps;

    private:
        void init();
        int mUnnormalizeActive; // 0 - no, otherwise refcount
        int mNormalizeActive; // 0 - no, otherwise refcount

        static bool mNPOTTextureSupported;
        static bool mFramebufferObjectSupported;
        static bool mSaturationSupported;
        Q_DISABLE_COPY( GLTexture )
    };

class KWIN_EXPORT GLShader
    {
    public:
        GLShader(const QString& vertexfile, const QString& fragmentfile);
        ~GLShader();

        bool isValid() const  { return mValid; }
        void bind();
        void unbind();

        int uniformLocation(const char* name);
        bool setUniform(const char* name, float value);
        bool setUniform(const char* name, int value);
        int attributeLocation(const char* name);
        bool setAttribute(const char* name, float value);

        void setTextureWidth( float width );
        void setTextureHeight( float height );
        float textureWidth();
        float textureHeight();

        static void initStatic();
        static bool fragmentShaderSupported()  { return mFragmentShaderSupported; }
        static bool vertexShaderSupported()  { return mVertexShaderSupported; }


    protected:
        bool loadFromFiles(const QString& vertexfile, const QString& fragmentfile);
        bool load(const QString& vertexsource, const QString& fragmentsource);


    private:
        unsigned int mProgram;
        bool mValid;
        static bool mFragmentShaderSupported;
        static bool mVertexShaderSupported;
        float mTextureWidth;
        float mTextureHeight;
    };

/**
 * @short Render target object
 *
 * Render target object enables you to render onto a texture. This texture can
 *  later be used to e.g. do post-processing of the scene.
 *
 * @author Rivo Laks <rivolaks@hot.ee>
 **/
class KWIN_EXPORT GLRenderTarget
{
    public:
        /**
         * Constructs a GLRenderTarget
         * @param color texture where the scene will be rendered onto
         **/
        GLRenderTarget(GLTexture* color);
        ~GLRenderTarget();

        /**
         * Enables this render target.
         * All OpenGL commands from now on affect this render target until the
         *  @ref disable method is called
         **/
        bool enable();
        /**
         * Disables this render target, activating whichever target was active
         *  when @ref enable was called.
         **/
        bool disable();

        bool valid() const  { return mValid; }

        static void initStatic();
        static bool supported()  { return mSupported; }


    protected:
        void initFBO();


    private:
        static bool mSupported;

        GLTexture* mTexture;
        bool mValid;

        GLuint mFramebuffer;
};

} // namespace

#endif

/** @} */

#endif
