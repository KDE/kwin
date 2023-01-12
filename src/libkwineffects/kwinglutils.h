/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

// kwin
#include "kwingltexture.h"
#include "kwinglutils_funcs.h"
#include <kwinglutils_export.h>

// Qt
#include <QSize>
#include <QStack>

/** @addtogroup kwineffects */
/** @{ */

class QVector2D;
class QVector3D;
class QVector4D;
class QMatrix4x4;

template<class K, class V>
class QHash;

namespace KWin
{

class GLVertexBuffer;
class GLVertexBufferPrivate;

// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
typedef void (*resolveFuncPtr)();
void KWINGLUTILS_EXPORT initGL(const std::function<resolveFuncPtr(const char *)> &resolveFunction);
// Cleans up all resources hold by the GL Context
void KWINGLUTILS_EXPORT cleanupGL();

bool KWINGLUTILS_EXPORT hasGLVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool KWINGLUTILS_EXPORT hasGLExtension(const QByteArray &extension);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool KWINGLUTILS_EXPORT checkGLError(const char *txt);

QList<QByteArray> KWINGLUTILS_EXPORT openGLExtensions();

class KWINGLUTILS_EXPORT GLShader
{
public:
    enum Flags {
        NoFlags = 0,
        ExplicitLinking = (1 << 0)
    };

    GLShader(const QString &vertexfile, const QString &fragmentfile, unsigned int flags = NoFlags);
    ~GLShader();

    bool isValid() const
    {
        return mValid;
    }

    void bindAttributeLocation(const char *name, int index);
    void bindFragDataLocation(const char *name, int index);

    bool link();

    int uniformLocation(const char *name);

    bool setUniform(const char *name, float value);
    bool setUniform(const char *name, int value);
    bool setUniform(const char *name, const QVector2D &value);
    bool setUniform(const char *name, const QVector3D &value);
    bool setUniform(const char *name, const QVector4D &value);
    bool setUniform(const char *name, const QMatrix4x4 &value);
    bool setUniform(const char *name, const QColor &color);

    bool setUniform(int location, float value);
    bool setUniform(int location, int value);
    bool setUniform(int location, const QVector2D &value);
    bool setUniform(int location, const QVector3D &value);
    bool setUniform(int location, const QVector4D &value);
    bool setUniform(int location, const QMatrix4x4 &value);
    bool setUniform(int location, const QColor &value);

    int attributeLocation(const char *name);
    bool setAttribute(const char *name, float value);
    /**
     * @return The value of the uniform as a matrix
     * @since 4.7
     */
    QMatrix4x4 getUniformMatrix4x4(const char *name);

    enum MatrixUniform {
        TextureMatrix = 0,
        ProjectionMatrix,
        ModelViewMatrix,
        ModelViewProjectionMatrix,
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
        AlphaToOne, ///< @deprecated no longer used
        TextureWidth,
        TextureHeight,
        IntUniformCount
    };

    enum ColorUniform {
        Color,
        ColorUniformCount
    };

    bool setUniform(MatrixUniform uniform, const QMatrix4x4 &matrix);
    bool setUniform(Vec2Uniform uniform, const QVector2D &value);
    bool setUniform(Vec4Uniform uniform, const QVector4D &value);
    bool setUniform(FloatUniform uniform, float value);
    bool setUniform(IntUniform uniform, int value);
    bool setUniform(ColorUniform uniform, const QVector4D &value);
    bool setUniform(ColorUniform uniform, const QColor &value);

protected:
    GLShader(unsigned int flags = NoFlags);
    bool loadFromFiles(const QString &vertexfile, const QString &fragmentfile);
    bool load(const QByteArray &vertexSource, const QByteArray &fragmentSource);
    const QByteArray prepareSource(GLenum shaderType, const QByteArray &sourceCode) const;
    bool compile(GLuint program, GLenum shaderType, const QByteArray &sourceCode) const;
    void bind();
    void unbind();
    void resolveLocations();

private:
    unsigned int mProgram;
    bool mValid : 1;
    bool mLocationsResolved : 1;
    bool mExplicitLinking : 1;
    int mMatrixLocation[MatrixCount];
    int mVec2Location[Vec2UniformCount];
    int mVec4Location[Vec4UniformCount];
    int mFloatLocation[FloatUniformCount];
    int mIntLocation[IntUniformCount];
    int mColorLocation[ColorUniformCount];

