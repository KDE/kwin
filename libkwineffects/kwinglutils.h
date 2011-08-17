/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include <kwinglutils_funcs.h>

#include <QtGui/QPixmap>

#include <QtGui/QImage>
#include <QtCore/QSize>
#include <QtCore/QSharedData>
#include <QtCore/QStack>

#include "kwingltexture.h"

/** @addtogroup kwineffects */
/** @{ */

class QVector2D;
class QVector3D;
class QVector4D;
class QMatrix4x4;

template< class K, class V > class QHash;


namespace KWin
{


class GLTexture;
class GLVertexBuffer;
class GLVertexBufferPrivate;


// Initializes GLX function pointers
void KWIN_EXPORT initGLX();
// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
void KWIN_EXPORT initGL();
// Initializes EGL function pointers
void KWIN_EXPORT initEGL();
// Cleans up all resources hold by the GL Context
void KWIN_EXPORT cleanupGL();

// Number of supported texture units
extern KWIN_EXPORT int glTextureUnitsCount;


bool KWIN_EXPORT hasGLVersion(int major, int minor, int release = 0);
bool KWIN_EXPORT hasGLXVersion(int major, int minor, int release = 0);
bool KWIN_EXPORT hasEGLVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool KWIN_EXPORT hasGLExtension(const QString& extension);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool KWIN_EXPORT checkGLError(const char* txt);

inline bool KWIN_EXPORT isPowerOfTwo(int x)
{
    return ((x & (x - 1)) == 0);
}
/**
 * @return power of two integer _greater or equal to_ x.
 *  E.g. nearestPowerOfTwo(513) = nearestPowerOfTwo(800) = 1024
 **/
int KWIN_EXPORT nearestPowerOfTwo(int x);

/**
 * Push a new matrix on the GL matrix stack.
 * In GLES this method is a noop. This method should be preferred over glPushMatrix
 * as it also handles GLES.
 * @since 4.7
 **/
KWIN_EXPORT void pushMatrix();
/**
 * Multiplies current matrix on GL stack with @p matrix and pushes the result on the matrix stack.
 * This method is the same as pushMatrix followed by multiplyMatrix.
 * In GLES this method is a noop. This method should be preferred over glPushMatrix
 * as it also handles GLES.
 * @param matrix The matrix the current matrix on the stack should be multiplied with.
 * @see pushMatrix
 * @see multiplyMatrix
 * @since 4.7
 **/
KWIN_EXPORT void pushMatrix(const QMatrix4x4 &matrix);
/**
 * Multiplies the current matrix on GL stack with @p matrix.
 * In GLES this method is a noop. This method should be preferred over glMultMatrix
 * as it also handles GLES.
 * @param matrix The matrix the current matrix on the stack should be multiplied with.
 * @since 4.7
 **/
KWIN_EXPORT void multiplyMatrix(const QMatrix4x4 &matrix);
/**
 * Replaces the current matrix on GL stack with @p matrix.
 * In GLES this method is a no-op. This method should be preferred over glLoadMatrix
 * as it also handles GLES.
 * @param matrix The new matrix to replace the existing one on the GL stack.
 * @since 4.7
 **/
KWIN_EXPORT void loadMatrix(const QMatrix4x4 &matrix);
/**
 * Pops the current matrix from the GL matrix stack.
 * In GLES this method is a noop. This method should be preferred over glPopMatrix
 * as it also handles GLES.
 * @since 4.7
 **/
KWIN_EXPORT void popMatrix();

class KWIN_EXPORT GLShader
{
public:
    GLShader(const QString& vertexfile, const QString& fragmentfile);
    ~GLShader();

    bool isValid() const  {
        return mValid;
    }

    int uniformLocation(const char* name);

    bool setUniform(const char* name, float value);
    bool setUniform(const char* name, int value);
    bool setUniform(const char* name, const QVector2D& value);
    bool setUniform(const char* name, const QVector3D& value);
    bool setUniform(const char* name, const QVector4D& value);
    bool setUniform(const char* name, const QMatrix4x4& value);
    bool setUniform(const char* name, const QColor& color);

