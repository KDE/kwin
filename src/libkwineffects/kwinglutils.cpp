/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinglutils.h"

// need to call GLTexturePrivate::initStatic()
#include "kwingltexture_p.h"

#include "kwineffects.h"
#include "kwinglplatform.h"
#include "logging_p.h"

#include <QFile>
#include <QHash>
#include <QImage>
#include <QMatrix4x4>
#include <QPixmap>
#include <QVarLengthArray>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cmath>
#include <deque>

#define DEBUG_GLFRAMEBUFFER 0

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

namespace KWin
{
// Variables
// List of all supported GL extensions
static QList<QByteArray> glExtensions;

// Functions

static void initDebugOutput()
{
    static bool enabled = qEnvironmentVariableIntValue("KWIN_GL_DEBUG");
    if (!enabled) {
        return;
    }

    const bool have_KHR_debug = hasGLExtension(QByteArrayLiteral("GL_KHR_debug"));
    const bool have_ARB_debug = hasGLExtension(QByteArrayLiteral("GL_ARB_debug_output"));
    if (!have_KHR_debug && !have_ARB_debug) {
        return;
    }

    if (!have_ARB_debug) {
        // if we don't have ARB debug, but only KHR debug we need to verify whether the context is a debug context
        // it should work without as well, but empirical tests show: no it doesn't
        if (GLPlatform::instance()->isGLES()) {
            if (!hasGLVersion(3, 2)) {
                // empirical data shows extension doesn't work
                return;
            }
        } else if (!hasGLVersion(3, 0)) {
            return;
        }
        // can only be queried with either OpenGL >= 3.0 or OpenGL ES of at least 3.1
        GLint value = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &value);
        if (!(value & GL_CONTEXT_FLAG_DEBUG_BIT)) {
            return;
        }
    }

    // Set the callback function
    auto callback = [](GLenum source, GLenum type, GLuint id,
                       GLenum severity, GLsizei length,
                       const GLchar *message,
                       const GLvoid *userParam) {
        while (length && std::isspace(message[length - 1])) {
            --length;
        }

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            qCWarning(LIBKWINGLUTILS, "%#x: %.*s", id, length, message);
            break;

        case GL_DEBUG_TYPE_OTHER:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        default:
            qCDebug(LIBKWINGLUTILS, "%#x: %.*s", id, length, message);
            break;
        }
    };

    glDebugMessageCallback(callback, nullptr);

    // This state exists only in GL_KHR_debug
    if (have_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
    }

#if !defined(QT_NO_DEBUG)
    // Enable all debug messages
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#else
    // Enable error messages
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

    // Insert a test message
    const QByteArray message = QByteArrayLiteral("OpenGL debug output initialized");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0,
                         GL_DEBUG_SEVERITY_LOW, message.length(), message.constData());
}

void initGL(const std::function<resolveFuncPtr(const char *)> &resolveFunction)
{
    // Get list of supported OpenGL extensions
    if (hasGLVersion(3, 0)) {
        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const QByteArray name = (const char *)glGetStringi(GL_EXTENSIONS, i);
            glExtensions << name;
        }
    } else {
        glExtensions = QByteArray((const char *)glGetString(GL_EXTENSIONS)).split(' ');
    }

    // handle OpenGL extensions functions
    glResolveFunctions(resolveFunction);

    initDebugOutput();

    GLTexturePrivate::initStatic();
    GLFramebuffer::initStatic();
    GLVertexBuffer::initStatic();
}

void cleanupGL()
{
    ShaderManager::cleanup();
    GLTexturePrivate::cleanup();
    GLFramebuffer::cleanup();
    GLVertexBuffer::cleanup();
    GLPlatform::cleanup();

    glExtensions.clear();
}

bool hasGLVersion(int major, int minor, int release)
{
    return GLPlatform::instance()->glVersion() >= kVersionNumber(major, minor, release);
}

bool hasGLExtension(const QByteArray &extension)
{
    return glExtensions.contains(extension);
}

QList<QByteArray> openGLExtensions()
{
    return glExtensions;
}

static QString formatGLError(GLenum err)
{
    switch (err) {
    case GL_NO_ERROR:
        return QStringLiteral("GL_NO_ERROR");
    case GL_INVALID_ENUM:
        return QStringLiteral("GL_INVALID_ENUM");
    case GL_INVALID_VALUE:
        return QStringLiteral("GL_INVALID_VALUE");
    case GL_INVALID_OPERATION:
        return QStringLiteral("GL_INVALID_OPERATION");
    case GL_STACK_OVERFLOW:
        return QStringLiteral("GL_STACK_OVERFLOW");
    case GL_STACK_UNDERFLOW:
        return QStringLiteral("GL_STACK_UNDERFLOW");
    case GL_OUT_OF_MEMORY:
        return QStringLiteral("GL_OUT_OF_MEMORY");
    default:
        return QLatin1String("0x") + QString::number(err, 16);
    }
}

bool checkGLError(const char *txt)
{
    GLenum err = glGetError();
    if (err == GL_CONTEXT_LOST) {
        qCWarning(LIBKWINGLUTILS) << "GL error: context lost";
        return true;
    }
    bool hasError = false;
    while (err != GL_NO_ERROR) {
        qCWarning(LIBKWINGLUTILS) << "GL error (" << txt << "): " << formatGLError(err);
        hasError = true;
        err = glGetError();
        if (err == GL_CONTEXT_LOST) {
            qCWarning(LIBKWINGLUTILS) << "GL error: context lost";
            break;
        }
    }
    return hasError;
}

//****************************************
// GLShader
//****************************************

GLShader::GLShader(unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
}

GLShader::GLShader(const QString &vertexfile, const QString &fragmentfile, unsigned int flags)
    : mValid(false)
    , mLocationsResolved(false)
    , mExplicitLinking(flags & ExplicitLinking)
{
    mProgram = glCreateProgram();
    loadFromFiles(vertexfile, fragmentfile);
}

GLShader::~GLShader()
{
    if (mProgram) {
        glDeleteProgram(mProgram);
    }
}

bool GLShader::loadFromFiles(const QString &vertexFile, const QString &fragmentFile)
{
    QFile vf(vertexFile);
    if (!vf.open(QIODevice::ReadOnly)) {
        qCCritical(LIBKWINGLUTILS) << "Couldn't open" << vertexFile << "for reading!";
        return false;
    }
    const QByteArray vertexSource = vf.readAll();

    QFile ff(fragmentFile);
    if (!ff.open(QIODevice::ReadOnly)) {
        qCCritical(LIBKWINGLUTILS) << "Couldn't open" << fragmentFile << "for reading!";
        return false;
    }
    const QByteArray fragmentSource = ff.readAll();

    return load(vertexSource, fragmentSource);
}

bool GLShader::link()
{
    // Be optimistic
    mValid = true;

    glLinkProgram(mProgram);

    // Get the program info log
    int maxLength, length;
    glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetProgramInfoLog(mProgram, maxLength, &length, log.data());

    // Make sure the program linked successfully
    int status;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);

    if (status == 0) {
        qCCritical(LIBKWINGLUTILS) << "Failed to link shader:"
                                   << "\n"
                                   << log;
        mValid = false;
    } else if (length > 0) {
        qCDebug(LIBKWINGLUTILS) << "Shader link log:" << log;
    }

    return mValid;
}

const QByteArray GLShader::prepareSource(GLenum shaderType, const QByteArray &source) const
{
    // Prepare the source code
    QByteArray ba;
    if (GLPlatform::instance()->isGLES() && GLPlatform::instance()->glslVersion() < kVersionNumber(3, 0)) {
        ba.append("precision highp float;\n");
    }
    ba.append(source);
    if (GLPlatform::instance()->isGLES() && GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0)) {
        ba.replace("#version 140", "#version 300 es\n\nprecision highp float;\n");
    }

    return ba;
}

bool GLShader::compile(GLuint program, GLenum shaderType, const QByteArray &source) const
{
    GLuint shader = glCreateShader(shaderType);

    QByteArray preparedSource = prepareSource(shaderType, source);
    const char *src = preparedSource.constData();
    glShaderSource(shader, 1, &src, nullptr);

    // Compile the shader
    glCompileShader(shader);

    // Get the shader info log
    int maxLength, length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    QByteArray log(maxLength, 0);
    glGetShaderInfoLog(shader, maxLength, &length, log.data());

    // Check the status
    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == 0) {
        const char *typeName = (shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment");
        qCCritical(LIBKWINGLUTILS) << "Failed to compile" << typeName << "shader:"
                                   << "\n"
                                   << log;
    } else if (length > 0) {
        qCDebug(LIBKWINGLUTILS) << "Shader compile log:" << log;
    }

    if (status != 0) {
        glAttachShader(program, shader);
    }

    glDeleteShader(shader);
    return status != 0;
}

bool GLShader::load(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    // Make sure shaders are actually supported
    if (!(GLPlatform::instance()->supports(GLSL) &&
          // we lack shader branching for Texture2DRectangle everywhere - and it's probably not worth it
          GLPlatform::instance()->supports(TextureNPOT))) {
        qCCritical(LIBKWINGLUTILS) << "Shaders are not supported";
        return false;
    }

    mValid = false;

    // Compile the vertex shader
    if (!vertexSource.isEmpty()) {
        bool success = compile(mProgram, GL_VERTEX_SHADER, vertexSource);

        if (!success) {
            return false;
        }
    }

    // Compile the fragment shader
    if (!fragmentSource.isEmpty()) {
        bool success = compile(mProgram, GL_FRAGMENT_SHADER, fragmentSource);

        if (!success) {
            return false;
        }
    }

    if (mExplicitLinking) {
        return true;
    }

    // link() sets mValid
    return link();
}

void GLShader::bindAttributeLocation(const char *name, int index)
{
    glBindAttribLocation(mProgram, index, name);
}

