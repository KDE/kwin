/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglutils.h>

#include <QMatrix4x4>
#include <QObject>
#include <QVector2D>
#include <QVector4D>

namespace KWin
{

class BlurShader : public QObject
{
    Q_OBJECT

public:
    BlurShader(QObject *parent = nullptr);
    ~BlurShader() override;

    bool isValid() const;

    enum SampleType {
        DownSampleType,
        UpSampleType,
        CopySampleType,
        NoiseSampleType
    };

    void bind(SampleType sampleType);
    void unbind();

    void setModelViewProjectionMatrix(const QMatrix4x4 &matrix);
    void setOffset(float offset);
    void setTargetTextureSize(const QSize &renderTextureSize);
    void setNoiseTextureSize(const QSize &noiseTextureSize);
    void setTexturePosition(const QPoint &texPos);
    void setBlurRect(const QRect &blurRect, const QSize &screenSize);

private:
    std::unique_ptr<GLShader> m_shaderDownsample;
    std::unique_ptr<GLShader> m_shaderUpsample;
    std::unique_ptr<GLShader> m_shaderCopysample;
    std::unique_ptr<GLShader> m_shaderNoisesample;

    int m_mvpMatrixLocationDownsample;
    int m_offsetLocationDownsample;
    int m_renderTextureSizeLocationDownsample;
    int m_halfpixelLocationDownsample;

    int m_mvpMatrixLocationUpsample;
    int m_offsetLocationUpsample;
    int m_renderTextureSizeLocationUpsample;
    int m_halfpixelLocationUpsample;

    int m_mvpMatrixLocationCopysample;
    int m_renderTextureSizeLocationCopysample;
    int m_blurRectLocationCopysample;

    int m_mvpMatrixLocationNoisesample;
    int m_offsetLocationNoisesample;
    int m_renderTextureSizeLocationNoisesample;
    int m_noiseTextureSizeLocationNoisesample;
    int m_texStartPosLocationNoisesample;
    int m_halfpixelLocationNoisesample;

    // Caching uniform values to aviod unnecessary setUniform calls
    int m_activeSampleType = -1;

    float m_offsetDownsample = 0.0;
    QMatrix4x4 m_matrixDownsample;

    float m_offsetUpsample = 0.0;
    QMatrix4x4 m_matrixUpsample;

    QMatrix4x4 m_matrixCopysample;

    float m_offsetNoisesample = 0.0;
    QVector2D m_noiseTextureSizeNoisesample;
    QMatrix4x4 m_matrixNoisesample;

    bool m_valid = false;

    Q_DISABLE_COPY(BlurShader);
};

inline bool BlurShader::isValid() const
{
    return m_valid;
}

} // namespace KWin