    bool setUniform(int location, float value);
    bool setUniform(int location, int value);
    bool setUniform(int location, const QVector2D &value);
    bool setUniform(int location, const QVector3D &value);
    bool setUniform(int location, const QVector4D &value);
    bool setUniform(int location, const QMatrix4x4 &value);
    bool setUniform(int location, const QColor &value);

    int attributeLocation(const char* name);
    bool setAttribute(const char* name, float value);
    /**
     * @return The value of the uniform as a matrix
     * @since 4.7
     **/
    QMatrix4x4 getUniformMatrix4x4(const char* name);

    enum MatrixUniform {
        TextureMatrix = 0,
        ProjectionMatrix,
        ModelViewMatrix,
        WindowTransformation,
        ScreenTransformation,
        MatrixCount
    };

    enum Vec2Uniform {
        Offset,
        Vec2UniformCount
    };

    enum Vec4Uniform {
        ModulationConstant,
        Vec4UniformCount
    };

    enum FloatUniform {
        Saturation,
        FloatUniformCount
    };

    enum IntUniform {
        AlphaToOne,
        IntUniformCount
    };

    bool setUniform(MatrixUniform uniform, const QMatrix4x4 &matrix);
    bool setUniform(Vec2Uniform uniform,   const QVector2D &value);
    bool setUniform(Vec4Uniform uniform,   const QVector4D &value);
    bool setUniform(FloatUniform uniform,  float value);
    bool setUniform(IntUniform uniform,    int value);

protected:
    GLShader();
    bool loadFromFiles(const QString& vertexfile, const QString& fragmentfile);
    bool load(const QByteArray &vertexSource, const QByteArray &fragmentSource);
    bool compile(GLuint program, GLenum shaderType, const QByteArray &sourceCode) const;
    void bind();
    void unbind();
    void resolveLocations();

private:
    unsigned int mProgram;
    bool mValid:1;
    bool mLocationsResolved:1;
    int mMatrixLocation[MatrixCount];
    int mVec2Location[Vec2UniformCount];
    int mVec4Location[Vec4UniformCount];
    int mFloatLocation[FloatUniformCount];
    int mIntLocation[IntUniformCount];

    friend class ShaderManager;
};

/**
 * @short Manager for Shaders.
 *
 * This class provides some built-in shaders to be used by both compositing scene and effects.
 * The ShaderManager provides methods to bind a built-in or a custom shader and keeps track of
 * the shaders which have been bound. When a shader is unbound the previously bound shader
 * will be rebound.
 *
 * @author Martin Gräßlin <kde@martin-graesslin.com>
 * @since 4.7
 **/
class KWIN_EXPORT ShaderManager
{
public:
    /**
     * Identifiers for built-in shaders available for effects and scene
     **/
    enum ShaderType {
        /**
         * An orthographic projection shader able to render textured geometries.
         * Expects a @c vec2 uniform @c offset describing the offset from top-left corner
         * and defaults to @c (0/0). Expects a @c vec2 uniform @c textureSize to calculate
         * normalized texture coordinates. Defaults to @c (1.0/1.0). And expects a @c vec3
         * uniform @c colorManiuplation, with @c x being opacity, @c y being brightness and
         * @c z being saturation. All three values default to @c 1.0.
         * The fragment shader takes a uniform @c u_forceAlpha to force the alpha component
         * to @c 1.0 if it is set to a value not @c 0. This uniform is useful when rendering
         * a RGB-only window and the alpha channel is required in a later step.
         * The sampler uniform is @c sample and defaults to @c 0.
         * The shader uses two vertex attributes @c vertex and @c texCoord.
         **/
        SimpleShader,
        /**
         * A generic shader able to render transformed, textured geometries.
         * This shader is mostly needed by the scene and not of much interest for effects.
         * Effects can influence this shader through @link ScreenPaintData and @link WindowPaintData.
         * The shader expects four @c mat4 uniforms @c projection, @c modelview,
         * @c screenTransformation and @c windowTransformation. The fragment shader expect the
         * same uniforms as the SimpleShader and the same vertex attributes are used.
         **/
        GenericShader,
        /**
         * An orthographic shader to render simple colored geometries without texturing.
         * Expects a @c vec2 uniform @c offset describing the offset from top-left corner
         * and defaults to @c (0/0). The fragment shader expects a single @c vec4 uniform
         * @c geometryColor, which defaults to fully opaque black.
         * The Shader uses one vertex attribute @c vertex.
         **/
        ColorShader
    };

