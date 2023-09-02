/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "libkwineffects/kwinglutils.h"

// need to call GLTexturePrivate::initStatic()
#include "kwingltexture_p.h"

#include "libkwineffects/kwineffects.h"
#include "libkwineffects/kwinglplatform.h"
#include "libkwineffects/rendertarget.h"
#include "libkwineffects/renderviewport.h"
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
#include <bitset>
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
    return GLPlatform::instance()->glVersion() >= Version(major, minor, release);
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
    if (GLPlatform::instance()->isGLES() && GLPlatform::instance()->glslVersion() < Version(3, 0)) {
        ba.append("precision highp float;\n");
    }
    ba.append(source);
    if (GLPlatform::instance()->isGLES() && GLPlatform::instance()->glslVersion() >= Version(3, 0)) {
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
    if (!(GLPlatform::instance()->supports(GLFeature::GLSL) &&
          // we lack shader branching for Texture2DRectangle everywhere - and it's probably not worth it
          GLPlatform::instance()->supports(GLFeature::TextureNPOT))) {
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
    mMatrixLocation[ColorimetryTransformation] = uniformLocation("colorimetryTransform");

    mVec2Location[Offset] = uniformLocation("offset");

    m_vec3Locations[Vec3Uniform::PrimaryBrightness] = uniformLocation("primaryBrightness");

    mVec4Location[ModulationConstant] = uniformLocation("modulation");

    mFloatLocation[Saturation] = uniformLocation("saturation");
    mFloatLocation[MaxHdrBrightness] = uniformLocation("maxHdrBrightness");

    mColorLocation[Color] = uniformLocation("geometryColor");

    mIntLocation[TextureWidth] = uniformLocation("textureWidth");
    mIntLocation[TextureHeight] = uniformLocation("textureHeight");
    mIntLocation[SourceNamedTransferFunction] = uniformLocation("sourceNamedTransferFunction");
    mIntLocation[DestinationNamedTransferFunction] = uniformLocation("destinationNamedTransferFunction");
    mIntLocation[SdrBrightness] = uniformLocation("sdrBrightness");

    mLocationsResolved = true;
}

int GLShader::uniformLocation(const char *name)
{
    const int location = glGetUniformLocation(mProgram, name);
    return location;
}

bool GLShader::setUniform(MatrixUniform uniform, const QMatrix3x3 &value)
{
    resolveLocations();
    return setUniform(mMatrixLocation[uniform], value);
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

bool GLShader::setUniform(Vec3Uniform uniform, const QVector3D &value)
{
    resolveLocations();
    return setUniform(m_vec3Locations[uniform], value);
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

bool GLShader::setUniform(int location, const QMatrix3x3 &value)
{
    if (location >= 0) {
        glUniformMatrix3fv(location, 1, GL_FALSE, value.constData());
    }
    return location >= 0;
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

bool GLShader::setColorspaceUniforms(const ColorDescription &src, const ColorDescription &dst)
{
    return setUniform(GLShader::MatrixUniform::ColorimetryTransformation, src.colorimetry().toOther(dst.colorimetry()))
        && setUniform(GLShader::IntUniform::SourceNamedTransferFunction, int(src.transferFunction()))
        && setUniform(GLShader::IntUniform::DestinationNamedTransferFunction, int(dst.transferFunction()))
        && setUniform(IntUniform::SdrBrightness, dst.sdrBrightness())
        && setUniform(FloatUniform::MaxHdrBrightness, dst.maxHdrBrightness());
}

bool GLShader::setColorspaceUniformsFromSRGB(const ColorDescription &dst)
{
    return setColorspaceUniforms(ColorDescription::sRGB, dst);
}

bool GLShader::setColorspaceUniformsToSRGB(const ColorDescription &src)
{
    return setUniform(GLShader::MatrixUniform::ColorimetryTransformation, src.colorimetry().toOther(ColorDescription::sRGB.colorimetry()))
        && setUniform(GLShader::IntUniform::SourceNamedTransferFunction, int(src.transferFunction()))
        && setUniform(GLShader::IntUniform::DestinationNamedTransferFunction, int(NamedTransferFunction::sRGB))
        && setUniform(IntUniform::SdrBrightness, src.sdrBrightness())
        && setUniform(FloatUniform::MaxHdrBrightness, src.sdrBrightness());
}

//****************************************
// ShaderManager
//****************************************
std::unique_ptr<ShaderManager> ShaderManager::s_shaderManager;

ShaderManager *ShaderManager::instance()
{
    if (!s_shaderManager) {
        s_shaderManager = std::make_unique<ShaderManager>();
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
        const bool glsl_140 = gl->glslVersion() >= Version(1, 40);

        attribute = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_140 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_140) {
            stream << "#version 140\n\n";
        }
    } else {
        const bool glsl_es_300 = gl->glslVersion() >= Version(3, 0);

        attribute = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_es_300 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }
    }

    stream << attribute << " vec4 position;\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture)) {
        stream << attribute << " vec4 texcoord;\n\n";
        stream << varying << " vec2 texcoord0;\n\n";
    } else {
        stream << "\n";
    }

    stream << "uniform mat4 modelViewProjectionMatrix;\n\n";

    stream << "void main()\n{\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture)) {
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
        const bool glsl_140 = gl->glslVersion() >= Version(1, 40);

        if (glsl_140) {
            stream << "#version 140\n\n";
        }

        varying = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_140 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_140 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    } else {
        const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= Version(3, 0);

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
        stream << varying << " vec2 texcoord0;\n";
    } else if (traits & ShaderTrait::MapExternalTexture) {
        stream << "#extension GL_OES_EGL_image_external : require\n\n";
        stream << "uniform samplerExternalOES sampler;\n";
        stream << varying << " vec2 texcoord0;\n";
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "uniform vec4 geometryColor;\n";
    }
    if (traits & ShaderTrait::Modulate) {
        stream << "uniform vec4 modulation;\n";
    }
    if (traits & ShaderTrait::AdjustSaturation) {
        stream << "uniform float saturation;\n";
        stream << "uniform vec3 primaryBrightness;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "const int sRGB_EOTF = 0;\n";
        stream << "const int linear_EOTF = 1;\n";
        stream << "const int PQ_EOTF = 2;\n";
        stream << "\n";
        stream << "uniform mat3 colorimetryTransform;\n";
        stream << "uniform int sourceNamedTransferFunction;\n";
        stream << "uniform int destinationNamedTransferFunction;\n";
        stream << "uniform int sdrBrightness;// in nits\n";
        stream << "uniform float maxHdrBrightness; // in nits\n";
        stream << "\n";
        stream << "vec3 nitsToPq(vec3 nits) {\n";
        stream << "    vec3 normalized = clamp(nits / 10000.0, vec3(0), vec3(1));\n";
        stream << "    float c1 = 0.8359375;\n";
        stream << "    float c2 = 18.8515625;\n";
        stream << "    float c3 = 18.6875;\n";
        stream << "    float m1 = 0.1593017578125;\n";
        stream << "    float m2 = 78.84375;\n";
        stream << "    vec3 num = vec3(c1) + c2 * pow(normalized, vec3(m1));\n";
        stream << "    vec3 denum = vec3(1.0) + c3 * pow(normalized, vec3(m1));\n";
        stream << "    return pow(num / denum, vec3(m2));\n";
        stream << "}\n";
        stream << "vec3 srgbToLinear(vec3 color) {\n";
        stream << "    bvec3 isLow = lessThanEqual(color, vec3(0.04045f));\n";
        stream << "    vec3 loPart = color / 12.92f;\n";
        stream << "    vec3 hiPart = pow((color + 0.055f) / 1.055f, vec3(12.0f / 5.0f));\n";
        stream << "    return mix(hiPart, loPart, isLow);\n";
        stream << "}\n";
        stream << "\n";
        stream << "vec3 linearToSrgb(vec3 color) {\n";
        stream << "    bvec3 isLow = lessThanEqual(color, vec3(0.0031308f));\n";
        stream << "    vec3 loPart = color * 12.92f;\n";
        stream << "    vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;\n";
        stream << "    return mix(hiPart, loPart, isLow);\n";
        stream << "}\n";
        stream << "\n";
        stream << "vec3 doTonemapping(vec3 color, float maxBrightness) {\n";
        stream << "    // colorimetric 'tonemapping': just clip to the output color space\n";
        stream << "    return clamp(color, vec3(0.0), vec3(maxBrightness));\n";
        stream << "}\n";
        stream << "\n";
    }

    if (output != QByteArrayLiteral("gl_FragColor")) {
        stream << "\nout vec4 " << output << ";\n";
    }

    stream << "\nvoid main(void)\n{\n";
    stream << "    vec4 result;\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << "    result = " << textureLookup << "(sampler, texcoord0);\n";
    } else if (traits & ShaderTrait::MapExternalTexture) {
        // external textures require texture2D for sampling
        stream << "    result = texture2D(sampler, texcoord0);\n";
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "    result = geometryColor;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    if (sourceNamedTransferFunction == sRGB_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = sdrBrightness * srgbToLinear(result.rgb);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    }\n";
        stream << "    result.rgb = doTonemapping(colorimetryTransform * result.rgb, maxHdrBrightness);\n";
    }
    if (traits & ShaderTrait::AdjustSaturation) {
        // this calculates the Y component of the XYZ color representation for the color,
        // which roughly corresponds to the brightness of the RGB tuple
        stream << "    float Y = dot(result.rgb, primaryBrightness);\n";
        stream << "    result.rgb = mix(vec3(Y), result.rgb, saturation);\n";
    }
    if (traits & ShaderTrait::Modulate) {
        stream << "    result *= modulation;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    if (destinationNamedTransferFunction == sRGB_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = linearToSrgb(doTonemapping(result.rgb, sdrBrightness) / sdrBrightness);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    } else if (destinationNamedTransferFunction == PQ_EOTF) {\n";
        stream << "        result.rgb = nitsToPq(result.rgb);\n";
        stream << "    }\n";
    }

    stream << "    " << output << " = result;\n";
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

    const Version coreVersionNumber = GLPlatform::instance()->isGLES() ? Version(3, 0) : Version(1, 40);
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
bool GLFramebuffer::sSupportsPackedDepthStencil = false;
bool GLFramebuffer::sSupportsDepth24 = false;
bool GLFramebuffer::s_blitSupported = false;
QStack<GLFramebuffer *> GLFramebuffer::s_fbos = QStack<GLFramebuffer *>();

