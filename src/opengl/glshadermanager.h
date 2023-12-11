/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QByteArray>
#include <QFlags>
#include <QStack>
#include <map>
#include <memory>

namespace KWin
{

class GLShader;

enum class ShaderTrait {
    MapTexture = (1 << 0),
    UniformColor = (1 << 1),
    Modulate = (1 << 2),
    AdjustSaturation = (1 << 3),
    TransformColorspace = (1 << 4),
    MapExternalTexture = (1 << 5),
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
class KWIN_EXPORT ShaderManager
{
public:
    explicit ShaderManager();
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
    void bindFragDataLocations(GLShader *shader);
    void bindAttributeLocations(GLShader *shader) const;

    std::optional<QByteArray> preprocess(const QByteArray &src, int recursionDepth = 0) const;
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
class KWIN_EXPORT ShaderBinder
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

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ShaderTraits)