    /**
     * @return The currently bound shader or @c null if no shader is bound.
     **/
    GLShader *getBoundShader() const;

    /**
     * @return @c true if a shader is bound, @c false otherwise
     **/
    bool isShaderBound() const;
    /**
     * @return @c true if the built-in shaders are valid, @c false otherwise
     **/
    bool isValid() const;
    /**
     * Is @c true if the environment variable KWIN_GL_DEBUG is set to 1.
     * In that case shaders are compiled with KWIN_SHADER_DEBUG defined.
     * @returns @c true if shaders are compiled with debug information
     * @since 4.8
     **/
    bool isShaderDebug() const;

    /**
     * Binds the shader of specified @p type.
     * To unbind the shader use @link popShader. A previous bound shader will be rebound.
     * @param type The built-in shader to bind
     * @param reset Whether all uniforms should be reset to their default values
     * @return The bound shader or @c NULL if shaders are not valid
     * @see popShader
     **/
    GLShader *pushShader(ShaderType type, bool reset = false);
    /**
     * Binds the @p shader.
     * To unbind the shader use @link popShader. A previous bound shader will be rebound.
     * To bind a built-in shader use the more specific method.
     * @param shader The shader to be bound
     * @see popShader
     **/
    void pushShader(GLShader *shader);

    /**
     * Unbinds the currently bound shader and rebinds a previous stored shader.
     * If there is no previous shader, no shader will be rebound.
     * It is not safe to call this method if there is no bound shader.
     * @see pushShader
     * @see getBoundShader
     **/
    void popShader();

    /**
     * Creates a GLShader with a built-in vertex shader and a custom fragment shader.
     * @param vertex The generic vertex shader
     * @param fragmentFile The path to the source code of the fragment shader
     * @return The created shader
     **/
    GLShader *loadFragmentShader(ShaderType vertex, const QString &fragmentFile);
    /**
     * Creates a GLShader with a built-in fragment shader and a custom vertex shader.
     * @param fragment The generic fragment shader
     * @param vertexFile The path to the source code of the vertex shader
     * @return The created shader
     **/
    GLShader *loadVertexShader(ShaderType fragment, const QString &vertexFile);
    /**
     * Creates a GLShader with the specified sources.
     * The difference to GLShader is that it does not need to be loaded from files.
     * @param vertexSource The source code of the vertex shader
     * @param fragmentSource The source code of the fragment shader.
     * @return The created shader
     **/
    GLShader *loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource);

    /**
     * @return a pointer to the ShaderManager instance
     **/
    static ShaderManager *instance();

    /**
     * @internal
     **/
    static void cleanup();

private:
    ShaderManager();
    ~ShaderManager();

    void initShaders();
    void resetShader(ShaderType type);

    QStack<GLShader*> m_boundShaders;
    GLShader *m_orthoShader;
    GLShader *m_genericShader;
    GLShader *m_colorShader;
    bool m_inited;
    bool m_valid;
    bool m_debug;
    static ShaderManager *s_shaderManager;
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

    bool valid() const  {
        return mValid;
    }

    static void initStatic();
    static bool supported()  {
        return sSupported;
    }
    static void pushRenderTarget(GLRenderTarget *target);
    static GLRenderTarget *popRenderTarget();
    static bool isRenderTargetBound();
    /**
     * Whether the GL_EXT_framebuffer_blit extension is supported.
     * This functionality is not available in OpenGL ES 2.0.
     *
     * @returns whether framebuffer blitting is supported.
     * @since 4.8
     **/
    static bool blitSupported();