    friend class ShaderManager;
};

enum class ShaderTrait {
    MapTexture = (1 << 0),
    UniformColor = (1 << 1),
    Modulate = (1 << 2),
    AdjustSaturation = (1 << 3),
};

Q_DECLARE_FLAGS(ShaderTraits, ShaderTrait)

/**
 * @short Manager for Shaders.
 *
 * This class provides some built-in shaders to be used by both compositing scene and effects.
 * The ShaderManager provides methods to bind a built-in or a custom shader and keeps track of
 * the shaders which have been bound. When a shader is unbound the previously bound shader
 * will be rebound.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.7
 */
class KWINGLUTILS_EXPORT ShaderManager
{
public:
    ~ShaderManager();

    /**
     * Returns a shader with the given traits, creating it if necessary.
     */
    GLShader *shader(ShaderTraits traits);

    /**
     * @return The currently bound shader or @c null if no shader is bound.
     */
    GLShader *getBoundShader() const;

    /**
     * @return @c true if a shader is bound, @c false otherwise
     */
    bool isShaderBound() const;

    /**
     * Pushes the current shader onto the stack and binds a shader
     * with the given traits.
     */
    GLShader *pushShader(ShaderTraits traits);

    /**
     * Binds the @p shader.
     * To unbind the shader use popShader. A previous bound shader will be rebound.
     * To bind a built-in shader use the more specific method.
     * @param shader The shader to be bound
     * @see popShader
     */
    void pushShader(GLShader *shader);

    /**
     * Unbinds the currently bound shader and rebinds a previous stored shader.
     * If there is no previous shader, no shader will be rebound.
     * It is not safe to call this method if there is no bound shader.
     * @see pushShader
     * @see getBoundShader
     */
    void popShader();

    /**
     * Creates a GLShader with the specified sources.
     * The difference to GLShader is that it does not need to be loaded from files.
     * @param vertexSource The source code of the vertex shader
     * @param fragmentSource The source code of the fragment shader.
     * @return The created shader
     */
    std::unique_ptr<GLShader> loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource);

    /**
     * Creates a custom shader with the given @p traits and custom @p vertexSource and or @p fragmentSource.
     * If the @p vertexSource is empty a vertex shader with the given @p traits is generated.
     * If it is not empty the @p vertexSource is used as the source for the vertex shader.
     *
     * The same applies for argument @p fragmentSource just for the fragment shader.
     *
     * So if both @p vertesSource and @p fragmentSource are provided the @p traits are ignored.
     * If neither are provided a new shader following the @p traits is generated.
     *
     * @param traits The shader traits for generating the shader
     * @param vertexSource optional vertex shader source code to be used instead of shader traits
     * @param fragmentSource optional fragment shader source code to be used instead of shader traits
     * @return new generated shader
     * @since 5.6
     */
    std::unique_ptr<GLShader> generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource = QByteArray(), const QByteArray &fragmentSource = QByteArray());

    /**
     * Creates a custom shader with the given @p traits and custom @p vertexFile and or @p fragmentFile.
     *
     * If the @p vertexFile is empty a vertex shader with the given @p traits is generated.
     * If it is not empty the @p vertexFile is used as the source for the vertex shader.
     *
     * The same applies for argument @p fragmentFile just for the fragment shader.
     *
     * So if both @p vertexFile and @p fragmentFile are provided the @p traits are ignored.
     * If neither are provided a new shader following the @p traits is generated.
     *
     * If a custom shader stage is provided and core profile is used, the final file path will
     * be resolved by appending "_core" to the basename.
     *
     * @param traits The shader traits for generating the shader
     * @param vertexFile optional vertex shader source code to be used instead of shader traits
     * @param fragmentFile optional fragment shader source code to be used instead of shader traits
     * @return new generated shader
     * @see generateCustomShader
     */
    std::unique_ptr<GLShader> generateShaderFromFile(ShaderTraits traits, const QString &vertexFile = QString(), const QString &fragmentFile = QString());

    /**
     * @return a pointer to the ShaderManager instance
     */
    static ShaderManager *instance();

    /**
     * @internal
     */
    static void cleanup();