void GLShader::bindFragDataLocation(const char *name, int index)
{
    if (!GLPlatform::instance()->isGLES() && (hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_gpu_shader4")))) {
        glBindFragDataLocation(mProgram, index, name);
    }
}

void GLShader::bind()
{
    glUseProgram(mProgram);
}

void GLShader::unbind()
{
    glUseProgram(0);
}

void GLShader::resolveLocations()
{
    if (mLocationsResolved) {
        return;
    }

    mMatrixLocation[TextureMatrix] = uniformLocation("textureMatrix");
    mMatrixLocation[ProjectionMatrix] = uniformLocation("projection");
    mMatrixLocation[ModelViewMatrix] = uniformLocation("modelview");
    mMatrixLocation[ModelViewProjectionMatrix] = uniformLocation("modelViewProjectionMatrix");
    mMatrixLocation[WindowTransformation] = uniformLocation("windowTransformation");
    mMatrixLocation[ScreenTransformation] = uniformLocation("screenTransformation");

    mVec2Location[Offset] = uniformLocation("offset");

    mVec4Location[ModulationConstant] = uniformLocation("modulation");

    mFloatLocation[Saturation] = uniformLocation("saturation");

    mColorLocation[Color] = uniformLocation("geometryColor");

    mIntLocation[TextureWidth] = uniformLocation("textureWidth");
    mIntLocation[TextureHeight] = uniformLocation("textureHeight");

    mLocationsResolved = true;
}

int GLShader::uniformLocation(const char *name)
{
    const int location = glGetUniformLocation(mProgram, name);
    return location;
}

bool GLShader::setUniform(GLShader::MatrixUniform uniform, const QMatrix4x4 &matrix)
{
    resolveLocations();
    return setUniform(mMatrixLocation[uniform], matrix);
}

bool GLShader::setUniform(GLShader::Vec2Uniform uniform, const QVector2D &value)
{
    resolveLocations();
    return setUniform(mVec2Location[uniform], value);
}

bool GLShader::setUniform(GLShader::Vec4Uniform uniform, const QVector4D &value)
{
    resolveLocations();
    return setUniform(mVec4Location[uniform], value);
}

bool GLShader::setUniform(GLShader::FloatUniform uniform, float value)
{
    resolveLocations();
    return setUniform(mFloatLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::IntUniform uniform, int value)
{
    resolveLocations();
    return setUniform(mIntLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QVector4D &value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
}

bool GLShader::setUniform(GLShader::ColorUniform uniform, const QColor &value)
{
    resolveLocations();
    return setUniform(mColorLocation[uniform], value);
}

bool GLShader::setUniform(const char *name, float value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, int value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector2D &value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector3D &value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QVector4D &value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QMatrix4x4 &value)
{
    const int location = uniformLocation(name);
    return setUniform(location, value);
}

bool GLShader::setUniform(const char *name, const QColor &color)
{
    const int location = uniformLocation(name);
    return setUniform(location, color);
}

bool GLShader::setUniform(int location, float value)
{
    if (location >= 0) {
        glUniform1f(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, int value)
{
    if (location >= 0) {
        glUniform1i(location, value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector2D &value)
{
    if (location >= 0) {
        glUniform2fv(location, 1, (const GLfloat *)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector3D &value)
{
    if (location >= 0) {
        glUniform3fv(location, 1, (const GLfloat *)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QVector4D &value)
{
    if (location >= 0) {
        glUniform4fv(location, 1, (const GLfloat *)&value);
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QMatrix4x4 &value)
{
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
    }
    return (location >= 0);
}

bool GLShader::setUniform(int location, const QColor &color)
{
    if (location >= 0) {
        glUniform4f(location, color.redF(), color.greenF(), color.blueF(), color.alphaF());
    }
    return (location >= 0);
}

int GLShader::attributeLocation(const char *name)
{
    int location = glGetAttribLocation(mProgram, name);
    return location;
}

bool GLShader::setAttribute(const char *name, float value)
{
    int location = attributeLocation(name);
    if (location >= 0) {
        glVertexAttrib1f(location, value);
    }
    return (location >= 0);
}

QMatrix4x4 GLShader::getUniformMatrix4x4(const char *name)
{
    int location = uniformLocation(name);
    if (location >= 0) {
        GLfloat m[16];
        glGetnUniformfv(mProgram, location, sizeof(m), m);
        QMatrix4x4 matrix(m[0], m[4], m[8], m[12],
                          m[1], m[5], m[9], m[13],
                          m[2], m[6], m[10], m[14],
                          m[3], m[7], m[11], m[15]);
        matrix.optimize();
        return matrix;
    } else {
        return QMatrix4x4();
    }
}

//****************************************
// ShaderManager
//****************************************
std::unique_ptr<ShaderManager> ShaderManager::s_shaderManager;

ShaderManager *ShaderManager::instance()
{
    if (!s_shaderManager) {
        s_shaderManager.reset(new ShaderManager());
    }
    return s_shaderManager.get();
}

void ShaderManager::cleanup()
{
    s_shaderManager.reset();
}

ShaderManager::ShaderManager()
{
}

ShaderManager::~ShaderManager()
{
    while (!m_boundShaders.isEmpty()) {
        popShader();
    }
}

QByteArray ShaderManager::generateVertexSource(ShaderTraits traits) const
{
    QByteArray source;
    QTextStream stream(&source);

    GLPlatform *const gl = GLPlatform::instance();
    QByteArray attribute, varying;

    if (!gl->isGLES()) {
        const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);

        attribute = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_140 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_140) {
            stream << "#version 140\n\n";
        }
    } else {
        const bool glsl_es_300 = gl->glslVersion() >= kVersionNumber(3, 0);

        attribute = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_es_300 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }
    }

    stream << attribute << " vec4 position;\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << attribute << " vec4 texcoord;\n\n";
        stream << varying << " vec2 texcoord0;\n\n";
    } else {
        stream << "\n";
    }

    stream << "uniform mat4 modelViewProjectionMatrix;\n\n";

    stream << "void main()\n{\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << "    texcoord0 = texcoord.st;\n";
    }

    stream << "    gl_Position = modelViewProjectionMatrix * position;\n";
    stream << "}\n";

    stream.flush();
    return source;
}

QByteArray ShaderManager::generateFragmentSource(ShaderTraits traits) const
{
    QByteArray source;
    QTextStream stream(&source);

    GLPlatform *const gl = GLPlatform::instance();
    QByteArray varying, output, textureLookup;

    if (!gl->isGLES()) {
        const bool glsl_140 = gl->glslVersion() >= kVersionNumber(1, 40);

        if (glsl_140) {
            stream << "#version 140\n\n";
        }

        varying = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_140 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_140 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    } else {
        const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0);

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }

        // From the GLSL ES specification:
        //
        //     "The fragment language has no default precision qualifier for floating point types."
        stream << "precision highp float;\n\n";

        varying = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_es_300 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_es_300 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    }

    if (traits & ShaderTrait::MapTexture) {
        stream << "uniform sampler2D sampler;\n";

        if (traits & ShaderTrait::Modulate) {
            stream << "uniform vec4 modulation;\n";
        }
        if (traits & ShaderTrait::AdjustSaturation) {
            stream << "uniform float saturation;\n";
        }

        stream << "\n"
               << varying << " vec2 texcoord0;\n";

    } else if (traits & ShaderTrait::UniformColor) {
        stream << "uniform vec4 geometryColor;\n";
    }

    if (output != QByteArrayLiteral("gl_FragColor")) {
        stream << "\nout vec4 " << output << ";\n";
    }

    stream << "\nvoid main(void)\n{\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << "vec2 texcoordC = texcoord0;\n";

        if (traits & (ShaderTrait::Modulate | ShaderTrait::AdjustSaturation)) {
            stream << "    vec4 texel = " << textureLookup << "(sampler, texcoordC);\n";
            if (traits & ShaderTrait::Modulate) {
                stream << "    texel *= modulation;\n";
            }
            if (traits & ShaderTrait::AdjustSaturation) {
                stream << "    texel.rgb = mix(vec3(dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722))), texel.rgb, saturation);\n";
            }

            stream << "    " << output << " = texel;\n";
        } else {
            stream << "    " << output << " = " << textureLookup << "(sampler, texcoordC);\n";
        }
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "    " << output << " = geometryColor;\n";
    }

    stream << "}";
    stream.flush();
    return source;
}

std::unique_ptr<GLShader> ShaderManager::generateShader(ShaderTraits traits)
{
    return generateCustomShader(traits);
}

std::unique_ptr<GLShader> ShaderManager::generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    const QByteArray vertex = vertexSource.isEmpty() ? generateVertexSource(traits) : vertexSource;
    const QByteArray fragment = fragmentSource.isEmpty() ? generateFragmentSource(traits) : fragmentSource;

#if 0
    qCDebug(LIBKWINGLUTILS) << "**************";
    qCDebug(LIBKWINGLUTILS) << vertex;
    qCDebug(LIBKWINGLUTILS) << "**************";
    qCDebug(LIBKWINGLUTILS) << fragment;
    qCDebug(LIBKWINGLUTILS) << "**************";
#endif

    std::unique_ptr<GLShader> shader{new GLShader(GLShader::ExplicitLinking)};
    shader->load(vertex, fragment);

    shader->bindAttributeLocation("position", VA_Position);
    shader->bindAttributeLocation("texcoord", VA_TexCoord);
    shader->bindFragDataLocation("fragColor", 0);

    shader->link();
    return shader;
}

static QString resolveShaderFilePath(const QString &filePath)
{
    QString suffix;
    QString extension;

    const qint64 coreVersionNumber = GLPlatform::instance()->isGLES() ? kVersionNumber(3, 0) : kVersionNumber(1, 40);
    if (GLPlatform::instance()->glslVersion() >= coreVersionNumber) {
        suffix = QStringLiteral("_core");
    }

    if (filePath.endsWith(QStringLiteral(".frag"))) {
        extension = QStringLiteral(".frag");
    } else if (filePath.endsWith(QStringLiteral(".vert"))) {
        extension = QStringLiteral(".vert");
    } else {
        qCWarning(LIBKWINGLUTILS) << filePath << "must end either with .vert or .frag";
        return QString();
    }

    const QString prefix = filePath.chopped(extension.size());
    return prefix + suffix + extension;
}

std::unique_ptr<GLShader> ShaderManager::generateShaderFromFile(ShaderTraits traits, const QString &vertexFile, const QString &fragmentFile)
{
    auto loadShaderFile = [](const QString &filePath) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
        qCCritical(LIBKWINGLUTILS) << "Failed to read shader " << filePath;
        return QByteArray();
    };
    QByteArray vertexSource;
    QByteArray fragmentSource;
    if (!vertexFile.isEmpty()) {
        vertexSource = loadShaderFile(resolveShaderFilePath(vertexFile));
        if (vertexSource.isEmpty()) {
            return std::unique_ptr<GLShader>(new GLShader());
        }
    }
    if (!fragmentFile.isEmpty()) {
        fragmentSource = loadShaderFile(resolveShaderFilePath(fragmentFile));
        if (fragmentSource.isEmpty()) {
            return std::unique_ptr<GLShader>(new GLShader());
        }
    }
    return generateCustomShader(traits, vertexSource, fragmentSource);
}

GLShader *ShaderManager::shader(ShaderTraits traits)
{
    std::unique_ptr<GLShader> &shader = m_shaderHash[traits];
    if (!shader) {
        shader = generateShader(traits);
    }
    return shader.get();
}

GLShader *ShaderManager::getBoundShader() const
{
    if (m_boundShaders.isEmpty()) {
        return nullptr;
    } else {
        return m_boundShaders.top();
    }
}

bool ShaderManager::isShaderBound() const
{
    return !m_boundShaders.isEmpty();
}

GLShader *ShaderManager::pushShader(ShaderTraits traits)
{
    GLShader *shader = this->shader(traits);
    pushShader(shader);
    return shader;
}

void ShaderManager::pushShader(GLShader *shader)
{
    // only bind shader if it is not already bound
    if (shader != getBoundShader()) {
        shader->bind();
    }
    m_boundShaders.push(shader);
}

void ShaderManager::popShader()
{
    if (m_boundShaders.isEmpty()) {
        return;
    }
    GLShader *shader = m_boundShaders.pop();
    if (m_boundShaders.isEmpty()) {
        // no more shader bound - unbind
        shader->unbind();
    } else if (shader != m_boundShaders.top()) {
        // only rebind if a different shader is on top of stack
        m_boundShaders.top()->bind();
    }
}

void ShaderManager::bindFragDataLocations(GLShader *shader)
{
    shader->bindFragDataLocation("fragColor", 0);
}

void ShaderManager::bindAttributeLocations(GLShader *shader) const
{
    shader->bindAttributeLocation("vertex", VA_Position);
    shader->bindAttributeLocation("texCoord", VA_TexCoord);
}

std::unique_ptr<GLShader> ShaderManager::loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    std::unique_ptr<GLShader> shader{new GLShader(GLShader::ExplicitLinking)};
    shader->load(vertexSource, fragmentSource);
    bindAttributeLocations(shader.get());
    bindFragDataLocations(shader.get());
    shader->link();
    return shader;
}

