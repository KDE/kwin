/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blurshader.h"

#include <kwineffects.h>
#include <kwinglplatform.h>
#include <kwinglutils.h>

#include <QByteArray>
#include <QTextStream>

namespace KWin
{

BlurShader::BlurShader(QObject *parent)
    : QObject(parent)
{
    const bool gles = GLPlatform::instance()->isGLES();
    const bool glsl_140 = !gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(1, 40);
    const bool core = glsl_140 || (gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0));

    QByteArray vertexSource;
    QByteArray fragmentDownSource;
    QByteArray fragmentUpSource;
    QByteArray fragmentCopySource;
    QByteArray fragmentNoiseSource;

    const QByteArray attribute = core ? "in"        : "attribute";
    const QByteArray texture2D = core ? "texture"   : "texture2D";
    const QByteArray fragColor = core ? "fragColor" : "gl_FragColor";

    QString glHeaderString;

    if (gles) {
        if (core) {
            glHeaderString += "#version 300 es\n\n";
        }

        glHeaderString += "precision highp float;\n";
    } else if (glsl_140) {
        glHeaderString += "#version 140\n\n";
    }

    QString glUniformString = "uniform sampler2D texUnit;\n"
        "uniform float offset;\n"
        "uniform vec2 renderTextureSize;\n"
        "uniform vec2 halfpixel;\n";

    if (core) {
        glUniformString += "out vec4 fragColor;\n\n";
    }

    // Vertex shader
    QTextStream streamVert(&vertexSource);

    streamVert << glHeaderString;

    streamVert << "uniform mat4 modelViewProjectionMatrix;\n";
    streamVert << attribute << " vec4 vertex;\n\n";
    streamVert << "\n";
    streamVert << "void main(void)\n";
    streamVert << "{\n";
    streamVert << "    gl_Position = modelViewProjectionMatrix * vertex;\n";
    streamVert << "}\n";

    streamVert.flush();

    // Fragment shader (Dual Kawase Blur) - Downsample
    QTextStream streamFragDown(&fragmentDownSource);

    streamFragDown << glHeaderString << glUniformString;

