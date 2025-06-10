/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glutils.h"

class QMatrix4x4;

namespace KWin
{

class ContrastShader
{
public:
    ContrastShader();

    void init();
    void setColorMatrix(const QMatrix4x4 &matrix);
    void setTextureMatrix(const QMatrix4x4 &matrix);
    void setModelViewProjectionMatrix(const QMatrix4x4 &matrix);
    void setOpacity(float opacity);

    float opacity() const;
    bool isValid() const;

    void bind();
    void unbind();

protected:
    void setIsValid(bool value);
    void reset();

private:
    bool m_valid;
    std::unique_ptr<GLShader> m_shader;
    int m_mvpMatrixLocation;
    int m_textureMatrixLocation;
    int m_colorMatrixLocation;
    int m_opacityLocation;
    float m_opacity;
};

} // namespace KWin
