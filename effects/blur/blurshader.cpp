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

#include <QByteArray>
#include <QTextStream>
#include <KDebug>

#include <cmath>

using namespace KWin;


BlurShader::BlurShader()
    : mRadius(12)
{
}

BlurShader::~BlurShader()
{
}

BlurShader *BlurShader::create()
{
    if (GLShader::vertexShaderSupported() && GLShader::fragmentShaderSupported())
        return new GLSLBlurShader();

    return new ARBBlurShader();
}

void BlurShader::setRadius(int radius)
{
    const int r = qMax(radius, 2);

    if (mRadius != r) {
        mRadius = r;
        reset();
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

QVector<float> BlurShader::gaussianKernel() const
{
    int size = qMin(mRadius | 1, maxKernelSize());
    if (!(size & 0x1))
        size -= 1;

    QVector<float> kernel(size);
    const int center = size / 2;
    const qreal sigma = (size - 1) / 2.5;

    // Generate the gaussian kernel
    kernel[center] = gaussian(0, sigma) * .5;
    for (int i = 1; i <= center; i++) {
        const float val = gaussian(1.5 + (i - 1) * 2.0, sigma);
        kernel[center + i] = val;
        kernel[center - i] = val;
    }

    // Normalize the kernel
    qreal total = 0;
    for (int i = 0; i < size; i++)
        total += kernel[i];

    for (int i = 0; i < size; i++)
        kernel[i] /= total;

    return kernel;
}



// ----------------------------------------------------------------------------



GLSLBlurShader::GLSLBlurShader()
    : BlurShader(), program(0)
{
}

GLSLBlurShader::~GLSLBlurShader()
{
    reset();
}

void GLSLBlurShader::reset()
{
    if (program) {
        glDeleteProgram(program);
        program = 0;
    }
}

void GLSLBlurShader::setPixelDistance(float val)
{
    float pixelSize[2] = { 0.0, 0.0 };

    if (direction() == Qt::Horizontal)
        pixelSize[0] = val;
    else
        pixelSize[1] = val;

    glUniform2fv(uPixelSize, 1, pixelSize);
}

void GLSLBlurShader::setOpacity(float val)
{
    const float opacity[4] = { val, val, val, val };
    glUniform4fv(uOpacity, 1, opacity);
}

void GLSLBlurShader::bind()
{
    if (!program)
        init();

    glUseProgram(program);
    glUniform1i(uTexUnit, 0);
}

void GLSLBlurShader::unbind()
{
    glUseProgram(0);
}

int GLSLBlurShader::maxKernelSize() const
{
    int value;
    glGetIntegerv(GL_MAX_VARYING_FLOATS, &value);
    // Note: In theory the driver could pack two vec2's in one vec4,
    //       but we'll assume it doesn't do that
    return value / 4; // Max number of vec4 varyings
}

GLuint GLSLBlurShader::compile(GLenum type, const QByteArray &source)
{
    const char *sourceData = source.constData();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &sourceData, 0);
    glCompileShader(shader);

    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLsizei size, length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);

        QByteArray log(size, 0);
        glGetShaderInfoLog(shader, size, &length, log.data());

        kError() << "Failed to compile shader: " << log;
        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

GLuint GLSLBlurShader::link(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_FALSE) {
        GLsizei size, length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &size);

        QByteArray log(size, 0);
        glGetProgramInfoLog(program, size, &length, log.data());

        kError() << "Failed to link shader: " << log;
        glDeleteProgram(program);
        program = 0;
    }

    return program;
}