/***  GLFramebuffer  ***/
bool GLFramebuffer::sSupported = false;
bool GLFramebuffer::s_blitSupported = false;
QStack<GLFramebuffer *> GLFramebuffer::s_fbos = QStack<GLFramebuffer *>();

void GLFramebuffer::initStatic()
{
    if (GLPlatform::instance()->isGLES()) {
        sSupported = true;
        s_blitSupported = hasGLVersion(3, 0);
    } else {
        sSupported = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_framebuffer_object")) || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));

        s_blitSupported = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_framebuffer_object")) || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_blit"));
    }
}

void GLFramebuffer::cleanup()
{
    Q_ASSERT(s_fbos.isEmpty());
    sSupported = false;
    s_blitSupported = false;
}

bool GLFramebuffer::blitSupported()
{
    return s_blitSupported;
}

GLFramebuffer *GLFramebuffer::currentFramebuffer()
{
    return s_fbos.isEmpty() ? nullptr : s_fbos.top();
}

void GLFramebuffer::pushFramebuffer(GLFramebuffer *fbo)
{
    fbo->bind();
    s_fbos.push(fbo);
}

void GLFramebuffer::pushFramebuffers(QStack<GLFramebuffer *> fbos)
{
    fbos.top()->bind();
    s_fbos.append(fbos);
}

GLFramebuffer *GLFramebuffer::popFramebuffer()
{
    GLFramebuffer *ret = s_fbos.pop();
    if (!s_fbos.isEmpty()) {
        s_fbos.top()->bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    return ret;
}

GLFramebuffer::GLFramebuffer()
{
}

GLFramebuffer::GLFramebuffer(GLTexture *colorAttachment)
    : mSize(colorAttachment->size())
{
    // Make sure FBO is supported
    if (sSupported && !colorAttachment->isNull()) {
        initFBO(colorAttachment);
    } else {
        qCCritical(LIBKWINGLUTILS) << "Framebuffer objects aren't supported!";
    }
}

GLFramebuffer::GLFramebuffer(GLuint handle, const QSize &size)
    : mFramebuffer(handle)
    , mSize(size)
    , mValid(true)
    , mForeign(true)
{
}

GLFramebuffer::~GLFramebuffer()
{
    if (!mForeign && mValid) {
        glDeleteFramebuffers(1, &mFramebuffer);
    }
}

bool GLFramebuffer::bind()
{
    if (!valid()) {
        qCCritical(LIBKWINGLUTILS) << "Can't enable invalid framebuffer object!";
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, handle());
    glViewport(0, 0, mSize.width(), mSize.height());

    return true;
}

static QString formatFramebufferStatus(GLenum status)
{
    switch (status) {
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        // An attachment is the wrong type / is invalid / has 0 width or height
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        // There are no images attached to the framebuffer
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
    case GL_FRAMEBUFFER_UNSUPPORTED:
        // A format or the combination of formats of the attachments is unsupported
        return QStringLiteral("GL_FRAMEBUFFER_UNSUPPORTED");
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
        // Not all attached images have the same width and height
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT");
    case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
        // The color attachments don't have the same format
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT");
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
        // The attachments don't have the same number of samples
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
        // The draw buffer is missing
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
        // The read buffer is missing
        return QStringLiteral("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
    default:
        return QStringLiteral("Unknown (0x") + QString::number(status, 16) + QStringLiteral(")");
    }
}

void GLFramebuffer::initFBO(GLTexture *colorAttachment)
{
#if DEBUG_GLFRAMEBUFFER
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        qCCritical(LIBKWINGLUTILS) << "Error status when entering GLFramebuffer::initFBO: " << formatGLError(err);
#endif

    GLuint prevFbo = 0;
    if (const GLFramebuffer *current = currentFramebuffer()) {
        prevFbo = current->handle();
    }

    glGenFramebuffers(1, &mFramebuffer);

#if DEBUG_GLFRAMEBUFFER
    if ((err = glGetError()) != GL_NO_ERROR) {
        qCCritical(LIBKWINGLUTILS) << "glGenFramebuffers failed: " << formatGLError(err);
        return;
    }
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

#if DEBUG_GLFRAMEBUFFER
    if ((err = glGetError()) != GL_NO_ERROR) {
        qCCritical(LIBKWINGLUTILS) << "glBindFramebuffer failed: " << formatGLError(err);
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }
#endif

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           colorAttachment->target(), colorAttachment->texture(), 0);

#if DEBUG_GLFRAMEBUFFER
    if ((err = glGetError()) != GL_NO_ERROR) {
        qCCritical(LIBKWINGLUTILS) << "glFramebufferTexture2D failed: " << formatGLError(err);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }
#endif

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // We have an incomplete framebuffer, consider it invalid
        if (status == 0) {
            qCCritical(LIBKWINGLUTILS) << "glCheckFramebufferStatus failed: " << formatGLError(glGetError());
        } else {
            qCCritical(LIBKWINGLUTILS) << "Invalid framebuffer status: " << formatFramebufferStatus(status);
        }
        glDeleteFramebuffers(1, &mFramebuffer);
        return;
    }

    mValid = true;
}

void GLFramebuffer::blitFromFramebuffer(const QRect &source, const QRect &destination, GLenum filter)
{
    if (!valid()) {
        return;
    }

    const GLFramebuffer *top = currentFramebuffer();
    GLFramebuffer::pushFramebuffer(this);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, handle());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, top->handle());

    const QRect s = source.isNull() ? QRect(QPoint(0, 0), top->size()) : source;
    const QRect d = destination.isNull() ? QRect(QPoint(0, 0), size()) : destination;

    const GLuint srcX0 = s.x();
    const GLuint srcY0 = top->size().height() - (s.y() + s.height());
    const GLuint srcX1 = s.x() + s.width();
    const GLuint srcY1 = top->size().height() - s.y();

    const GLuint dstX0 = d.x();
    const GLuint dstY0 = mSize.height() - (d.y() + d.height());
    const GLuint dstX1 = d.x() + d.width();
    const GLuint dstY1 = mSize.height() - d.y();

    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, filter);

    GLFramebuffer::popFramebuffer();
}

// ------------------------------------------------------------------