    /**
     * Blits the content of the current draw framebuffer into the texture attached to this FBO.
     *
     * Be aware that framebuffer blitting may not be supported on all hardware. Use @link blitSupported to check whether
     * it is supported.
     * @param source Geometry in screen coordinates which should be blitted, if not specified complete framebuffer is used
     * @param destination Geometry in attached texture, if not specified complete texture is used as destination
     * @param filter The filter to use if blitted content needs to be scaled.
     * @see blitSupported
     * @since 4.8
     **/
    void blitFromFramebuffer(const QRect &source = QRect(), const QRect &destination = QRect(), GLenum filter = GL_LINEAR);


protected:
    void initFBO();


private:
    static bool sSupported;
    static bool s_blitSupported;
    static QStack<GLRenderTarget*> s_renderTargets;

    GLTexture* mTexture;
    bool mValid;

    GLuint mFramebuffer;
};

/**
 * @short Vertex Buffer Object
 *
 * This is a short helper class to use vertex buffer objects (VBO). A VBO can be used to buffer
 * vertex data and to store them on graphics memory. It is the only allowed way to pass vertex
 * data to the GPU in OpenGL ES 2 and OpenGL 3 with forward compatible mode.
 *
 * If VBOs are not supported on the used OpenGL profile this class falls back to legacy
 * rendering using client arrays. Therefore this class should always be used for rendering geometries.
 *
 * @author Martin Gräßlin <kde@martin-graesslin.com>
 * @since 4.6
 */
class KWIN_EXPORT GLVertexBuffer
{
public:
    /**
     * Enum to define how often the vertex data in the buffer object changes.
     */
    enum UsageHint {
        Dynamic, ///< frequent changes, but used several times for rendering
        Static, ///< No changes to data
        Stream ///< Data only used once for rendering, updated very frequently
    };

    GLVertexBuffer(UsageHint hint);
    ~GLVertexBuffer();

    /**
     * Sets the vertex data.
     * @param numberVertices The number of vertices in the arrays
     * @param dim The dimension of the vertices: 2 for x/y, 3 for x/y/z
     * @param vertices The vertices, size must equal @a numberVertices * @a dim
     * @param texcoords The texture coordinates for each vertex.
     * Size must equal 2 * @a numberVertices.
     */
    void setData(int numberVertices, int dim, const float* vertices, const float* texcoords);
    /**
     * Renders the vertex data in given @a primitiveMode.
     * Please refer to OpenGL documentation of glDrawArrays or glDrawElements for allowed
     * values for @a primitiveMode. Best is to use GL_TRIANGLES or similar to be future
     * compatible.
     */
    void render(GLenum primitiveMode);
    /**
     * Same as above restricting painting to @a region.
     */
    void render(const QRegion& region, GLenum primitiveMode);
    /**
     * Sets the color the geometry will be rendered with.
     * For legacy rendering glColor is used before rendering the geometry.
     * For core shader a uniform "geometryColor" is expected and is set.
     * @param color The color to render the geometry
     * @param enableColor Whether the geometry should be rendered with a color or not
     * @see setUseColor
     * @see isUseColor
     * @since 4.7
     **/
    void setColor(const QColor& color, bool enableColor = true);
    /**
     * @return @c true if geometry will be painted with a color, @c false otherwise
     * @see setUseColor
     * @see setColor
     * @since 4.7
     **/
    bool isUseColor() const;
    /**
     * Enables/Disables rendering the geometry with a color.
     * If no color is set an opaque, black color is used.
     * @param enable Enable/Disable rendering with color
     * @see isUseColor
     * @see setColor
     * @since 4.7
     **/
    void setUseColor(bool enable);

    /**
     * Resets the instance to default values.
     * Useful for shared buffers.
     * @since 4.7
     **/
    void reset();

    /**
     * @internal
     */
    static void initStatic();
    /**
     * Returns true if VBOs are supported, it is save to use this class even if VBOs are not
     * supported.
     * @returns true if vertex buffer objects are supported
     */
    static bool isSupported();

    /**
     * @return A shared VBO for streaming data
     * @since 4.7
     **/
    static GLVertexBuffer *streamingBuffer();

private:
    GLVertexBufferPrivate* const d;
};

} // namespace

/** @} */

#endif