private:
    ShaderManager();

    void bindFragDataLocations(GLShader *shader);
    void bindAttributeLocations(GLShader *shader) const;

    QByteArray generateVertexSource(ShaderTraits traits) const;
    QByteArray generateFragmentSource(ShaderTraits traits) const;
    std::unique_ptr<GLShader> generateShader(ShaderTraits traits);

    QStack<GLShader *> m_boundShaders;
    std::map<ShaderTraits, std::unique_ptr<GLShader>> m_shaderHash;
    static std::unique_ptr<ShaderManager> s_shaderManager;
};

/**
 * An helper class to push a Shader on to ShaderManager's stack and ensuring that the Shader
 * gets popped again from the stack automatically once the object goes out of life.
 *
 * How to use:
 * @code
 * {
 * GLShader *myCustomShaderIWantToPush;
 * ShaderBinder binder(myCustomShaderIWantToPush);
 * // do stuff with the shader being pushed on the stack
 * }
 * // here the Shader is automatically popped as helper does no longer exist.
 * @endcode
 *
 * @since 4.10
 */
class KWINGLUTILS_EXPORT ShaderBinder
{
public:
    /**
     * @brief Pushes the given @p shader to the ShaderManager's stack.
     *
     * @param shader The Shader to push on the stack
     * @see ShaderManager::pushShader
     */
    explicit ShaderBinder(GLShader *shader);
    /**
     * @brief Pushes the Shader with the given @p traits to the ShaderManager's stack.
     *
     * @param traits The traits describing the shader
     * @see ShaderManager::pushShader
     * @since 5.6
     */
    explicit ShaderBinder(ShaderTraits traits);
    ~ShaderBinder();

    /**
     * @return The Shader pushed to the Stack.
     */
    GLShader *shader();

private:
    GLShader *m_shader;
};

inline ShaderBinder::ShaderBinder(GLShader *shader)
    : m_shader(shader)
{
    ShaderManager::instance()->pushShader(shader);
}

inline ShaderBinder::ShaderBinder(ShaderTraits traits)
    : m_shader(nullptr)
{
    m_shader = ShaderManager::instance()->pushShader(traits);
}

inline ShaderBinder::~ShaderBinder()
{
    ShaderManager::instance()->popShader();
}

inline GLShader *ShaderBinder::shader()
{
    return m_shader;
}

/**
 * @short OpenGL framebuffer object
 *
 * Framebuffer object enables you to render onto a texture. This texture can
 * later be used to e.g. do post-processing of the scene.
 *
 * @author Rivo Laks <rivolaks@hot.ee>
 */
class KWINGLUTILS_EXPORT GLFramebuffer
{
public:
    /**
     * Constructs a GLFramebuffer
     * @since 5.13
     */
    explicit GLFramebuffer();

    /**
     * Constructs a GLFramebuffer. Note that ensuring the color attachment outlives
     * the framebuffer is the responsibility of the caller.
     *
     * @param colorAttachment texture where the scene will be rendered onto
     */
    explicit GLFramebuffer(GLTexture *colorAttachment);

    /**
     * Constructs a wrapper for an already created framebuffer object. The GLFramebuffer
     * does not take the ownership of the framebuffer object handle.
     */
    GLFramebuffer(GLuint handle, const QSize &size);
    ~GLFramebuffer();

    /**
     * Returns the framebuffer object handle to this framebuffer object.
     */
    GLuint handle() const
    {
        return mFramebuffer;
    }
    /**
     * Returns the size of the color attachment to this framebuffer object.
     */
    QSize size() const
    {
        return mSize;
    }
    bool valid() const
    {
        return mValid;
    }