static const uint16_t indices[] = {
    1, 0, 3, 3, 2, 1, 5, 4, 7, 7, 6, 5, 9, 8, 11, 11, 10, 9,
    13, 12, 15, 15, 14, 13, 17, 16, 19, 19, 18, 17, 21, 20, 23, 23, 22, 21,
    25, 24, 27, 27, 26, 25, 29, 28, 31, 31, 30, 29, 33, 32, 35, 35, 34, 33,
    37, 36, 39, 39, 38, 37, 41, 40, 43, 43, 42, 41, 45, 44, 47, 47, 46, 45,
    49, 48, 51, 51, 50, 49, 53, 52, 55, 55, 54, 53, 57, 56, 59, 59, 58, 57,
    61, 60, 63, 63, 62, 61, 65, 64, 67, 67, 66, 65, 69, 68, 71, 71, 70, 69,
    73, 72, 75, 75, 74, 73, 77, 76, 79, 79, 78, 77, 81, 80, 83, 83, 82, 81,
    85, 84, 87, 87, 86, 85, 89, 88, 91, 91, 90, 89, 93, 92, 95, 95, 94, 93,
    97, 96, 99, 99, 98, 97, 101, 100, 103, 103, 102, 101, 105, 104, 107, 107, 106, 105,
    109, 108, 111, 111, 110, 109, 113, 112, 115, 115, 114, 113, 117, 116, 119, 119, 118, 117,
    121, 120, 123, 123, 122, 121, 125, 124, 127, 127, 126, 125, 129, 128, 131, 131, 130, 129,
    133, 132, 135, 135, 134, 133, 137, 136, 139, 139, 138, 137, 141, 140, 143, 143, 142, 141,
    145, 144, 147, 147, 146, 145, 149, 148, 151, 151, 150, 149, 153, 152, 155, 155, 154, 153,
    157, 156, 159, 159, 158, 157, 161, 160, 163, 163, 162, 161, 165, 164, 167, 167, 166, 165,
    169, 168, 171, 171, 170, 169, 173, 172, 175, 175, 174, 173, 177, 176, 179, 179, 178, 177,
    181, 180, 183, 183, 182, 181, 185, 184, 187, 187, 186, 185, 189, 188, 191, 191, 190, 189,
    193, 192, 195, 195, 194, 193, 197, 196, 199, 199, 198, 197, 201, 200, 203, 203, 202, 201,
    205, 204, 207, 207, 206, 205, 209, 208, 211, 211, 210, 209, 213, 212, 215, 215, 214, 213,
    217, 216, 219, 219, 218, 217, 221, 220, 223, 223, 222, 221, 225, 224, 227, 227, 226, 225,
    229, 228, 231, 231, 230, 229, 233, 232, 235, 235, 234, 233, 237, 236, 239, 239, 238, 237,
    241, 240, 243, 243, 242, 241, 245, 244, 247, 247, 246, 245, 249, 248, 251, 251, 250, 249,
    253, 252, 255, 255, 254, 253, 257, 256, 259, 259, 258, 257, 261, 260, 263, 263, 262, 261,
    265, 264, 267, 267, 266, 265, 269, 268, 271, 271, 270, 269, 273, 272, 275, 275, 274, 273,
    277, 276, 279, 279, 278, 277, 281, 280, 283, 283, 282, 281, 285, 284, 287, 287, 286, 285,
    289, 288, 291, 291, 290, 289, 293, 292, 295, 295, 294, 293, 297, 296, 299, 299, 298, 297,
    301, 300, 303, 303, 302, 301, 305, 304, 307, 307, 306, 305, 309, 308, 311, 311, 310, 309,
    313, 312, 315, 315, 314, 313, 317, 316, 319, 319, 318, 317, 321, 320, 323, 323, 322, 321,
    325, 324, 327, 327, 326, 325, 329, 328, 331, 331, 330, 329, 333, 332, 335, 335, 334, 333,
    337, 336, 339, 339, 338, 337, 341, 340, 343, 343, 342, 341, 345, 344, 347, 347, 346, 345,
    349, 348, 351, 351, 350, 349, 353, 352, 355, 355, 354, 353, 357, 356, 359, 359, 358, 357,
    361, 360, 363, 363, 362, 361, 365, 364, 367, 367, 366, 365, 369, 368, 371, 371, 370, 369,
    373, 372, 375, 375, 374, 373, 377, 376, 379, 379, 378, 377, 381, 380, 383, 383, 382, 381,
    385, 384, 387, 387, 386, 385, 389, 388, 391, 391, 390, 389, 393, 392, 395, 395, 394, 393,
    397, 396, 399, 399, 398, 397, 401, 400, 403, 403, 402, 401, 405, 404, 407, 407, 406, 405,
    409, 408, 411, 411, 410, 409, 413, 412, 415, 415, 414, 413, 417, 416, 419, 419, 418, 417,
    421, 420, 423, 423, 422, 421, 425, 424, 427, 427, 426, 425, 429, 428, 431, 431, 430, 429,
    433, 432, 435, 435, 434, 433, 437, 436, 439, 439, 438, 437, 441, 440, 443, 443, 442, 441,
    445, 444, 447, 447, 446, 445, 449, 448, 451, 451, 450, 449, 453, 452, 455, 455, 454, 453,
    457, 456, 459, 459, 458, 457, 461, 460, 463, 463, 462, 461, 465, 464, 467, 467, 466, 465,
    469, 468, 471, 471, 470, 469, 473, 472, 475, 475, 474, 473, 477, 476, 479, 479, 478, 477,
    481, 480, 483, 483, 482, 481, 485, 484, 487, 487, 486, 485, 489, 488, 491, 491, 490, 489,
    493, 492, 495, 495, 494, 493, 497, 496, 499, 499, 498, 497, 501, 500, 503, 503, 502, 501,
    505, 504, 507, 507, 506, 505, 509, 508, 511, 511, 510, 509, 513, 512, 515, 515, 514, 513,
    517, 516, 519, 519, 518, 517, 521, 520, 523, 523, 522, 521, 525, 524, 527, 527, 526, 525,
    529, 528, 531, 531, 530, 529, 533, 532, 535, 535, 534, 533, 537, 536, 539, 539, 538, 537,
    541, 540, 543, 543, 542, 541, 545, 544, 547, 547, 546, 545, 549, 548, 551, 551, 550, 549,
    553, 552, 555, 555, 554, 553, 557, 556, 559, 559, 558, 557, 561, 560, 563, 563, 562, 561,
    565, 564, 567, 567, 566, 565, 569, 568, 571, 571, 570, 569, 573, 572, 575, 575, 574, 573,
    577, 576, 579, 579, 578, 577, 581, 580, 583, 583, 582, 581, 585, 584, 587, 587, 586, 585,
    589, 588, 591, 591, 590, 589, 593, 592, 595, 595, 594, 593, 597, 596, 599, 599, 598, 597,
    601, 600, 603, 603, 602, 601, 605, 604, 607, 607, 606, 605, 609, 608, 611, 611, 610, 609,
    613, 612, 615, 615, 614, 613, 617, 616, 619, 619, 618, 617, 621, 620, 623, 623, 622, 621,
    625, 624, 627, 627, 626, 625, 629, 628, 631, 631, 630, 629, 633, 632, 635, 635, 634, 633,
    637, 636, 639, 639, 638, 637, 641, 640, 643, 643, 642, 641, 645, 644, 647, 647, 646, 645,
    649, 648, 651, 651, 650, 649, 653, 652, 655, 655, 654, 653, 657, 656, 659, 659, 658, 657,
    661, 660, 663, 663, 662, 661, 665, 664, 667, 667, 666, 665, 669, 668, 671, 671, 670, 669,
    673, 672, 675, 675, 674, 673, 677, 676, 679, 679, 678, 677, 681, 680, 683, 683, 682, 681,
    685, 684, 687, 687, 686, 685, 689, 688, 691, 691, 690, 689, 693, 692, 695, 695, 694, 693,
    697, 696, 699, 699, 698, 697, 701, 700, 703, 703, 702, 701, 705, 704, 707, 707, 706, 705,
    709, 708, 711, 711, 710, 709, 713, 712, 715, 715, 714, 713, 717, 716, 719, 719, 718, 717,
    721, 720, 723, 723, 722, 721, 725, 724, 727, 727, 726, 725, 729, 728, 731, 731, 730, 729,
    733, 732, 735, 735, 734, 733, 737, 736, 739, 739, 738, 737, 741, 740, 743, 743, 742, 741,
    745, 744, 747, 747, 746, 745, 749, 748, 751, 751, 750, 749, 753, 752, 755, 755, 754, 753,
    757, 756, 759, 759, 758, 757, 761, 760, 763, 763, 762, 761, 765, 764, 767, 767, 766, 765,
    769, 768, 771, 771, 770, 769, 773, 772, 775, 775, 774, 773, 777, 776, 779, 779, 778, 777,
    781, 780, 783, 783, 782, 781, 785, 784, 787, 787, 786, 785, 789, 788, 791, 791, 790, 789,
    793, 792, 795, 795, 794, 793, 797, 796, 799, 799, 798, 797, 801, 800, 803, 803, 802, 801,
    805, 804, 807, 807, 806, 805, 809, 808, 811, 811, 810, 809, 813, 812, 815, 815, 814, 813,
    817, 816, 819, 819, 818, 817, 821, 820, 823, 823, 822, 821, 825, 824, 827, 827, 826, 825,
    829, 828, 831, 831, 830, 829, 833, 832, 835, 835, 834, 833, 837, 836, 839, 839, 838, 837,
    841, 840, 843, 843, 842, 841, 845, 844, 847, 847, 846, 845, 849, 848, 851, 851, 850, 849,
    853, 852, 855, 855, 854, 853, 857, 856, 859, 859, 858, 857, 861, 860, 863, 863, 862, 861,
    865, 864, 867, 867, 866, 865, 869, 868, 871, 871, 870, 869, 873, 872, 875, 875, 874, 873,
    877, 876, 879, 879, 878, 877, 881, 880, 883, 883, 882, 881, 885, 884, 887, 887, 886, 885,
    889, 888, 891, 891, 890, 889, 893, 892, 895, 895, 894, 893, 897, 896, 899, 899, 898, 897,
    901, 900, 903, 903, 902, 901, 905, 904, 907, 907, 906, 905, 909, 908, 911, 911, 910, 909,
    913, 912, 915, 915, 914, 913, 917, 916, 919, 919, 918, 917, 921, 920, 923, 923, 922, 921,
    925, 924, 927, 927, 926, 925, 929, 928, 931, 931, 930, 929, 933, 932, 935, 935, 934, 933,
    937, 936, 939, 939, 938, 937, 941, 940, 943, 943, 942, 941, 945, 944, 947, 947, 946, 945,
    949, 948, 951, 951, 950, 949, 953, 952, 955, 955, 954, 953, 957, 956, 959, 959, 958, 957,
    961, 960, 963, 963, 962, 961, 965, 964, 967, 967, 966, 965, 969, 968, 971, 971, 970, 969,
    973, 972, 975, 975, 974, 973, 977, 976, 979, 979, 978, 977, 981, 980, 983, 983, 982, 981,
    985, 984, 987, 987, 986, 985, 989, 988, 991, 991, 990, 989, 993, 992, 995, 995, 994, 993,
    997, 996, 999, 999, 998, 997, 1001, 1000, 1003, 1003, 1002, 1001, 1005, 1004, 1007, 1007, 1006, 1005,
    1009, 1008, 1011, 1011, 1010, 1009, 1013, 1012, 1015, 1015, 1014, 1013, 1017, 1016, 1019, 1019, 1018, 1017,
    1021, 1020, 1023, 1023, 1022, 1021, 1025, 1024, 1027, 1027, 1026, 1025, 1029, 1028, 1031, 1031, 1030, 1029,
    1033, 1032, 1035, 1035, 1034, 1033, 1037, 1036, 1039, 1039, 1038, 1037, 1041, 1040, 1043, 1043, 1042, 1041,
    1045, 1044, 1047, 1047, 1046, 1045, 1049, 1048, 1051, 1051, 1050, 1049, 1053, 1052, 1055, 1055, 1054, 1053,
    1057, 1056, 1059, 1059, 1058, 1057, 1061, 1060, 1063, 1063, 1062, 1061, 1065, 1064, 1067, 1067, 1066, 1065,
    1069, 1068, 1071, 1071, 1070, 1069, 1073, 1072, 1075, 1075, 1074, 1073, 1077, 1076, 1079, 1079, 1078, 1077,
    1081, 1080, 1083, 1083, 1082, 1081, 1085, 1084, 1087, 1087, 1086, 1085, 1089, 1088, 1091, 1091, 1090, 1089,
    1093, 1092, 1095, 1095, 1094, 1093, 1097, 1096, 1099, 1099, 1098, 1097, 1101, 1100, 1103, 1103, 1102, 1101,
    1105, 1104, 1107, 1107, 1106, 1105, 1109, 1108, 1111, 1111, 1110, 1109, 1113, 1112, 1115, 1115, 1114, 1113,
    1117, 1116, 1119, 1119, 1118, 1117, 1121, 1120, 1123, 1123, 1122, 1121, 1125, 1124, 1127, 1127, 1126, 1125,
    1129, 1128, 1131, 1131, 1130, 1129, 1133, 1132, 1135, 1135, 1134, 1133, 1137, 1136, 1139, 1139, 1138, 1137,
    1141, 1140, 1143, 1143, 1142, 1141, 1145, 1144, 1147, 1147, 1146, 1145, 1149, 1148, 1151, 1151, 1150, 1149,
    1153, 1152, 1155, 1155, 1154, 1153, 1157, 1156, 1159, 1159, 1158, 1157, 1161, 1160, 1163, 1163, 1162, 1161,
    1165, 1164, 1167, 1167, 1166, 1165, 1169, 1168, 1171, 1171, 1170, 1169, 1173, 1172, 1175, 1175, 1174, 1173,
    1177, 1176, 1179, 1179, 1178, 1177, 1181, 1180, 1183, 1183, 1182, 1181, 1185, 1184, 1187, 1187, 1186, 1185,
    1189, 1188, 1191, 1191, 1190, 1189, 1193, 1192, 1195, 1195, 1194, 1193, 1197, 1196, 1199, 1199, 1198, 1197,
    1201, 1200, 1203, 1203, 1202, 1201, 1205, 1204, 1207, 1207, 1206, 1205, 1209, 1208, 1211, 1211, 1210, 1209,
    1213, 1212, 1215, 1215, 1214, 1213, 1217, 1216, 1219, 1219, 1218, 1217, 1221, 1220, 1223, 1223, 1222, 1221,
    1225, 1224, 1227, 1227, 1226, 1225, 1229, 1228, 1231, 1231, 1230, 1229, 1233, 1232, 1235, 1235, 1234, 1233,
    1237, 1236, 1239, 1239, 1238, 1237, 1241, 1240, 1243, 1243, 1242, 1241, 1245, 1244, 1247, 1247, 1246, 1245,
    1249, 1248, 1251, 1251, 1250, 1249, 1253, 1252, 1255, 1255, 1254, 1253, 1257, 1256, 1259, 1259, 1258, 1257,
    1261, 1260, 1263, 1263, 1262, 1261, 1265, 1264, 1267, 1267, 1266, 1265, 1269, 1268, 1271, 1271, 1270, 1269,
    1273, 1272, 1275, 1275, 1274, 1273, 1277, 1276, 1279, 1279, 1278, 1277, 1281, 1280, 1283, 1283, 1282, 1281,
    1285, 1284, 1287, 1287, 1286, 1285, 1289, 1288, 1291, 1291, 1290, 1289, 1293, 1292, 1295, 1295, 1294, 1293,
    1297, 1296, 1299, 1299, 1298, 1297, 1301, 1300, 1303, 1303, 1302, 1301, 1305, 1304, 1307, 1307, 1306, 1305,
    1309, 1308, 1311, 1311, 1310, 1309, 1313, 1312, 1315, 1315, 1314, 1313, 1317, 1316, 1319, 1319, 1318, 1317,
    1321, 1320, 1323, 1323, 1322, 1321, 1325, 1324, 1327, 1327, 1326, 1325, 1329, 1328, 1331, 1331, 1330, 1329,
    1333, 1332, 1335, 1335, 1334, 1333, 1337, 1336, 1339, 1339, 1338, 1337, 1341, 1340, 1343, 1343, 1342, 1341,
    1345, 1344, 1347, 1347, 1346, 1345, 1349, 1348, 1351, 1351, 1350, 1349, 1353, 1352, 1355, 1355, 1354, 1353,
    1357, 1356, 1359, 1359, 1358, 1357, 1361, 1360, 1363, 1363, 1362, 1361, 1365, 1364, 1367, 1367, 1366, 1365,
    1369, 1368, 1371, 1371, 1370, 1369, 1373, 1372, 1375, 1375, 1374, 1373, 1377, 1376, 1379, 1379, 1378, 1377,
    1381, 1380, 1383, 1383, 1382, 1381, 1385, 1384, 1387, 1387, 1386, 1385, 1389, 1388, 1391, 1391, 1390, 1389,
    1393, 1392, 1395, 1395, 1394, 1393, 1397, 1396, 1399, 1399, 1398, 1397, 1401, 1400, 1403, 1403, 1402, 1401,
    1405, 1404, 1407, 1407, 1406, 1405, 1409, 1408, 1411, 1411, 1410, 1409, 1413, 1412, 1415, 1415, 1414, 1413,
    1417, 1416, 1419, 1419, 1418, 1417, 1421, 1420, 1423, 1423, 1422, 1421, 1425, 1424, 1427, 1427, 1426, 1425,
    1429, 1428, 1431, 1431, 1430, 1429, 1433, 1432, 1435, 1435, 1434, 1433, 1437, 1436, 1439, 1439, 1438, 1437,
    1441, 1440, 1443, 1443, 1442, 1441, 1445, 1444, 1447, 1447, 1446, 1445, 1449, 1448, 1451, 1451, 1450, 1449,
    1453, 1452, 1455, 1455, 1454, 1453, 1457, 1456, 1459, 1459, 1458, 1457, 1461, 1460, 1463, 1463, 1462, 1461,
    1465, 1464, 1467, 1467, 1466, 1465, 1469, 1468, 1471, 1471, 1470, 1469, 1473, 1472, 1475, 1475, 1474, 1473,
    1477, 1476, 1479, 1479, 1478, 1477, 1481, 1480, 1483, 1483, 1482, 1481, 1485, 1484, 1487, 1487, 1486, 1485,
    1489, 1488, 1491, 1491, 1490, 1489, 1493, 1492, 1495, 1495, 1494, 1493, 1497, 1496, 1499, 1499, 1498, 1497,
    1501, 1500, 1503, 1503, 1502, 1501, 1505, 1504, 1507, 1507, 1506, 1505, 1509, 1508, 1511, 1511, 1510, 1509,
    1513, 1512, 1515, 1515, 1514, 1513, 1517, 1516, 1519, 1519, 1518, 1517, 1521, 1520, 1523, 1523, 1522, 1521,
    1525, 1524, 1527, 1527, 1526, 1525, 1529, 1528, 1531, 1531, 1530, 1529, 1533, 1532, 1535, 1535, 1534, 1533,
    1537, 1536, 1539, 1539, 1538, 1537, 1541, 1540, 1543, 1543, 1542, 1541, 1545, 1544, 1547, 1547, 1546, 1545,
    1549, 1548, 1551, 1551, 1550, 1549, 1553, 1552, 1555, 1555, 1554, 1553, 1557, 1556, 1559, 1559, 1558, 1557,
    1561, 1560, 1563, 1563, 1562, 1561, 1565, 1564, 1567, 1567, 1566, 1565, 1569, 1568, 1571, 1571, 1570, 1569,
    1573, 1572, 1575, 1575, 1574, 1573, 1577, 1576, 1579, 1579, 1578, 1577, 1581, 1580, 1583, 1583, 1582, 1581,
    1585, 1584, 1587, 1587, 1586, 1585, 1589, 1588, 1591, 1591, 1590, 1589, 1593, 1592, 1595, 1595, 1594, 1593,
    1597, 1596, 1599, 1599, 1598, 1597, 1601, 1600, 1603, 1603, 1602, 1601, 1605, 1604, 1607, 1607, 1606, 1605,
    1609, 1608, 1611, 1611, 1610, 1609, 1613, 1612, 1615, 1615, 1614, 1613, 1617, 1616, 1619, 1619, 1618, 1617,
    1621, 1620, 1623, 1623, 1622, 1621, 1625, 1624, 1627, 1627, 1626, 1625, 1629, 1628, 1631, 1631, 1630, 1629,
    1633, 1632, 1635, 1635, 1634, 1633, 1637, 1636, 1639, 1639, 1638, 1637, 1641, 1640, 1643, 1643, 1642, 1641,
    1645, 1644, 1647, 1647, 1646, 1645, 1649, 1648, 1651, 1651, 1650, 1649, 1653, 1652, 1655, 1655, 1654, 1653,
    1657, 1656, 1659, 1659, 1658, 1657, 1661, 1660, 1663, 1663, 1662, 1661, 1665, 1664, 1667, 1667, 1666, 1665,
    1669, 1668, 1671, 1671, 1670, 1669, 1673, 1672, 1675, 1675, 1674, 1673, 1677, 1676, 1679, 1679, 1678, 1677,
    1681, 1680, 1683, 1683, 1682, 1681, 1685, 1684, 1687, 1687, 1686, 1685, 1689, 1688, 1691, 1691, 1690, 1689,
    1693, 1692, 1695, 1695, 1694, 1693, 1697, 1696, 1699, 1699, 1698, 1697, 1701, 1700, 1703, 1703, 1702, 1701,
    1705, 1704, 1707, 1707, 1706, 1705, 1709, 1708, 1711, 1711, 1710, 1709, 1713, 1712, 1715, 1715, 1714, 1713,
    1717, 1716, 1719, 1719, 1718, 1717, 1721, 1720, 1723, 1723, 1722, 1721, 1725, 1724, 1727, 1727, 1726, 1725,
    1729, 1728, 1731, 1731, 1730, 1729, 1733, 1732, 1735, 1735, 1734, 1733, 1737, 1736, 1739, 1739, 1738, 1737,
    1741, 1740, 1743, 1743, 1742, 1741, 1745, 1744, 1747, 1747, 1746, 1745, 1749, 1748, 1751, 1751, 1750, 1749,
    1753, 1752, 1755, 1755, 1754, 1753, 1757, 1756, 1759, 1759, 1758, 1757, 1761, 1760, 1763, 1763, 1762, 1761,
    1765, 1764, 1767, 1767, 1766, 1765, 1769, 1768, 1771, 1771, 1770, 1769, 1773, 1772, 1775, 1775, 1774, 1773,
    1777, 1776, 1779, 1779, 1778, 1777, 1781, 1780, 1783, 1783, 1782, 1781, 1785, 1784, 1787, 1787, 1786, 1785,
    1789, 1788, 1791, 1791, 1790, 1789, 1793, 1792, 1795, 1795, 1794, 1793, 1797, 1796, 1799, 1799, 1798, 1797,
    1801, 1800, 1803, 1803, 1802, 1801, 1805, 1804, 1807, 1807, 1806, 1805, 1809, 1808, 1811, 1811, 1810, 1809,
    1813, 1812, 1815, 1815, 1814, 1813, 1817, 1816, 1819, 1819, 1818, 1817, 1821, 1820, 1823, 1823, 1822, 1821,
    1825, 1824, 1827, 1827, 1826, 1825, 1829, 1828, 1831, 1831, 1830, 1829, 1833, 1832, 1835, 1835, 1834, 1833,
    1837, 1836, 1839, 1839, 1838, 1837, 1841, 1840, 1843, 1843, 1842, 1841, 1845, 1844, 1847, 1847, 1846, 1845,
    1849, 1848, 1851, 1851, 1850, 1849, 1853, 1852, 1855, 1855, 1854, 1853, 1857, 1856, 1859, 1859, 1858, 1857,
    1861, 1860, 1863, 1863, 1862, 1861, 1865, 1864, 1867, 1867, 1866, 1865, 1869, 1868, 1871, 1871, 1870, 1869,
    1873, 1872, 1875, 1875, 1874, 1873, 1877, 1876, 1879, 1879, 1878, 1877, 1881, 1880, 1883, 1883, 1882, 1881,
    1885, 1884, 1887, 1887, 1886, 1885, 1889, 1888, 1891, 1891, 1890, 1889, 1893, 1892, 1895, 1895, 1894, 1893,
    1897, 1896, 1899, 1899, 1898, 1897, 1901, 1900, 1903, 1903, 1902, 1901, 1905, 1904, 1907, 1907, 1906, 1905,
    1909, 1908, 1911, 1911, 1910, 1909, 1913, 1912, 1915, 1915, 1914, 1913, 1917, 1916, 1919, 1919, 1918, 1917,
    1921, 1920, 1923, 1923, 1922, 1921, 1925, 1924, 1927, 1927, 1926, 1925, 1929, 1928, 1931, 1931, 1930, 1929,
    1933, 1932, 1935, 1935, 1934, 1933, 1937, 1936, 1939, 1939, 1938, 1937, 1941, 1940, 1943, 1943, 1942, 1941,
    1945, 1944, 1947, 1947, 1946, 1945, 1949, 1948, 1951, 1951, 1950, 1949, 1953, 1952, 1955, 1955, 1954, 1953,
    1957, 1956, 1959, 1959, 1958, 1957, 1961, 1960, 1963, 1963, 1962, 1961, 1965, 1964, 1967, 1967, 1966, 1965,
    1969, 1968, 1971, 1971, 1970, 1969, 1973, 1972, 1975, 1975, 1974, 1973, 1977, 1976, 1979, 1979, 1978, 1977,
    1981, 1980, 1983, 1983, 1982, 1981, 1985, 1984, 1987, 1987, 1986, 1985, 1989, 1988, 1991, 1991, 1990, 1989,
    1993, 1992, 1995, 1995, 1994, 1993, 1997, 1996, 1999, 1999, 1998, 1997, 2001, 2000, 2003, 2003, 2002, 2001,
    2005, 2004, 2007, 2007, 2006, 2005, 2009, 2008, 2011, 2011, 2010, 2009, 2013, 2012, 2015, 2015, 2014, 2013,
    2017, 2016, 2019, 2019, 2018, 2017, 2021, 2020, 2023, 2023, 2022, 2021, 2025, 2024, 2027, 2027, 2026, 2025,
    2029, 2028, 2031, 2031, 2030, 2029, 2033, 2032, 2035, 2035, 2034, 2033, 2037, 2036, 2039, 2039, 2038, 2037,
    2041, 2040, 2043, 2043, 2042, 2041, 2045, 2044, 2047, 2047, 2046, 2045};