void GLSLBlurShader::init()
{
    QVector<float> kernel = gaussianKernel();
    const int size = kernel.size();
    const int center = size / 2;

    QByteArray vertexSource;
    QByteArray fragmentSource;

    // Vertex shader
    // ===================================================================
    QTextStream stream(&vertexSource);

    stream << "uniform vec2 pixelSize;\n\n";
    for (int i = 0; i < size; i++)
        stream << "varying vec2 samplePos" << i << ";\n";
    stream << "\n";
    stream << "void main(void)\n";
    stream << "{\n";
    stream << "    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n";

    for (int i = 0; i < center; i++)
        stream << "    samplePos" << i << " = gl_TexCoord[0].st + pixelSize * vec2("
               << -(1.5 + (center - i - 1) * 2.0) << ");\n";
    stream << "    samplePos" << center << " = gl_TexCoord[0].st;\n";
    for (int i = center + 1; i < size; i++)
        stream << "    samplePos" << i << " = gl_TexCoord[0].st + pixelSize * vec2("
               << 1.5 + (i - center - 1) * 2.0 << ");\n";
    stream << "\n";
    stream << "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n";
    stream << "}\n";
    stream.flush();

    // Fragment shader
    // ===================================================================
    QTextStream stream2(&fragmentSource);

    stream2 << "uniform sampler2D texUnit;\n";
    stream2 << "uniform vec4 opacity;\n\n";
    for (int i = 0; i < size; i++)
        stream2 << "varying vec2 samplePos" << i << ";\n";
    stream2 << "\n";
    for (int i = 0; i <= center; i++)
        stream2 << "const vec4 kernel" << i << " = vec4(" << kernel[i] << ");\n";
    stream2 << "\n";

    stream2 << "void main(void)\n";
    stream2 << "{\n";
    stream2 << "    vec4 sum = texture2D(texUnit, samplePos0) * kernel0;\n";
    for (int i = 1; i < size; i++)
        stream2 << "    sum += texture2D(texUnit, samplePos" << i << ") * kernel"
                << (i > center ? size - i - 1 : i) << ";\n";
    stream2 << "    gl_FragColor = sum * opacity;\n";
    stream2 << "}\n";
    stream2.flush();

    GLuint vertexShader   = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);

    if (vertexShader && fragmentShader)
        program = link(vertexShader, fragmentShader);

    if (vertexShader)
        glDeleteShader(vertexShader);

    if (fragmentShader)    
        glDeleteShader(fragmentShader);

    if (program) {
        uTexUnit   = glGetUniformLocation(program, "texUnit");
        uPixelSize = glGetUniformLocation(program, "pixelSize");
        uOpacity   = glGetUniformLocation(program, "opacity");
    }
}



// ----------------------------------------------------------------------------



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

void ARBBlurShader::setOpacity(float val)
{
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 2, val, val, val, val);
}

void ARBBlurShader::bind()
{
    if (!program)
        init();

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
}

void ARBBlurShader::unbind()
{
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
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
    QVector<float> kernel = gaussianKernel();
    const int size = kernel.size();
    const int center = size / 2;

    QByteArray text;
    QTextStream stream(&text);

    stream << "!!ARBfp1.0\n";

    // The kernel values are hardcoded into the program 
    for (int i = 0; i <= center; i++)
        stream << "PARAM kernel" << i << " = " << kernel[i] << ";\n";

    stream << "PARAM firstSample = program.local[0];\n"; // Distance from gl_TexCoord[0] to the next sample
    stream << "PARAM nextSample  = program.local[1];\n"; // Distance to the subsequent sample
    stream << "PARAM opacity     = program.local[2];\n"; // The opacity with which to modulate the pixels

    stream << "TEMP coord;\n";   // The coordinate we'll be sampling
    stream << "TEMP sample;\n";  // The sampled value
    stream << "TEMP sum;\n";     // The sum of the weighted samples

    // Start by sampling the center coordinate
    stream << "TEX sample, fragment.texcoord[0], texture[0], 2D;\n";     // sample = texture2D(tex, gl_TexCoord[0])
    stream << "MUL sum, sample, kernel" << center << ";\n";              // sum = sample * kernel[center]

    for (int i = 1; i <= center; i++) {
        if (i == 1)
            stream << "SUB coord, fragment.texcoord[0], firstSample;\n"; // coord = gl_TexCoord[0] - firstSample
        else
            stream << "SUB coord, coord, nextSample;\n";                 // coord -= nextSample
        stream << "TEX sample, coord, texture[0], 2D;\n";                // sample = texture2D(tex, coord)
        stream << "MAD sum, sample, kernel" << center - i << ", sum;\n"; // sum += sample * kernel[center - i]
    }

    for (int i = 1; i <= center; i++) {
        if (i == 1)
            stream << "ADD coord, fragment.texcoord[0], firstSample;\n"; // coord = gl_TexCoord[0] + firstSample
        else
            stream << "ADD coord, coord, nextSample;\n";                 // coord += nextSample
        stream << "TEX sample, coord, texture[0], 2D;\n";                // sample = texture2D(tex, coord)
        stream << "MAD sum, sample, kernel" << center - i << ", sum;\n"; // sum += sample * kernel[center - i]
    }
    stream << "MUL result.color, sum, opacity;\n";                       // gl_FragColor = sum * opacity
    stream << "END\n";
    stream.flush();

    glGenProgramsARB(1, &program);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, text.length(), text.constData());

    if (glGetError()) {
        const char *error = (const char*)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
        kError() << "Failed to compile fragment program:" << error;
    }

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0); 
}

