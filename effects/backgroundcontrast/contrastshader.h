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

#ifndef CONTRASTSHADER_H
#define CONTRASTSHADER_H

#include <kwinglutils.h>

class QMatrix4x4;

namespace KWin
{

class ContrastShader
{
public:
    ContrastShader();
    virtual ~ContrastShader();

    static ContrastShader *create();

    bool isValid() const {
        return mValid;
    }

    // Sets the radius in pixels
    void setStrength(int strength);
    int strength() const {
        return mStrength;
    }
    
    virtual void setColorMatrix(const QMatrix4x4 &matrix) = 0;

    virtual void setTextureMatrix(const QMatrix4x4 &matrix) = 0;
    virtual void setModelViewProjectionMatrix(const QMatrix4x4 &matrix) = 0;

    virtual void bind() = 0;
    virtual void unbind() = 0;

protected:
    float gaussian(float x, float sigma) const;
    void setIsValid(bool value) {
        mValid = value;
    }
    virtual void init() = 0;
    virtual void reset() = 0;

private:
    int mStrength;
    bool mValid;
};


// ----------------------------------------------------------------------------



class GLSLContrastShader : public ContrastShader
{
public:
    GLSLContrastShader();
    ~GLSLContrastShader();

    void bind();
    void unbind();
    void setColorMatrix(const QMatrix4x4 &matrix);
    void setTextureMatrix(const QMatrix4x4 &matrix);
    void setModelViewProjectionMatrix(const QMatrix4x4 &matrix);

    static bool supported();

protected:
    void init();
    void reset();

private:
    GLShader *shader;
    int mvpMatrixLocation;
    int textureMatrixLocation;
    int colorMatrixLocation;
    int pixelSizeLocation;
};


} // namespace KWin

#endif