// Certain GPUs, especially mobile, require the data copied to the GPU to be aligned to a
// certain amount of bytes. For example, the Mali GPU requires data to be aligned to 8 bytes.
// This function helps ensure that the data is aligned.
template<typename T>
T align(T value, int bytes)
{
    return (value + bytes - 1) & ~T(bytes - 1);
}

class IndexBuffer
{
public:
    IndexBuffer();
    ~IndexBuffer();

    void accommodate(int count);
    void bind();

private:
    GLuint m_buffer;
    size_t m_size;
    int m_count;
};

IndexBuffer::IndexBuffer()
{
    // The maximum number of quads we can render with 16 bit indices is 16,384.
    // But we start with 512 and grow the buffer as needed.
    m_size = sizeof(indices);
    m_count = m_size / (6 * sizeof(uint16_t));

    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer()
{
    glDeleteBuffers(1, &m_buffer);
}

void IndexBuffer::accommodate(int count)
{
    // Check if we need to grow the buffer.
    if (count <= m_count) {
        return;
    }

    size_t size = 6 * sizeof(uint16_t) * count;

    // Create a new buffer object
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, nullptr, GL_STATIC_DRAW);

    // Use the GPU to copy the data from the old object to the new object,
    glBindBuffer(GL_COPY_READ_BUFFER, m_buffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_ELEMENT_ARRAY_BUFFER, 0, 0, m_size);
    glDeleteBuffers(1, &m_buffer);
    glFlush(); // Needed to work around what appears to be a CP DMA issue in r600g

    // Map the new object and fill in the uninitialized section
    const GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
    uint16_t *map = (uint16_t *)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, m_size, size - m_size, access);

    const uint16_t index[] = {1, 0, 3, 3, 2, 1};
    for (int i = m_count; i < count; i++) {
        for (int j = 0; j < 6; j++) {
            *(map++) = i * 4 + index[j];
        }
    }

    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    m_buffer = buffer;
    m_count = count;
    m_size = size;
}