void GLFramebuffer::initStatic()
{
    if (GLPlatform::instance()->isGLES()) {
        sSupported = true;
        sSupportsPackedDepthStencil = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_OES_packed_depth_stencil"));
        sSupportsDepth24 = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_OES_depth24"));
        s_blitSupported = hasGLVersion(3, 0);
    } else {
        sSupported = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_framebuffer_object")) || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
        sSupportsPackedDepthStencil = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_framebuffer_object")) || hasGLExtension(QByteArrayLiteral("GL_EXT_packed_depth_stencil"));
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
    : m_colorAttachment(nullptr)
{
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

GLFramebuffer::GLFramebuffer(GLTexture *colorAttachment, Attachment attachment)
    : mSize(colorAttachment->size())
    , m_colorAttachment(colorAttachment)
{
    if (!sSupported) {
        qCCritical(LIBKWINGLUTILS) << "Framebuffer objects aren't supported!";
        return;
    }

    GLuint prevFbo = 0;
    if (const GLFramebuffer *current = currentFramebuffer()) {
        prevFbo = current->handle();
    }

    glGenFramebuffers(1, &mFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);

    initColorAttachment(colorAttachment);
    if (attachment == Attachment::CombinedDepthStencil) {
        initDepthStencilAttachment();
    }

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

GLFramebuffer::GLFramebuffer(GLuint handle, const QSize &size)
    : mFramebuffer(handle)
    , mSize(size)
    , mValid(true)
    , mForeign(true)
    , m_colorAttachment(nullptr)
{
}

GLFramebuffer::~GLFramebuffer()
{
    if (!mForeign && mValid) {
        glDeleteFramebuffers(1, &mFramebuffer);
    }
    if (mDepthBuffer) {
        glDeleteRenderbuffers(1, &mDepthBuffer);
    }
    if (mStencilBuffer && mStencilBuffer != mDepthBuffer) {
        glDeleteRenderbuffers(1, &mStencilBuffer);
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

void GLFramebuffer::initColorAttachment(GLTexture *colorAttachment)
{
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           colorAttachment->target(), colorAttachment->texture(), 0);
}

void GLFramebuffer::initDepthStencilAttachment()
{
    GLuint buffer = 0;

    // Try to attach a depth/stencil combined attachment.
    if (sSupportsPackedDepthStencil) {
        glGenRenderbuffers(1, &buffer);
        glBindRenderbuffer(GL_RENDERBUFFER, buffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mSize.width(), mSize.height());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glDeleteRenderbuffers(1, &buffer);
        } else {
            mDepthBuffer = buffer;
            mStencilBuffer = buffer;
            return;
        }
    }

    // Try to attach a depth attachment separately.
    GLenum depthFormat;
    if (GLPlatform::instance()->isGLES()) {
        if (sSupportsDepth24) {
            depthFormat = GL_DEPTH_COMPONENT24;
        } else {
            depthFormat = GL_DEPTH_COMPONENT16;
        }
    } else {
        depthFormat = GL_DEPTH_COMPONENT;
    }

    glGenRenderbuffers(1, &buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, depthFormat, mSize.width(), mSize.height());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteRenderbuffers(1, &buffer);
    } else {
        mDepthBuffer = buffer;
    }

    // Try to attach a stencil attachment separately.
    GLenum stencilFormat;
    if (GLPlatform::instance()->isGLES()) {
        stencilFormat = GL_STENCIL_INDEX8;
    } else {
        stencilFormat = GL_STENCIL_INDEX;
    }

    glGenRenderbuffers(1, &buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, stencilFormat, mSize.width(), mSize.height());
    glFramebufferRenderbuffer(GL_RENDERBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteRenderbuffers(1, &buffer);
    } else {
        mStencilBuffer = buffer;
    }
}

void GLFramebuffer::blitFromFramebuffer(const QRect &source, const QRect &destination, GLenum filter, bool flipX, bool flipY)
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

    GLuint srcX0 = s.x();
    GLuint srcY0 = top->size().height() - (s.y() + s.height());
    GLuint srcX1 = s.x() + s.width();
    GLuint srcY1 = top->size().height() - s.y();
    if (flipX) {
        std::swap(srcX0, srcX1);
    }
    if (flipY) {
        std::swap(srcY0, srcY1);
    }

    const GLuint dstX0 = d.x();
    const GLuint dstY0 = mSize.height() - (d.y() + d.height());
    const GLuint dstX1 = d.x() + d.width();
    const GLuint dstY1 = mSize.height() - d.y();

    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, filter);

    GLFramebuffer::popFramebuffer();
}

bool GLFramebuffer::blitFromRenderTarget(const RenderTarget &sourceRenderTarget, const RenderViewport &sourceViewport, const QRect &source, const QRect &destination)
{
    TextureTransforms transform = sourceRenderTarget.texture() ? sourceRenderTarget.texture()->contentTransforms() : TextureTransforms();
    const bool hasRotation = (transform & TextureTransform::Rotate90) || (transform & TextureTransform::Rotate180) || (transform & TextureTransform::Rotate270);
    if (!hasRotation && blitSupported()) {
        // either no transformation or flipping only
        blitFromFramebuffer(sourceViewport.mapToRenderTarget(source), destination, GL_LINEAR, transform & TextureTransform::MirrorX, transform & TextureTransform::MirrorY);
        return true;
    } else {
        const auto texture = sourceRenderTarget.texture();
        if (!texture) {
            // rotations aren't possible without a texture
            return false;
        }

        GLFramebuffer::pushFramebuffer(this);

        QMatrix4x4 mat;
        mat.ortho(QRectF(QPointF(), size()));
        // GLTexture::render renders with origin (0, 0), move it to the correct place
        mat.translate(destination.x(), destination.y());

        ShaderBinder binder(ShaderTrait::MapTexture);
        binder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, mat);

        texture->render(sourceViewport.mapToRenderTargetTexture(source), infiniteRegion(), destination.size(), 1);

        GLFramebuffer::popFramebuffer();
        return true;
    }
}

