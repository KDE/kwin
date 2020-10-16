/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    void init();

    static ContrastShader *create();

    bool isValid() const {
        return mValid;
    }

    void setColorMatrix(const QMatrix4x4 &matrix);

    void setTextureMatrix(const QMatrix4x4 &matrix);
    void setModelViewProjectionMatrix(const QMatrix4x4 &matrix);

    void bind();
    void unbind();

    void setOpacity(float opacity);
    float opacity() const;

protected:
    void setIsValid(bool value) {
        mValid = value;
    }
    void reset();

private:
    bool mValid;
    GLShader *shader;
    int mvpMatrixLocation;
    int textureMatrixLocation;
    int colorMatrixLocation;
    int opacityLocation;
    float m_opacity;
};


} // namespace KWin

#endif

