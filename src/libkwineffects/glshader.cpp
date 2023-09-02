/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glshader.h"
#include "kwinglplatform.h"
#include "kwinglutils.h"
#include "logging_p.h"

#include <QFile>

namespace KWin
{

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

}