    static void initStatic();
    static bool supported()
    {
        return sSupported;
    }

    /**
     * Returns the last bound framebuffer, or @c null if no framebuffer is current.
     */
    static GLFramebuffer *currentFramebuffer();

    /**
     * Pushes the framebuffer stack of the input parameter in reverse order.
     * @param fbos The stack of GLFramebuffers
     * @since 5.13
     */
    static void pushFramebuffers(QStack<GLFramebuffer *> fbos);

    static void pushFramebuffer(GLFramebuffer *fbo);
    static GLFramebuffer *popFramebuffer();
    /**
     * Whether the GL_EXT_framebuffer_blit extension is supported.
     * This functionality is not available in OpenGL ES 2.0.
     *
     * @returns whether framebuffer blitting is supported.
     * @since 4.8
     */
    static bool blitSupported();

    /**
     * Blits from @a source rectangle in the current framebuffer to the @a destination rectangle in
     * this framebuffer.
     *
     * Be aware that framebuffer blitting may not be supported on all hardware. Use blitSupported()
     * to check whether it is supported.
     *
     * The @a source and the @a destination rectangles can have different sizes. The @a filter indicates
     * what filter will be used in case scaling needs to be performed.
     *
     * @see blitSupported
     * @since 4.8
     */
    void blitFromFramebuffer(const QRect &source = QRect(), const QRect &destination = QRect(), GLenum filter = GL_LINEAR);

protected:
    void initFBO(GLTexture *colorAttachment);

private:
    bool bind();

    friend void KWin::cleanupGL();
    static void cleanup();
    static bool sSupported;
    static bool s_blitSupported;
    static QStack<GLFramebuffer *> s_fbos;

    GLuint mFramebuffer = 0;
    QSize mSize;
    bool mValid = false;
    bool mForeign = false;
};

enum VertexAttributeType {
    VA_Position = 0,
    VA_TexCoord = 1,
    VertexAttributeCount = 2,
};

/**
 * Describes the format of a vertex attribute stored in a buffer object.
 *
 * The attribute format consists of the attribute index, the number of
 * vector components, the data type, and the offset of the first element
 * relative to the start of the vertex data.
 */
struct GLVertexAttrib
{
    int index; /** The attribute index */
    int size; /** The number of components [1..4] */
    GLenum type; /** The type (e.g. GL_FLOAT) */
    int relativeOffset; /** The relative offset of the attribute */
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
 * @author Martin Gräßlin <mgraesslin@kde.org>
 * @since 4.6
 */
class KWINGLUTILS_EXPORT GLVertexBuffer
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

    explicit GLVertexBuffer(UsageHint hint);
    ~GLVertexBuffer();

    /**
     * Specifies how interleaved vertex attributes are laid out in
     * the buffer object.
     *
     * Note that the attributes and the stride should be 32 bit aligned
     * or a performance penalty may be incurred.
     *
     * For some hardware the optimal stride is a multiple of 32 bytes.
     *
     * Example:
     *
     *     struct Vertex {
     *         QVector3D position;
     *         QVector2D texcoord;
     *     };
     *
     *     const GLVertexAttrib attribs[] = {
     *         { VA_Position, 3, GL_FLOAT, offsetof(Vertex, position) },
     *         { VA_TexCoord, 2, GL_FLOAT, offsetof(Vertex, texcoord) }
     *     };
     *
     *     Vertex vertices[6];
     *     vbo->setAttribLayout(attribs, 2, sizeof(Vertex));
     *     vbo->setData(vertices, sizeof(vertices));
     */
    void setAttribLayout(const GLVertexAttrib *attribs, int count, int stride);

    /**
     * Uploads data into the buffer object's data store.
     */
    void setData(const void *data, size_t sizeInBytes);

    /**
     * Sets the number of vertices that will be drawn by the render() method.
     */
    void setVertexCount(int count);

