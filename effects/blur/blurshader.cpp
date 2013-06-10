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

#include "blurshader.h"

#include <kwineffects.h>
#include <kwinglplatform.h>

#include <QByteArray>
#include <QMatrix4x4>
#include <QTextStream>
#include <QVector2D>
#include <KDebug>

#include <cmath>

using namespace KWin;


BlurShader::BlurShader()
    : mRadius(0), mValid(false)
{
}

BlurShader::~BlurShader()
{
}

BlurShader *BlurShader::create()
{
    if (GLSLBlurShader::supported())
        return new GLSLBlurShader();

#ifdef KWIN_HAVE_OPENGL_1
    return new ARBBlurShader();
#else
    return NULL;
#endif
}

void BlurShader::setRadius(int radius)
{
    const int r = qMax(radius, 2);

    if (mRadius != r) {
        mRadius = r;
        reset();
        init();
    }
}

void BlurShader::setDirection(Qt::Orientation direction)
{
    mDirection = direction;
}

float BlurShader::gaussian(float x, float sigma) const
{
    return (1.0 / std::sqrt(2.0 * M_PI) * sigma)
           * std::exp(-((x * x) / (2.0 * sigma * sigma)));
}

QList<KernelValue> BlurShader::gaussianKernel() const
{
    int size = qMin(mRadius | 1, maxKernelSize());
    if (!(size & 0x1))
        size -= 1;

    QList<KernelValue> kernel;
    const int center = size / 2;
    const qreal sigma = (size - 1) / 2.5;

    kernel <<  KernelValue(0.0, gaussian(0.0, sigma));
    float total = kernel[0].g;

    for (int x = 1; x <= center; x++) {
        const float fx = (x - 1) * 2 + 1.5;
        const float g1 = gaussian(fx - 0.5, sigma);
        const float g2 = gaussian(fx + 0.5, sigma);

        // Offset taking the contribution of both pixels into account
        const float offset = .5 - g1 / (g1 + g2);

        kernel << KernelValue(fx + offset, g1 + g2);
        kernel << KernelValue(-(fx + offset), g1 + g2);

        total += (g1 + g2) * 2;
    }

    qSort(kernel);

    // Normalize the kernel
    for (int i = 0; i < kernel.count(); i++)
        kernel[i].g /= total;

    return kernel;
}



// ----------------------------------------------------------------------------



GLSLBlurShader::GLSLBlurShader()
    : BlurShader(), shader(NULL)
{
}

GLSLBlurShader::~GLSLBlurShader()
{
    reset();
}

void GLSLBlurShader::reset()
{
    delete shader;
    shader = NULL;

    setIsValid(false);
}

bool GLSLBlurShader::supported()
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

void GLSLBlurShader::setPixelDistance(float val)
{
    if (!isValid())
        return;

    QVector2D pixelSize(0.0, 0.0);
    if (direction() == Qt::Horizontal)
        pixelSize.setX(val);
    else
        pixelSize.setY(val);

    shader->setUniform(pixelSizeLocation, pixelSize);
}

void GLSLBlurShader::setTextureMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid())
        return;

    shader->setUniform(textureMatrixLocation, matrix);
}

void GLSLBlurShader::setModelViewProjectionMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid())
        return;

    shader->setUniform(mvpMatrixLocation, matrix);
}

void GLSLBlurShader::bind()
{
    if (!isValid())
        return;

    ShaderManager::instance()->pushShader(shader);
}

void GLSLBlurShader::unbind()
{
    ShaderManager::instance()->popShader();
}

int GLSLBlurShader::maxKernelSize() const
{
#ifdef KWIN_HAVE_OPENGLES
    // GL_MAX_VARYING_FLOATS not available in GLES
    // querying for GL_MAX_VARYING_VECTORS crashes on nouveau
    // using the minimum value of 8
    return 8 * 2;
#else
    int value;
    glGetIntegerv(GL_MAX_VARYING_FLOATS, &value);
    // Maximum number of vec4 varyings * 2
    // The code generator will pack two vec2's into each vec4.
    return value / 2;
#endif
}

