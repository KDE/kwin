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
    : program(0), radius(12)
{
}

BlurShader::~BlurShader()
{
    if (program) {
        glDeleteProgramsARB(1, &program);
        program = 0;
    }
}

void BlurShader::setRadius(int _radius)
{
    int r = qMax(_radius, 2);

    if (radius != r) {
        radius = r;

        if (program) {
            glDeleteProgramsARB(1, &program);
            program = 0;
        }
    }
}

void BlurShader::setDirection(Qt::Orientation orientation)
{
    direction = orientation;
}

void BlurShader::setPixelDistance(float val)
{
    float firstStep = val * 1.5;
    float nextStep = val * 2.0;

    if (direction == Qt::Horizontal) {
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, firstStep, 0, 0, 0);
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, nextStep, 0, 0, 0);
    } else {
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 0, 0, firstStep, 0, 0);
        glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 1, 0, nextStep, 0, 0);
    }
}

void BlurShader::setOpacity(float val)
{
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, 2, val, val, val, val);
}

void BlurShader::bind()
{
    if (!program)
        init();

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);
}

void BlurShader::unbind()
{
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

float BlurShader::gaussian(float x, float sigma) const
{
    return (1.0 / std::sqrt(2.0 * M_PI) * sigma)
            * std::exp(-((x * x) / (2.0 * sigma * sigma)));
}

QVector<float> BlurShader::gaussianKernel() const
{
    const int size = radius | 1;
    QVector<float> kernel(size);
    const qreal sigma = radius / 2.5;
    const int center = size / 2;

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

void BlurShader::init()
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
        kError() << "Error when compiling fragment program:" << error;
    }

    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0); 
}