    /**
     * Sets the vertex data.
     * @param numberVertices The number of vertices in the arrays
     * @param dim The dimension of the vertices: 2 for x/y, 3 for x/y/z
     * @param vertices The vertices, size must equal @a numberVertices * @a dim
     * @param texcoords The texture coordinates for each vertex.
     * Size must equal 2 * @a numberVertices.
     */
    void setData(int numberVertices, int dim, const float *vertices, const float *texcoords);

    /**
     * Maps an unused range of the data store into the client's address space.
     *
     * The data store will be reallocated if it is smaller than the given size.
     *
     * The buffer object is mapped for writing, not reading. Attempts to read from
     * the mapped buffer range may result in system errors, including program
     * termination. The data in the mapped region is undefined until it has been
     * written to. If subsequent GL calls access unwritten memory, the results are
     * undefined and system errors, including program termination, may occur.
     *
     * No GL calls that access the buffer object must be made while the buffer
     * object is mapped. The returned pointer must not be passed as a parameter
     * value to any GL function.
     *
     * It is assumed that the GL_ARRAY_BUFFER_BINDING will not be changed while
     * the buffer object is mapped.
     */
    GLvoid *map(size_t size);

    /**
     * Flushes the mapped buffer range and unmaps the buffer.
     */
    void unmap();

    /**
     * Binds the vertex arrays to the context.
     */
    void bindArrays();

    /**
     * Disables the vertex arrays.
     */
    void unbindArrays();

    /**
     * Draws count vertices beginning with first.
     */
    void draw(GLenum primitiveMode, int first, int count);

    /**
     * Draws count vertices beginning with first.
     */
    void draw(const QRegion &region, GLenum primitiveMode, int first, int count, bool hardwareClipping = false);

    /**
     * Renders the vertex data in given @a primitiveMode.
     * Please refer to OpenGL documentation of glDrawArrays or glDrawElements for allowed
     * values for @a primitiveMode. Best is to use GL_TRIANGLES or similar to be future
     * compatible.
     */
    void render(GLenum primitiveMode);
    /**
     * Same as above restricting painting to @a region if @a hardwareClipping is true.
     * It's within the caller's responsibility to enable GL_SCISSOR_TEST.
     */
    void render(const QRegion &region, GLenum primitiveMode, bool hardwareClipping = false);
    /**
     * Sets the color the geometry will be rendered with.
     * For legacy rendering glColor is used before rendering the geometry.
     * For core shader a uniform "geometryColor" is expected and is set.
     * @param color The color to render the geometry
     * @param enableColor Whether the geometry should be rendered with a color or not
     * @see setUseColor
     * @see isUseColor
     * @since 4.7
     */
    void setColor(const QColor &color, bool enableColor = true);
    /**
     * @return @c true if geometry will be painted with a color, @c false otherwise
     * @see setUseColor
     * @see setColor
     * @since 4.7
     */
    bool isUseColor() const;
    /**
     * Enables/Disables rendering the geometry with a color.
     * If no color is set an opaque, black color is used.
     * @param enable Enable/Disable rendering with color
     * @see isUseColor
     * @see setColor
     * @since 4.7
     */
    void setUseColor(bool enable);

    /**
     * Resets the instance to default values.
     * Useful for shared buffers.
     * @since 4.7
     */
    void reset();

    /**
     * Notifies the vertex buffer that we are done painting the frame.
     *
     * @internal
     */
    void endOfFrame();

    /**
     * Notifies the vertex buffer that we are about to paint a frame.
     *
     * @internal
     */
    void beginFrame();

    /**
     * @internal
     */
    static void initStatic();

    /**
     * @internal
     */
    static void cleanup();

    /**
     * Returns true if indexed quad mode is supported, and false otherwise.
     */
    static bool supportsIndexedQuads();

    /**
     * @return A shared VBO for streaming data
     * @since 4.7
     */
    static GLVertexBuffer *streamingBuffer();

    static const GLVertexAttrib GLVertex2DLayout[2];
    static const GLVertexAttrib GLVertex3DLayout[2];

private:
    const std::unique_ptr<GLVertexBufferPrivate> d;
};

} // namespace

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ShaderTraits)

/** @} */