void IndexBuffer::bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
}

// ------------------------------------------------------------------

class BitRef
{
public:
    BitRef(uint32_t &bitfield, int bit)
        : m_bitfield(bitfield)
        , m_mask(1 << bit)
    {
    }

    void operator=(bool val)
    {
        if (val) {
            m_bitfield |= m_mask;
        } else {
            m_bitfield &= ~m_mask;
        }
    }

    operator bool() const
    {
        return m_bitfield & m_mask;
    }

private:
    uint32_t &m_bitfield;
    int const m_mask;
};

// ------------------------------------------------------------------

class Bitfield
{
public:
    Bitfield()
        : m_bitfield(0)
    {
    }
    Bitfield(uint32_t bits)
        : m_bitfield(bits)
    {
    }

    void set(int i)
    {
        m_bitfield |= (1 << i);
    }
    void clear(int i)
    {
        m_bitfield &= ~(1 << i);
    }

    BitRef operator[](int i)
    {
        return BitRef(m_bitfield, i);
    }
    operator uint32_t() const
    {
        return m_bitfield;
    }

private:
    uint32_t m_bitfield;
};

// ------------------------------------------------------------------

class BitfieldIterator
{
public:
    BitfieldIterator(uint32_t bitfield)
        : m_bitfield(bitfield)
    {
    }

    bool hasNext() const
    {
        return m_bitfield != 0;
    }

    int next()
    {
        const int bit = ffs(m_bitfield) - 1;
        m_bitfield ^= (1 << bit);
        return bit;
    }

private:
    uint32_t m_bitfield;
};

// ------------------------------------------------------------------