    streamFragDown << "void main(void)\n";
    streamFragDown << "{\n";
    streamFragDown << "    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);\n";
    streamFragDown << "    \n";
    streamFragDown << "    vec4 sum = " << texture2D << "(texUnit, uv) * 4.0;\n";
    streamFragDown << "    sum += " << texture2D << "(texUnit, uv - halfpixel.xy * offset);\n";
    streamFragDown << "    sum += " << texture2D << "(texUnit, uv + halfpixel.xy * offset);\n";
    streamFragDown << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset);\n";
    streamFragDown << "    sum += " << texture2D << "(texUnit, uv - vec2(halfpixel.x, -halfpixel.y) * offset);\n";
    streamFragDown << "    \n";
    streamFragDown << "    " << fragColor << " = sum / 8.0;\n";
    streamFragDown << "}\n";

    streamFragDown.flush();

    // Fragment shader (Dual Kawase Blur) - Upsample
    QTextStream streamFragUp(&fragmentUpSource);

    streamFragUp << glHeaderString << glUniformString;

    streamFragUp << "void main(void)\n";
    streamFragUp << "{\n";
    streamFragUp << "    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);\n";
    streamFragUp << "    \n";
    streamFragUp << "    vec4 sum = " << texture2D << "(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);\n";
    streamFragUp << "    sum += " << texture2D << "(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;\n";
    streamFragUp << "    \n";
    streamFragUp << "    " << fragColor << " = sum / 12.0;\n";
    streamFragUp << "}\n";

    streamFragUp.flush();

    // Fragment shader - Copy texture
    QTextStream streamFragCopy(&fragmentCopySource);

    streamFragCopy << glHeaderString;

    streamFragCopy << "uniform sampler2D texUnit;\n";
    streamFragCopy << "uniform vec2 renderTextureSize;\n";
    streamFragCopy << "uniform vec4 blurRect;\n";

    if (core) {
        streamFragCopy << "out vec4 fragColor;\n\n";
    }

    streamFragCopy << "void main(void)\n";
    streamFragCopy << "{\n";
    streamFragCopy << "     vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);\n";
    streamFragCopy << "    " << fragColor << " = " << texture2D << "(texUnit, clamp(uv, blurRect.xy, blurRect.zw));\n";
    streamFragCopy << "}\n";

    streamFragCopy.flush();

    // Fragment shader - Noise texture
    QTextStream streamFragNoise(&fragmentNoiseSource);

    streamFragNoise << glHeaderString << glUniformString;

    streamFragNoise << "uniform sampler2D noiseTexUnit;\n";
    streamFragNoise << "uniform vec2 noiseTextureSize;\n";
    streamFragNoise << "uniform vec2 texStartPos;\n";

    // Upsampling + Noise
    streamFragNoise << "void main(void)\n";
    streamFragNoise << "{\n";
    streamFragNoise << "    vec2 uv = vec2(gl_FragCoord.xy / renderTextureSize);\n";
    streamFragNoise << "    vec2 uvNoise = vec2((texStartPos.xy + gl_FragCoord.xy) / noiseTextureSize);\n";
    streamFragNoise << "    \n";
    streamFragNoise << "    vec4 sum = " << texture2D << "(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);\n";
    streamFragNoise << "    sum += " << texture2D << "(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;\n";
    streamFragNoise << "    \n";
    streamFragNoise << "    " << fragColor << " = sum / 12.0 - (vec4(0.5, 0.5, 0.5, 0) - vec4(" << texture2D << "(noiseTexUnit, uvNoise).rrr, 0));\n";
    streamFragNoise << "}\n";

    streamFragNoise.flush();

    m_shaderDownsample.reset(ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentDownSource));
    m_shaderUpsample.reset(ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentUpSource));
    m_shaderCopysample.reset(ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentCopySource));
    m_shaderNoisesample.reset(ShaderManager::instance()->loadShaderFromCode(vertexSource, fragmentNoiseSource));

    m_valid = m_shaderDownsample->isValid() &&
        m_shaderUpsample->isValid() &&
        m_shaderCopysample->isValid() &&
        m_shaderNoisesample->isValid();

    if (m_valid) {
        m_mvpMatrixLocationDownsample = m_shaderDownsample->uniformLocation("modelViewProjectionMatrix");
        m_offsetLocationDownsample = m_shaderDownsample->uniformLocation("offset");
        m_renderTextureSizeLocationDownsample = m_shaderDownsample->uniformLocation("renderTextureSize");
        m_halfpixelLocationDownsample = m_shaderDownsample->uniformLocation("halfpixel");

        m_mvpMatrixLocationUpsample = m_shaderUpsample->uniformLocation("modelViewProjectionMatrix");
        m_offsetLocationUpsample = m_shaderUpsample->uniformLocation("offset");
        m_renderTextureSizeLocationUpsample = m_shaderUpsample->uniformLocation("renderTextureSize");
        m_halfpixelLocationUpsample = m_shaderUpsample->uniformLocation("halfpixel");

        m_mvpMatrixLocationCopysample = m_shaderCopysample->uniformLocation("modelViewProjectionMatrix");
        m_renderTextureSizeLocationCopysample = m_shaderCopysample->uniformLocation("renderTextureSize");
        m_blurRectLocationCopysample = m_shaderCopysample->uniformLocation("blurRect");

        m_mvpMatrixLocationNoisesample = m_shaderNoisesample->uniformLocation("modelViewProjectionMatrix");
        m_offsetLocationNoisesample = m_shaderNoisesample->uniformLocation("offset");
        m_renderTextureSizeLocationNoisesample = m_shaderNoisesample->uniformLocation("renderTextureSize");
        m_noiseTextureSizeLocationNoisesample = m_shaderNoisesample->uniformLocation("noiseTextureSize");
        m_texStartPosLocationNoisesample = m_shaderNoisesample->uniformLocation("texStartPos");
        m_halfpixelLocationNoisesample = m_shaderNoisesample->uniformLocation("halfpixel");

        QMatrix4x4 modelViewProjection;
        const QSize screenSize = effects->virtualScreenSize();
        modelViewProjection.ortho(0, screenSize.width(), screenSize.height(), 0, 0, 65535);

        //Add default values to the uniforms of the shaders
        ShaderManager::instance()->pushShader(m_shaderDownsample.data());
        m_shaderDownsample->setUniform(m_mvpMatrixLocationDownsample, modelViewProjection);
        m_shaderDownsample->setUniform(m_offsetLocationDownsample, float(1.0));
        m_shaderDownsample->setUniform(m_renderTextureSizeLocationDownsample, QVector2D(1.0, 1.0));
        m_shaderDownsample->setUniform(m_halfpixelLocationDownsample, QVector2D(1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderUpsample.data());
        m_shaderUpsample->setUniform(m_mvpMatrixLocationUpsample, modelViewProjection);
        m_shaderUpsample->setUniform(m_offsetLocationUpsample, float(1.0));
        m_shaderUpsample->setUniform(m_renderTextureSizeLocationUpsample, QVector2D(1.0, 1.0));
        m_shaderUpsample->setUniform(m_halfpixelLocationUpsample, QVector2D(1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderCopysample.data());
        m_shaderCopysample->setUniform(m_mvpMatrixLocationCopysample, modelViewProjection);
        m_shaderCopysample->setUniform(m_renderTextureSizeLocationCopysample, QVector2D(1.0, 1.0));
        m_shaderCopysample->setUniform(m_blurRectLocationCopysample, QVector4D(1.0, 1.0, 1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderNoisesample.data());
        m_shaderNoisesample->setUniform(m_mvpMatrixLocationNoisesample, modelViewProjection);
        m_shaderNoisesample->setUniform(m_offsetLocationNoisesample, float(1.0));
        m_shaderNoisesample->setUniform(m_renderTextureSizeLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_noiseTextureSizeLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_texStartPosLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_halfpixelLocationNoisesample, QVector2D(1.0, 1.0));

        glUniform1i(m_shaderNoisesample->uniformLocation("texUnit"), 0);
        glUniform1i(m_shaderNoisesample->uniformLocation("noiseTexUnit"), 1);

        ShaderManager::instance()->popShader();
    }
}

BlurShader::~BlurShader()
{
}

void BlurShader::setModelViewProjectionMatrix(const QMatrix4x4 &matrix)
{
    if (!isValid()) {
        return;
    }

    switch (m_activeSampleType) {
    case CopySampleType:
        if (matrix == m_matrixCopysample) {
            return;
        }

        m_matrixCopysample = matrix;
        m_shaderCopysample->setUniform(m_mvpMatrixLocationCopysample, matrix);
        break;

    case UpSampleType:
        if (matrix == m_matrixUpsample) {
            return;
        }

        m_matrixUpsample = matrix;
        m_shaderUpsample->setUniform(m_mvpMatrixLocationUpsample, matrix);
        break;

    case DownSampleType:
        if (matrix == m_matrixDownsample) {
            return;
        }

        m_matrixDownsample = matrix;
        m_shaderDownsample->setUniform(m_mvpMatrixLocationDownsample, matrix);
        break;

    case NoiseSampleType:
        if (matrix == m_matrixNoisesample) {
            return;
        }

        m_matrixNoisesample = matrix;
        m_shaderNoisesample->setUniform(m_mvpMatrixLocationNoisesample, matrix);
        break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

void BlurShader::setOffset(float offset)
{
    if (!isValid()) {
        return;
    }

    switch (m_activeSampleType) {
    case UpSampleType:
        if (offset == m_offsetUpsample) {
            return;
        }

        m_offsetUpsample = offset;
        m_shaderUpsample->setUniform(m_offsetLocationUpsample, offset);
        break;

    case DownSampleType:
        if (offset == m_offsetDownsample) {
            return;
        }

        m_offsetDownsample = offset;
        m_shaderDownsample->setUniform(m_offsetLocationDownsample, offset);
        break;

    case NoiseSampleType:
        if (offset == m_offsetNoisesample) {
            return;
        }

        m_offsetNoisesample = offset;
        m_shaderNoisesample->setUniform(m_offsetLocationNoisesample, offset);
        break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

void BlurShader::setTargetTextureSize(const QSize &renderTextureSize)
{
    if (!isValid()) {
        return;
    }

    const QVector2D texSize(renderTextureSize.width(), renderTextureSize.height());

    switch (m_activeSampleType) {
    case CopySampleType:
        m_shaderCopysample->setUniform(m_renderTextureSizeLocationCopysample, texSize);
        break;

    case UpSampleType:
        m_shaderUpsample->setUniform(m_renderTextureSizeLocationUpsample, texSize);
        m_shaderUpsample->setUniform(m_halfpixelLocationUpsample, QVector2D(0.5 / texSize.x(), 0.5 / texSize.y()));
        break;

    case DownSampleType:
        m_shaderDownsample->setUniform(m_renderTextureSizeLocationDownsample, texSize);
        m_shaderDownsample->setUniform(m_halfpixelLocationDownsample, QVector2D(0.5 / texSize.x(), 0.5 / texSize.y()));
        break;

    case NoiseSampleType:
        m_shaderNoisesample->setUniform(m_renderTextureSizeLocationNoisesample, texSize);
        m_shaderNoisesample->setUniform(m_halfpixelLocationNoisesample, QVector2D(0.5 / texSize.x(), 0.5 / texSize.y()));
        break;

    default:
        Q_UNREACHABLE();
        break;
    }
}

void BlurShader::setNoiseTextureSize(const QSize &noiseTextureSize)
{
    const QVector2D noiseTexSize(noiseTextureSize.width(), noiseTextureSize.height());

    if (noiseTexSize != m_noiseTextureSizeNoisesample) {
        m_noiseTextureSizeNoisesample = noiseTexSize;
        m_shaderNoisesample->setUniform(m_noiseTextureSizeLocationNoisesample, noiseTexSize);
    }
}

void BlurShader::setTexturePosition(const QPoint &texPos)
{
    m_shaderNoisesample->setUniform(m_texStartPosLocationNoisesample, QVector2D(-texPos.x(), texPos.y()));
}

void BlurShader::setBlurRect(const QRect &blurRect, const QSize &screenSize)
{
    if (!isValid()) {
        return;
    }

    const QVector4D rect(
        blurRect.left()         / float(screenSize.width()),
        1.0 - blurRect.bottom() / float(screenSize.height()),
        blurRect.right()        / float(screenSize.width()),
        1.0 - blurRect.top()    / float(screenSize.height())
    );

    m_shaderCopysample->setUniform(m_blurRectLocationCopysample, rect);
}

void BlurShader::bind(SampleType sampleType)
{
    if (!isValid()) {
        return;
    }

    switch (sampleType) {
    case CopySampleType:
        ShaderManager::instance()->pushShader(m_shaderCopysample.data());
        break;

    case UpSampleType:
        ShaderManager::instance()->pushShader(m_shaderUpsample.data());
        break;

    case DownSampleType:
        ShaderManager::instance()->pushShader(m_shaderDownsample.data());
        break;

    case NoiseSampleType:
        ShaderManager::instance()->pushShader(m_shaderNoisesample.data());
        break;

    default:
        Q_UNREACHABLE();
        break;
    }

    m_activeSampleType = sampleType;
}

void BlurShader::unbind()
{
    ShaderManager::instance()->popShader();
}

} // namespace KWin