GLTexture *GLFramebuffer::colorAttachment() const
{
    return m_colorAttachment;
}

// ------------------------------------------------------------------

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

    void accommodate(size_t count);
    void bind();

private:
    GLuint m_buffer;
    size_t m_count = 0;
    std::vector<uint16_t> m_data;
};

IndexBuffer::IndexBuffer()
{
    // The maximum number of quads we can render with 16 bit indices is 16,384.
    // But we start with 512 and grow the buffer as needed.
    glGenBuffers(1, &m_buffer);
    accommodate(512);
}

IndexBuffer::~IndexBuffer()
{
    glDeleteBuffers(1, &m_buffer);
}

void IndexBuffer::accommodate(size_t count)
{
    // Check if we need to grow the buffer.
    if (count <= m_count) {
        return;
    }
    Q_ASSERT(m_count * 2 < std::numeric_limits<uint16_t>::max() / 4);
    const size_t oldCount = m_count;
    m_count *= 2;
    m_data.reserve(m_count * 6);
    for (size_t i = oldCount; i < m_count; i++) {
        const uint16_t offset = i * 4;
        m_data[i * 6 + 0] = offset + 1;
        m_data[i * 6 + 1] = offset + 0;
        m_data[i * 6 + 2] = offset + 3;
        m_data[i * 6 + 3] = offset + 3;
        m_data[i * 6 + 4] = offset + 2;
        m_data[i * 6 + 5] = offset + 1;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_count * sizeof(uint16_t), m_data.data(), GL_STATIC_DRAW);
}

void IndexBuffer::bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffer);
}

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
    std::bitset<32> enabledArrays;
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

    for (size_t i = 0; i < enabledArrays.size(); i++) {
        if (enabledArrays[i]) {
            glVertexAttribPointer(i, attrib[i].size, attrib[i].type, GL_FALSE, stride,
                                  (const GLvoid *)(baseAddress + attrib[i].offset));
            glEnableVertexAttribArray(i);
        }
    }
}

void GLVertexBufferPrivate::unbindArrays()
{
    for (size_t i = 0; i < enabledArrays.size(); i++) {
        if (enabledArrays[i]) {
            glDisableVertexAttribArray(i);
        }
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
    d->enabledArrays.reset();

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
