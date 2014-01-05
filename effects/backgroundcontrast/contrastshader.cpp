/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  if not, write to
 *   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *   Boston, MA 02110-1301, USA.
 */

#include "contrastshader.h"

#include <kwineffects.h>
#include <kwinglplatform.h>

#include <QByteArray>
#include <QMatrix4x4>
#include <QTextStream>
#include <QVector2D>
#include <KDebug>

#include <cmath>

using namespace KWin;


ContrastShader::ContrastShader()
    : shader(NULL), mValid(false)
{
}

ContrastShader::~ContrastShader()
{
    reset();
}

ContrastShader *ContrastShader::create()
{
    if (ContrastShader::supported()) {
        ContrastShader *s = new ContrastShader();
        s->reset();
        s->init();
        return s;
    }

    return NULL;
}

void ContrastShader::reset()
{
    delete shader;
    shader = NULL;

    setIsValid(false);
}

bool ContrastShader::supported()
{
    if (!GLPlatform::instance()->supports(GLSL))
        return false;
    if (effects->compositingType() == OpenGL1Compositing)
        return false;

    (void) glGetError(); // Clear the error state

#ifdef KWIN_HAVE_OPENGL_1
    // These are the minimum values the implementation is required to support
    int value = 0;

    glGetIntegerv(GL_MAX_VARYING_FLOATS, &value);
    if (value < 32)
        return false;

    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &value);
    if (value < 64)
        return false;

    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
    if (value < 512)
        return false;
#endif

    if (glGetError() != GL_NO_ERROR)
        return false;

    return true;
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


#ifdef KWIN_HAVE_OPENGLES
    const bool glsl_140 = false;
#else
    const bool glsl_140 = GLPlatform::instance()->glslVersion() >= kVersionNumber(1, 40);
#endif

    QByteArray vertexSource;
    QByteArray fragmentSource;

    const QByteArray attribute   = glsl_140 ? "in"                : "attribute";
    const QByteArray varying_in  = glsl_140 ? "noperspective in"  : "varying";
    const QByteArray varying_out = glsl_140 ? "noperspective out" : "varying";
    const QByteArray texture2D   = glsl_140 ? "texture"           : "texture2D";
    const QByteArray fragColor   = glsl_140 ? "fragColor"         : "gl_FragColor";

    // Vertex shader
    // ===================================================================
    QTextStream stream(&vertexSource);

   if (glsl_140)
        stream << "#version 140\n\n";

    stream << "uniform mat4 modelViewProjectionMatrix;\n";
    stream << "uniform mat4 textureMatrix;\n";
    stream << attribute << " vec4 vertex;\n\n";
    stream << "out vec4 varyingTexCoords;\n";
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

    if (glsl_140)
        stream2 << "#version 140\n\n";

    stream2 << "uniform mat4 colorMatrix;\n";
    stream2 << "uniform sampler2D sampler;\n";
    stream2 << "in vec4 varyingTexCoords;\n";

    if (glsl_140)
        stream2 << "out vec4 fragColor;\n\n";

    stream2 << "void main(void)\n";
    stream2 << "{\n";
    stream2 << "    vec4 tex = " << texture2D << "(sampler, varyingTexCoords.st);\n";

    QByteArray colorMatrix = "mat4(1.74631,  -0.448073, -0.0452333, 0.2685,\
                               -0.133194,  1.43143,  -0.0452333, 0.2685,\
                               -0.133194, -0.448073,  1.83427,   0.2685,\
                                0, 0, 0, 1);\n";

    
    stream2 << "    "  << fragColor << " = tex * colorMatrix;\n";// << colorMatrix;
    
    stream2 << "}\n";
    stream2.flush();

    shader = ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentSource);

    if (shader->isValid()) {
        colorMatrixLocation = shader->uniformLocation("colorMatrix");
        textureMatrixLocation = shader->uniformLocation("textureMatrix");
        mvpMatrixLocation     = shader->uniformLocation("modelViewProjectionMatrix");

        QMatrix4x4 modelViewProjection;
        modelViewProjection.ortho(0, displayWidth(), displayHeight(), 0, 0, 65535);
        ShaderManager::instance()->pushShader(shader);
        shader->setUniform(colorMatrixLocation, QMatrix4x4());
        shader->setUniform(textureMatrixLocation, QMatrix4x4());
        shader->setUniform(mvpMatrixLocation, modelViewProjection);
        ShaderManager::instance()->popShader();
    }

    setIsValid(shader->isValid());
}