void GLSLBlurShader::init()
{
    QList<KernelValue> kernel = gaussianKernel();
    const int size = kernel.size();
    const int center = size / 2;

    QList<QVector4D> offsets;
    for (int i = 0; i < kernel.size(); i += 2) {
        QVector4D vec4(0, 0, 0, 0);

        vec4.setX(kernel[i].x);
        vec4.setY(kernel[i].x);

        if (i < kernel.size() - 1) {
            vec4.setZ(kernel[i + 1].x);
            vec4.setW(kernel[i + 1].x);
        }

        offsets << vec4;
    }

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
    stream << "uniform vec2 pixelSize;\n\n";
    stream << attribute << " vec4 vertex;\n\n";
    stream << varying_out << " vec4 samplePos[" << std::ceil(size / 2.0) << "];\n";
    stream << "\n";
    stream << "void main(void)\n";
    stream << "{\n";
    stream << "    vec4 center = vec4(textureMatrix * vertex).stst;\n";
    stream << "    vec4 ps = pixelSize.stst;\n\n";
    for (int i = 0; i < offsets.size(); i++) {
        stream << "    samplePos[" << i << "] = center + ps * vec4("
               << offsets[i].x() << ", " << offsets[i].y() << ", "
               << offsets[i].z() << ", " << offsets[i].w() << ");\n";
    }
    stream << "\n";
    stream << "    gl_Position = modelViewProjectionMatrix * vertex;\n";
    stream << "}\n";
    stream.flush();

    // Fragment shader
    // ===================================================================
    QTextStream stream2(&fragmentSource);

    if (glsl_140)
        stream2 << "#version 140\n\n";

    stream2 << "uniform sampler2D texUnit;\n";
    stream2 << varying_in << " vec4 samplePos[" << std::ceil(size / 2.0) << "];\n\n";

    for (int i = 0; i <= center; i++)
        stream2 << "const float kernel" << i << " = " << kernel[i].g << ";\n";
    stream2 << "\n";

    if (glsl_140)
        stream2 << "out vec4 fragColor;\n\n";

    stream2 << "void main(void)\n";
    stream2 << "{\n";
    stream2 << "    vec4 sum = " << texture2D << "(texUnit, samplePos[0].st) * kernel0;\n";
    for (int i = 1, j = -center + 1; i < size; i++, j++)
        stream2 << "    sum = sum + " << texture2D << "(texUnit, samplePos[" << i / 2
                << ((i % 2) ? "].pq)" : "].st)") << " * kernel" << center - qAbs(j) << ";\n";
    stream2 << "    " << fragColor << " = sum;\n";
    stream2 << "}\n";
    stream2.flush();

    shader = ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentSource);
    if (shader->isValid()) {
        pixelSizeLocation     = shader->uniformLocation("pixelSize");
        textureMatrixLocation = shader->uniformLocation("textureMatrix");
        mvpMatrixLocation     = shader->uniformLocation("modelViewProjectionMatrix");

        QMatrix4x4 modelViewProjection;
        modelViewProjection.ortho(0, displayWidth(), displayHeight(), 0, 0, 65535);
        ShaderManager::instance()->pushShader(shader);
        shader->setUniform(textureMatrixLocation, QMatrix4x4());
        shader->setUniform(mvpMatrixLocation, modelViewProjection);
        ShaderManager::instance()->popShader();
    }

    setIsValid(shader->isValid());
}



// ----------------------------------------------------------------------------


#ifdef KWIN_HAVE_OPENGL_1
ARBBlurShader::ARBBlurShader()
    : BlurShader(), program(0)
{
}

ARBBlurShader::~ARBBlurShader()
{
    reset();
}

void ARBBlurShader::reset()
{
    if (program) {
        glDeleteProgramsARB(1, &program);
        program = 0;
    }

    setIsValid(false);
}

