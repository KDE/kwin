/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "contrastshader.h"

#include <kwineffects.h>
#include <kwinglplatform.h>

#include <QByteArray>
#include <QMatrix4x4>
#include <QTextStream>
#include <QVector2D>

#include <cmath>

namespace KWin
{

ContrastShader::ContrastShader()
    : mValid(false), shader(nullptr), m_opacity(1)
{
}

ContrastShader::~ContrastShader()
{
    reset();
}

ContrastShader *ContrastShader::create()
{
    return new ContrastShader();
}

void ContrastShader::reset()
{
    delete shader;
    shader = nullptr;

    setIsValid(false);
}

void ContrastShader::setOpacity(float opacity)
{
    m_opacity = opacity;

    ShaderManager::instance()->pushShader(shader);
    shader->setUniform(opacityLocation, opacity);
    ShaderManager::instance()->popShader();
}

float ContrastShader::opacity() const
{
    return m_opacity;
}

void ContrastShader::setColorMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid())
        return;

    ShaderManager::instance()->pushShader(shader);
    shader->setUniform(colorMatrixLocation, matrix);
    ShaderManager::instance()->popShader();
}

void ContrastShader::setTextureMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid())
        return;

    shader->setUniform(textureMatrixLocation, matrix);
}

void ContrastShader::setModelViewProjectionMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid())
        return;

    shader->setUniform(mvpMatrixLocation, matrix);
}

void ContrastShader::bind()
{
    if (!isValid())
        return;

    ShaderManager::instance()->pushShader(shader);
}

void ContrastShader::unbind()
{
    ShaderManager::instance()->popShader();
}

void ContrastShader::init()
{
    reset();

    const bool gles = GLPlatform::instance()->isGLES();
    const bool glsl_140 = !gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(1, 40);
    const bool core = glsl_140 || (gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0));

    QByteArray vertexSource;
    QByteArray fragmentSource;

    const QByteArray attribute   = core ? "in"                : "attribute";
    const QByteArray varying_in  = core ? (gles ? "in" : "noperspective in") : "varying";
    const QByteArray varying_out = core ? (gles ? "out" : "noperspective out") : "varying";
    const QByteArray texture2D   = core ? "texture"           : "texture2D";
    const QByteArray fragColor   = core ? "fragColor"         : "gl_FragColor";

    // Vertex shader
    // ===================================================================
    QTextStream stream(&vertexSource);

    if (gles) {
        if (core) {
            stream << "#version 300 es\n\n";
        }
        stream << "precision highp float;\n";
    } else if (glsl_140) {
        stream << "#version 140\n\n";
    }

    stream << "uniform mat4 modelViewProjectionMatrix;\n";
    stream << "uniform mat4 textureMatrix;\n";
    stream << attribute << " vec4 vertex;\n\n";
    stream << varying_out << " vec4 varyingTexCoords;\n";
    stream << "\n";
    stream << "void main(void)\n";
    stream << "{\n";
    stream << "    varyingTexCoords = vec4(textureMatrix * vertex).stst;\n";
    stream << "    gl_Position = modelViewProjectionMatrix * vertex;\n";
    stream << "}\n";
    stream.flush();


    // Fragment shader
    // ===================================================================
    QTextStream stream2(&fragmentSource);

    if (gles) {
        if (core) {
            stream2 << "#version 300 es\n\n";
        }
        stream2 << "precision highp float;\n";
    } else if (glsl_140) {
        stream2 << "#version 140\n\n";
    }

    stream2 << "uniform mat4 colorMatrix;\n";
    stream2 << "uniform sampler2D sampler;\n";
    stream2 << "uniform float opacity;\n";
    stream2 << varying_in << " vec4 varyingTexCoords;\n";

    if (core)
        stream2 << "out vec4 fragColor;\n\n";

    stream2 << "void main(void)\n";
    stream2 << "{\n";
    stream2 << "    vec4 tex = " << texture2D << "(sampler, varyingTexCoords.st);\n";

    stream2 << "    if (opacity >= 1.0) {\n";
    stream2 << "        " << fragColor << " = tex * colorMatrix;\n";
    stream2 << "    } else {\n";
    stream2 << "        " << fragColor << " = tex * (opacity * colorMatrix + (1.0 - opacity) * mat4(1.0));\n";
    stream2 << "    }\n";

    stream2 << "}\n";
    stream2.flush();

    shader = ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentSource);

    if (shader->isValid()) {
        colorMatrixLocation = shader->uniformLocation("colorMatrix");
        textureMatrixLocation = shader->uniformLocation("textureMatrix");
        mvpMatrixLocation     = shader->uniformLocation("modelViewProjectionMatrix");
        opacityLocation       = shader->uniformLocation("opacity");

        QMatrix4x4 modelViewProjection;
        const QSize screenSize = effects->virtualScreenSize();
        modelViewProjection.ortho(0, screenSize.width(), screenSize.height(), 0, 0, 65535);
        ShaderManager::instance()->pushShader(shader);
        shader->setUniform(colorMatrixLocation, QMatrix4x4());
        shader->setUniform(textureMatrixLocation, QMatrix4x4());
        shader->setUniform(mvpMatrixLocation, modelViewProjection);
        shader->setUniform(opacityLocation, (float)1.0);
        ShaderManager::instance()->popShader();
    }

    setIsValid(shader->isValid());
}

} // namespace KWin