struct VertexAttrib
{
    int size;
    GLenum type;
    int offset;
};

// ------------------------------------------------------------------

struct BufferFence
{
    GLsync sync;
    intptr_t nextEnd;

    bool signaled() const
    {
        GLint value;
        glGetSynciv(sync, GL_SYNC_STATUS, 1, nullptr, &value);
        return value == GL_SIGNALED;
    }
};

static void deleteAll(std::deque<BufferFence> &fences)
{
    for (const BufferFence &fence : fences) {
        glDeleteSync(fence.sync);
    }

    fences.clear();
}

// ------------------------------------------------------------------

template<size_t Count>
struct FrameSizesArray
{
public:
    FrameSizesArray()
    {
        m_array.fill(0);
    }

    void push(size_t size)
    {
        m_array[m_index] = size;
        m_index = (m_index + 1) % Count;
    }

    size_t average() const
    {
        size_t sum = 0;
        for (size_t size : m_array) {
            sum += size;
        }
        return sum / Count;
    }

private:
    std::array<size_t, Count> m_array;
    int m_index = 0;
};

//*********************************
// GLVertexBufferPrivate
//*********************************
class GLVertexBufferPrivate
{
public:
    GLVertexBufferPrivate(GLVertexBuffer::UsageHint usageHint)
        : vertexCount(0)
        , persistent(false)
        , useColor(false)
        , color(0, 0, 0, 255)
        , bufferSize(0)
        , bufferEnd(0)
        , mappedSize(0)
        , frameSize(0)
        , nextOffset(0)
        , baseAddress(0)
        , map(nullptr)
    {
        glGenBuffers(1, &buffer);

        switch (usageHint) {
        case GLVertexBuffer::Dynamic:
            usage = GL_DYNAMIC_DRAW;
            break;
        case GLVertexBuffer::Static:
            usage = GL_STATIC_DRAW;
            break;
        default:
            usage = GL_STREAM_DRAW;
            break;
        }
    }

    ~GLVertexBufferPrivate()
    {
        deleteAll(fences);

        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
            map = nullptr;
        }
    }

    void interleaveArrays(float *array, int dim, const float *vertices, const float *texcoords, int count);
    void bindArrays();
    void unbindArrays();
    void reallocateBuffer(size_t size);
    GLvoid *mapNextFreeRange(size_t size);
    void reallocatePersistentBuffer(size_t size);
    bool awaitFence(intptr_t offset);
    GLvoid *getIdleRange(size_t size);

    GLuint buffer;
    GLenum usage;
    int stride;
    int vertexCount;
    static std::unique_ptr<GLVertexBuffer> streamingBuffer;
    static bool haveBufferStorage;
    static bool haveSyncFences;
    static bool hasMapBufferRange;
    static bool supportsIndexedQuads;
    QByteArray dataStore;
    bool persistent;
    bool useColor;
    QVector4D color;
    size_t bufferSize;
    intptr_t bufferEnd;
    size_t mappedSize;
    size_t frameSize;
    intptr_t nextOffset;
    intptr_t baseAddress;
    uint8_t *map;
    std::deque<BufferFence> fences;
    FrameSizesArray<4> frameSizes;
    VertexAttrib attrib[VertexAttributeCount];
    Bitfield enabledArrays;
    static std::unique_ptr<IndexBuffer> s_indexBuffer;
};

bool GLVertexBufferPrivate::hasMapBufferRange = false;
bool GLVertexBufferPrivate::supportsIndexedQuads = false;
std::unique_ptr<GLVertexBuffer> GLVertexBufferPrivate::streamingBuffer;
bool GLVertexBufferPrivate::haveBufferStorage = false;
bool GLVertexBufferPrivate::haveSyncFences = false;
std::unique_ptr<IndexBuffer> GLVertexBufferPrivate::s_indexBuffer;

void GLVertexBufferPrivate::interleaveArrays(float *dst, int dim,
                                             const float *vertices, const float *texcoords,
                                             int count)
{
    if (!texcoords) {
        memcpy((void *)dst, vertices, dim * sizeof(float) * count);
        return;
    }

    switch (dim) {
    case 2:
        for (int i = 0; i < count; i++) {
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
        break;

    case 3:
        for (int i = 0; i < count; i++) {
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(vertices++);
            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
        break;

    default:
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < dim; j++) {
                *(dst++) = *(vertices++);
            }

            *(dst++) = *(texcoords++);
            *(dst++) = *(texcoords++);
        }
    }
}

void GLVertexBufferPrivate::bindArrays()
{
    if (useColor) {
        GLShader *shader = ShaderManager::instance()->getBoundShader();
        shader->setUniform(GLShader::Color, color);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    BitfieldIterator it(enabledArrays);
    while (it.hasNext()) {
        const int index = it.next();
        glVertexAttribPointer(index, attrib[index].size, attrib[index].type, GL_FALSE, stride,
                              (const GLvoid *)(baseAddress + attrib[index].offset));
        glEnableVertexAttribArray(index);
    }
}

void GLVertexBufferPrivate::unbindArrays()
{
    BitfieldIterator it(enabledArrays);
    while (it.hasNext()) {
        glDisableVertexAttribArray(it.next());
    }
}

void GLVertexBufferPrivate::reallocatePersistentBuffer(size_t size)
{
    if (buffer != 0) {
        // This also unmaps and unbinds the buffer
        glDeleteBuffers(1, &buffer);
        buffer = 0;

        deleteAll(fences);
    }

    if (buffer == 0) {
        glGenBuffers(1, &buffer);
    }

    // Round the size up to 64 kb
    size_t minSize = std::max<size_t>(frameSizes.average() * 3, 128 * 1024);
    bufferSize = std::max(size, minSize);

    const GLbitfield storage = GL_DYNAMIC_STORAGE_BIT;
    const GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferStorage(GL_ARRAY_BUFFER, bufferSize, nullptr, storage | access);

    map = (uint8_t *)glMapBufferRange(GL_ARRAY_BUFFER, 0, bufferSize, access);

    nextOffset = 0;
    bufferEnd = bufferSize;
}

bool GLVertexBufferPrivate::awaitFence(intptr_t end)
{
    // Skip fences until we reach the end offset
    while (!fences.empty() && fences.front().nextEnd < end) {
        glDeleteSync(fences.front().sync);
        fences.pop_front();
    }

    Q_ASSERT(!fences.empty());

    // Wait on the next fence
    const BufferFence &fence = fences.front();

    if (!fence.signaled()) {
        qCDebug(LIBKWINGLUTILS) << "Stalling on VBO fence";
        const GLenum ret = glClientWaitSync(fence.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);

        if (ret == GL_TIMEOUT_EXPIRED || ret == GL_WAIT_FAILED) {
            qCCritical(LIBKWINGLUTILS) << "Wait failed";
            return false;
        }
    }

    glDeleteSync(fence.sync);

    // Update the end pointer
    bufferEnd = fence.nextEnd;
    fences.pop_front();

    return true;
}

GLvoid *GLVertexBufferPrivate::getIdleRange(size_t size)
{
    if (unlikely(size > bufferSize)) {
        reallocatePersistentBuffer(size * 2);
    }

    // Handle wrap-around
    if (unlikely(nextOffset + size > bufferSize)) {
        nextOffset = 0;
        bufferEnd -= bufferSize;

        for (BufferFence &fence : fences) {
            fence.nextEnd -= bufferSize;
        }

        // Emit a fence now
        if (auto sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)) {
            fences.push_back(BufferFence{
                .sync = sync,
                .nextEnd = intptr_t(bufferSize)});
        }
    }

    if (unlikely(nextOffset + intptr_t(size) > bufferEnd)) {
        if (!awaitFence(nextOffset + size)) {
            return nullptr;
        }
    }

    return map + nextOffset;
}

void GLVertexBufferPrivate::reallocateBuffer(size_t size)
{
    // Round the size up to 4 Kb for streaming/dynamic buffers.
    const size_t minSize = 32768; // Minimum size for streaming buffers
    const size_t alloc = usage != GL_STATIC_DRAW ? std::max(size, minSize) : size;

    glBufferData(GL_ARRAY_BUFFER, alloc, nullptr, usage);

    bufferSize = alloc;
}

GLvoid *GLVertexBufferPrivate::mapNextFreeRange(size_t size)
{
    GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;

    if ((nextOffset + size) > bufferSize) {
        // Reallocate the data store if it's too small.
        if (size > bufferSize) {
            reallocateBuffer(size);
        } else {
            access |= GL_MAP_INVALIDATE_BUFFER_BIT;
            access ^= GL_MAP_UNSYNCHRONIZED_BIT;
        }

        nextOffset = 0;
    }

    return glMapBufferRange(GL_ARRAY_BUFFER, nextOffset, size, access);
}

//*********************************
// GLVertexBuffer
//*********************************

const GLVertexAttrib GLVertexBuffer::GLVertex2DLayout[2] = {
    {VA_Position, 2, GL_FLOAT, offsetof(GLVertex2D, position)},
    {VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex2D, texcoord)},
};

const GLVertexAttrib GLVertexBuffer::GLVertex3DLayout[2] = {
    {VA_Position, 3, GL_FLOAT, offsetof(GLVertex3D, position)},
    {VA_TexCoord, 2, GL_FLOAT, offsetof(GLVertex3D, texcoord)},
};

GLVertexBuffer::GLVertexBuffer(UsageHint hint)
    : d(std::make_unique<GLVertexBufferPrivate>(hint))
{
}

GLVertexBuffer::~GLVertexBuffer() = default;

void GLVertexBuffer::setData(const void *data, size_t size)
{
    GLvoid *ptr = map(size);
    memcpy(ptr, data, size);
    unmap();
}