bool ARBBlurShader::supported()
{
    if (!hasGLExtension("GL_ARB_fragment_program"))
        return false;

    (void) glGetError(); // Clear the error state

    // These are the minimum values the implementation is required to support
    int value = 0;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_PARAMETERS_ARB, &value);
    if (value < 24)
        return false;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_TEMPORARIES_ARB, &value);
    if (value < 16)
        return false;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &value);
    if (value < 72)
        return false;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB, &value);
    if (value < 24)
        return false;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB, &value);
    if (value < 4)
        return false;

    if (glGetError() != GL_NO_ERROR)
        return false;

    return true;
}

void ARBBlurShader::setPixelDistance(float val)
{
    float firstStep = val * 1.5;
    float nextStep = val * 2.0;

    if (direction() == Qt::Horizontal) {
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, firstStep, 0, 0, 0);
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, nextStep, 0, 0, 0);
    } else {
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 0, firstStep, 0, 0);
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, 0, nextStep, 0, 0);
    }
}

void ARBBlurShader::bind()
{
    if (!isValid())
        return;

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
}

void ARBBlurShader::unbind()
{
    int boundObject;
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_BINDING_ARB, &boundObject);
    if (boundObject == (int)program) {
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    }
}

int ARBBlurShader::maxKernelSize() const
{
    int value;
    int result;

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_PARAMETERS_ARB, &value);
    result = (value - 1) * 2; // We only need to store half the kernel, since it's symmetrical

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_INSTRUCTIONS_ARB, &value);
    result = qMin(result, value / 3); // We need 3 instructions / sample

    return result;
}

void ARBBlurShader::init()
{
    QList<KernelValue> kernel = gaussianKernel();
    const int size = kernel.size();
    const int center = size / 2;

    QByteArray text;
    QTextStream stream(&text);

    stream << "!!ARBfp1.0\n";

    // The kernel values are hardcoded into the program
    for (int i = 0; i <= center; i++)
        stream << "PARAM kernel" << i << " = " << kernel[center + i].g << ";\n";

    stream << "PARAM firstSample = program.local[0];\n"; // Distance from gl_TexCoord[0] to the next sample
    stream << "PARAM nextSample  = program.local[1];\n"; // Distance to the subsequent sample

    // Temporary variables to hold coordinates and texture samples
    for (int i = 0; i < size; i++)
        stream << "TEMP temp" << i << ";\n";

    // Compute the texture coordinates
    stream << "ADD temp1, fragment.texcoord[0], firstSample;\n"; // temp1 = gl_TexCoord[0] + firstSample
    stream << "SUB temp2, fragment.texcoord[0], firstSample;\n"; // temp2 = gl_TexCoord[0] - firstSample
    for (int i = 1, j = 3; i < center; i++, j += 2) {
        stream << "ADD temp" << j + 0 << ", temp" << j - 2 << ", nextSample;\n";
        stream << "SUB temp" << j + 1 << ", temp" << j - 1 << ", nextSample;\n";
    }

    // Sample the texture coordinates
    stream << "TEX temp0, fragment.texcoord[0], texture[0], 2D;\n";
    for (int i = 1; i < size; i++)
        stream << "TEX temp" << i << ", temp" << i << ", texture[0], 2D;\n";

    // Multiply the samples with the kernel values and compute the sum
    stream << "MUL temp0, temp0, kernel0;\n";
    for (int i = 0, j = 1; i < center; i++) {
        stream << "MAD temp0, temp" << j++ << ", kernel" << i + 1 << ", temp0;\n";
        stream << "MAD temp0, temp" << j++ << ", kernel" << i + 1 << ", temp0;\n";
    }

    stream << "MOV result.color, temp0;\n";  // gl_FragColor = temp0
    stream << "END\n";
    stream.flush();

    glGenProgramsARB(1, &program);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, text.length(), text.constData());

    if (glGetError()) {
        const char *error = (const char*)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
        kError() << "Failed to compile fragment program:" << error;
        setIsValid(false);
    } else
        setIsValid(true);

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
}
#endif

