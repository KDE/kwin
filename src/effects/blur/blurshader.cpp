/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blurshader.h"

#include <kwineffects.h>

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(blur);
}

namespace KWin
{

BlurShader::BlurShader(QObject *parent)
    : QObject(parent)
{
    ensureResources();

    m_shaderDownsample = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effects/blur/shaders/vertex.vert"),
        QStringLiteral(":/effects/blur/shaders/downsample.frag"));

    m_shaderUpsample = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effects/blur/shaders/vertex.vert"),
        QStringLiteral(":/effects/blur/shaders/upsample.frag"));

    m_shaderCopysample = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effects/blur/shaders/vertex.vert"),
        QStringLiteral(":/effects/blur/shaders/copy.frag"));

    m_shaderNoisesample = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effects/blur/shaders/vertex.vert"),
        QStringLiteral(":/effects/blur/shaders/noise.frag"));

    m_valid = m_shaderDownsample->isValid() && m_shaderUpsample->isValid() && m_shaderCopysample->isValid() && m_shaderNoisesample->isValid();

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

        // Add default values to the uniforms of the shaders
        ShaderManager::instance()->pushShader(m_shaderDownsample.get());
        m_shaderDownsample->setUniform(m_mvpMatrixLocationDownsample, modelViewProjection);
        m_shaderDownsample->setUniform(m_offsetLocationDownsample, float(1.0));
        m_shaderDownsample->setUniform(m_renderTextureSizeLocationDownsample, QVector2D(1.0, 1.0));
        m_shaderDownsample->setUniform(m_halfpixelLocationDownsample, QVector2D(1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderUpsample.get());
        m_shaderUpsample->setUniform(m_mvpMatrixLocationUpsample, modelViewProjection);
        m_shaderUpsample->setUniform(m_offsetLocationUpsample, float(1.0));
        m_shaderUpsample->setUniform(m_renderTextureSizeLocationUpsample, QVector2D(1.0, 1.0));
        m_shaderUpsample->setUniform(m_halfpixelLocationUpsample, QVector2D(1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderCopysample.get());
        m_shaderCopysample->setUniform(m_mvpMatrixLocationCopysample, modelViewProjection);
        m_shaderCopysample->setUniform(m_renderTextureSizeLocationCopysample, QVector2D(1.0, 1.0));
        m_shaderCopysample->setUniform(m_blurRectLocationCopysample, QVector4D(1.0, 1.0, 1.0, 1.0));
        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_shaderNoisesample.get());
        m_shaderNoisesample->setUniform(m_mvpMatrixLocationNoisesample, modelViewProjection);
        m_shaderNoisesample->setUniform(m_offsetLocationNoisesample, float(1.0));
        m_shaderNoisesample->setUniform(m_renderTextureSizeLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_noiseTextureSizeLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_texStartPosLocationNoisesample, QVector2D(1.0, 1.0));
        m_shaderNoisesample->setUniform(m_halfpixelLocationNoisesample, QVector2D(1.0, 1.0));

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
        blurRect.left() / float(screenSize.width()),
        1.0 - blurRect.bottom() / float(screenSize.height()),
        blurRect.right() / float(screenSize.width()),
        1.0 - blurRect.top() / float(screenSize.height()));

    m_shaderCopysample->setUniform(m_blurRectLocationCopysample, rect);
}

void BlurShader::bind(SampleType sampleType)
{
    if (!isValid()) {
        return;
    }

    switch (sampleType) {
    case CopySampleType:
        ShaderManager::instance()->pushShader(m_shaderCopysample.get());
        break;

    case UpSampleType:
        ShaderManager::instance()->pushShader(m_shaderUpsample.get());
        break;

    case DownSampleType:
        ShaderManager::instance()->pushShader(m_shaderDownsample.get());
        break;

    case NoiseSampleType:
        ShaderManager::instance()->pushShader(m_shaderNoisesample.get());
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
