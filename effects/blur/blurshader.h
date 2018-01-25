/*
 *   Copyright © 2010 Fredrik Höglund <fredrik@kde.org>
 *   Copyright © 2018 Alex Nemeth <alex.nemeth329@gmail.com>
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

#ifndef BLURSHADER_H
#define BLURSHADER_H

#include <kwinglutils.h>
#include <QVector2D>
#include <QVector4D>


class QMatrix4x4;

namespace KWin
{

class BlurShader
{
public:
    BlurShader();
    virtual ~BlurShader();

    static BlurShader *create();

    bool isValid() const {
        return mValid;
    }

    virtual void setModelViewProjectionMatrix(const QMatrix4x4 &matrix) = 0;
    virtual void setOffset(float offset) = 0;
    virtual void setTargetSize(QSize renderTextureSize) = 0;
    virtual void setBlurRect(QRect blurRect, QSize screenSize) = 0;

    enum SampleType {
        DownSampleType,
        UpSampleType,
        CopySampleType
    };

    virtual void bind(SampleType sampleType) = 0;
    virtual void unbind() = 0;

protected:
    void setIsValid(bool value) {
        mValid = value;
    }
    virtual void init() = 0;
    virtual void reset() = 0;

private:
    bool mValid;
};


// ----------------------------------------------------------------------------



class GLSLBlurShader : public BlurShader
{
public:
    GLSLBlurShader();
    ~GLSLBlurShader();

    void bind(SampleType sampleType) override final;
    void unbind() override final;
    void setModelViewProjectionMatrix(const QMatrix4x4 &matrix) override final;
    void setOffset(float offset) override final;
    void setTargetSize(QSize renderTextureSize) override final;
    void setBlurRect(QRect blurRect, QSize screenSize) override final;

protected:
    void init() override final;
    void reset() override final;

private:
    GLShader *m_shaderDownsample = nullptr;
    GLShader *m_shaderUpsample = nullptr;
    GLShader *m_shaderCopysample = nullptr;

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


    //Caching uniform values to aviod unnecessary setUniform calls
    int m_activeSampleType;

    float m_offsetDownsample;
    QMatrix4x4 m_matrixDownsample;
    QSize m_renderTextureSizeDownsample;

    float m_offsetUpsample;
    QMatrix4x4 m_matrixUpsample;
    QSize m_renderTextureSizeUpsample;

    QMatrix4x4 m_matrixCopysample;
    QSize m_renderTextureSizeCopysample;
    QRect m_blurRectCopysample;

};

} // namespace KWin

#endif