void GLVertexBuffer::setData(int vertexCount, int dim, const float *vertices, const float *texcoords)
{
    const GLVertexAttrib layout[] = {
        {VA_Position, dim, GL_FLOAT, 0},
        {VA_TexCoord, 2, GL_FLOAT, int(dim * sizeof(float))}};

    int stride = (texcoords ? dim + 2 : dim) * sizeof(float);
    int attribCount = texcoords ? 2 : 1;

    setAttribLayout(layout, attribCount, stride);
    setVertexCount(vertexCount);

    GLvoid *ptr = map(vertexCount * stride);
    d->interleaveArrays((float *)ptr, dim, vertices, texcoords, vertexCount);
    unmap();
}

GLvoid *GLVertexBuffer::map(size_t size)
{
    d->mappedSize = size;
    d->frameSize += size;

    if (d->persistent) {
        return d->getIdleRange(size);
    }

    glBindBuffer(GL_ARRAY_BUFFER, d->buffer);

    bool preferBufferSubData = GLPlatform::instance()->preferBufferSubData();

    if (GLVertexBufferPrivate::hasMapBufferRange && !preferBufferSubData) {
        return (GLvoid *)d->mapNextFreeRange(size);
    }

    // If we can't map the buffer we allocate local memory to hold the
    // buffer data and return a pointer to it.  The data will be submitted
    // to the actual buffer object when the user calls unmap().
    if (size_t(d->dataStore.size()) < size) {
        d->dataStore.resize(size);
    }

    return (GLvoid *)d->dataStore.data();
}

void GLVertexBuffer::unmap()
{
    if (d->persistent) {
        d->baseAddress = d->nextOffset;
        d->nextOffset += align(d->mappedSize, 8);
        d->mappedSize = 0;
        return;
    }

    bool preferBufferSubData = GLPlatform::instance()->preferBufferSubData();

    if (GLVertexBufferPrivate::hasMapBufferRange && !preferBufferSubData) {
        glUnmapBuffer(GL_ARRAY_BUFFER);

        d->baseAddress = d->nextOffset;
        d->nextOffset += align(d->mappedSize, 8);
    } else {
        // Upload the data from local memory to the buffer object
        if (preferBufferSubData) {
            if ((d->nextOffset + d->mappedSize) > d->bufferSize) {
                d->reallocateBuffer(d->mappedSize);
                d->nextOffset = 0;
            }

            glBufferSubData(GL_ARRAY_BUFFER, d->nextOffset, d->mappedSize, d->dataStore.constData());

            d->baseAddress = d->nextOffset;
            d->nextOffset += align(d->mappedSize, 8);
        } else {
            glBufferData(GL_ARRAY_BUFFER, d->mappedSize, d->dataStore.data(), d->usage);
            d->baseAddress = 0;
        }

        // Free the local memory buffer if it's unlikely to be used again
        if (d->usage == GL_STATIC_DRAW) {
            d->dataStore = QByteArray();
        }
    }

    d->mappedSize = 0;
}

void GLVertexBuffer::setVertexCount(int count)
{
    d->vertexCount = count;
}

void GLVertexBuffer::setAttribLayout(const GLVertexAttrib *attribs, int count, int stride)
{
    // Start by disabling all arrays
    d->enabledArrays = 0;

    for (int i = 0; i < count; i++) {
        const int index = attribs[i].index;

        Q_ASSERT(index >= 0 && index < VertexAttributeCount);
        Q_ASSERT(!d->enabledArrays[index]);

        d->attrib[index].size = attribs[i].size;
        d->attrib[index].type = attribs[i].type;
        d->attrib[index].offset = attribs[i].relativeOffset;

        d->enabledArrays[index] = true;
    }

    d->stride = stride;
}

void GLVertexBuffer::render(GLenum primitiveMode)
{
    render(infiniteRegion(), primitiveMode, false);
}

void GLVertexBuffer::render(const QRegion &region, GLenum primitiveMode, bool hardwareClipping)
{
    d->bindArrays();
    draw(region, primitiveMode, 0, d->vertexCount, hardwareClipping);
    d->unbindArrays();
}

void GLVertexBuffer::bindArrays()
{
    d->bindArrays();
}

void GLVertexBuffer::unbindArrays()
{
    d->unbindArrays();
}

void GLVertexBuffer::draw(GLenum primitiveMode, int first, int count)
{
    draw(infiniteRegion(), primitiveMode, first, count, false);
}

void GLVertexBuffer::draw(const QRegion &region, GLenum primitiveMode, int first, int count, bool hardwareClipping)
{
    if (primitiveMode == GL_QUADS) {
        if (!GLVertexBufferPrivate::s_indexBuffer) {
            GLVertexBufferPrivate::s_indexBuffer = std::make_unique<IndexBuffer>();
        }

        GLVertexBufferPrivate::s_indexBuffer->bind();
        GLVertexBufferPrivate::s_indexBuffer->accommodate(count / 4);

        count = count * 6 / 4;

        if (!hardwareClipping) {
            glDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr, first);
        } else {
            // Clip using scissoring
            const GLFramebuffer *current = GLFramebuffer::currentFramebuffer();
            for (const QRect &r : region) {
                glScissor(r.x(), current->size().height() - (r.y() + r.height()), r.width(), r.height());
                glDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, nullptr, first);
            }
        }
        return;
    }

    if (!hardwareClipping) {
        glDrawArrays(primitiveMode, first, count);
    } else {
        // Clip using scissoring
        const GLFramebuffer *current = GLFramebuffer::currentFramebuffer();
        for (const QRect &r : region) {
            glScissor(r.x(), current->size().height() - (r.y() + r.height()), r.width(), r.height());
            glDrawArrays(primitiveMode, first, count);
        }
    }
}

bool GLVertexBuffer::supportsIndexedQuads()
{
    return GLVertexBufferPrivate::supportsIndexedQuads;
}

bool GLVertexBuffer::isUseColor() const
{
    return d->useColor;
}

void GLVertexBuffer::setUseColor(bool enable)
{
    d->useColor = enable;
}

void GLVertexBuffer::setColor(const QColor &color, bool enable)
{
    d->useColor = enable;
    d->color = QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

void GLVertexBuffer::reset()
{
    d->useColor = false;
    d->color = QVector4D(0, 0, 0, 1);
    d->vertexCount = 0;
}

void GLVertexBuffer::endOfFrame()
{
    if (!d->persistent) {
        return;
    }

    // Emit a fence if we have uploaded data
    if (d->frameSize > 0) {
        d->frameSizes.push(d->frameSize);
        d->frameSize = 0;

        // Force the buffer to be reallocated at the beginning of the next frame
        // if the average frame size is greater than half the size of the buffer
        if (unlikely(d->frameSizes.average() > d->bufferSize / 2)) {
            deleteAll(d->fences);
            glDeleteBuffers(1, &d->buffer);

            d->buffer = 0;
            d->bufferSize = 0;
            d->nextOffset = 0;
            d->map = nullptr;
        } else {
            if (auto sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)) {
                d->fences.push_back(BufferFence{
                    .sync = sync,
                    .nextEnd = intptr_t(d->nextOffset + d->bufferSize)});
            }
        }
    }
}

void GLVertexBuffer::beginFrame()
{
    if (!d->persistent) {
        return;
    }

    // Remove finished fences from the list and update the bufferEnd offset
    while (d->fences.size() > 1 && d->fences.front().signaled()) {
        const BufferFence &fence = d->fences.front();
        glDeleteSync(fence.sync);

        d->bufferEnd = fence.nextEnd;
        d->fences.pop_front();
    }
}

void GLVertexBuffer::initStatic()
{
    if (GLPlatform::instance()->isGLES()) {
        bool haveBaseVertex = hasGLExtension(QByteArrayLiteral("GL_OES_draw_elements_base_vertex"));
        bool haveCopyBuffer = hasGLVersion(3, 0);
        bool haveMapBufferRange = hasGLExtension(QByteArrayLiteral("GL_EXT_map_buffer_range"));

        GLVertexBufferPrivate::hasMapBufferRange = haveMapBufferRange;
        GLVertexBufferPrivate::supportsIndexedQuads = haveBaseVertex && haveCopyBuffer && haveMapBufferRange;
        GLVertexBufferPrivate::haveBufferStorage = hasGLExtension("GL_EXT_buffer_storage");
        GLVertexBufferPrivate::haveSyncFences = hasGLVersion(3, 0);
    } else {
        bool haveBaseVertex = hasGLVersion(3, 2) || hasGLExtension(QByteArrayLiteral("GL_ARB_draw_elements_base_vertex"));
        bool haveCopyBuffer = hasGLVersion(3, 1) || hasGLExtension(QByteArrayLiteral("GL_ARB_copy_buffer"));
        bool haveMapBufferRange = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_map_buffer_range"));

        GLVertexBufferPrivate::hasMapBufferRange = haveMapBufferRange;
        GLVertexBufferPrivate::supportsIndexedQuads = haveBaseVertex && haveCopyBuffer && haveMapBufferRange;
        GLVertexBufferPrivate::haveBufferStorage = hasGLVersion(4, 4) || hasGLExtension("GL_ARB_buffer_storage");
        GLVertexBufferPrivate::haveSyncFences = hasGLVersion(3, 2) || hasGLExtension("GL_ARB_sync");
    }
    GLVertexBufferPrivate::s_indexBuffer.reset();
    GLVertexBufferPrivate::streamingBuffer = std::make_unique<GLVertexBuffer>(GLVertexBuffer::Stream);

    if (GLVertexBufferPrivate::haveBufferStorage && GLVertexBufferPrivate::haveSyncFences) {
        if (qgetenv("KWIN_PERSISTENT_VBO") != QByteArrayLiteral("0")) {
            GLVertexBufferPrivate::streamingBuffer->d->persistent = true;
        }
    }
}

void GLVertexBuffer::cleanup()
{
    GLVertexBufferPrivate::s_indexBuffer.reset();
    GLVertexBufferPrivate::hasMapBufferRange = false;
    GLVertexBufferPrivate::supportsIndexedQuads = false;
    GLVertexBufferPrivate::streamingBuffer.reset();
}

GLVertexBuffer *GLVertexBuffer::streamingBuffer()
{
    return GLVertexBufferPrivate::streamingBuffer.get();
}

} // namespace
