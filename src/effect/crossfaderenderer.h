/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glshader.h"
#include <memory>

namespace KWin
{
class GLTexture;

/**
 * Helper class to cross-fade two textures
 */
class KWIN_EXPORT CrossFadeRenderer
{
public:
    CrossFadeRenderer();

    void render(const QMatrix4x4 &modelViewProjectionMatrix, GLTexture *textureFrom, GLTexture *textureTo, const QRectF &deviceGeometry, float blendFactor);

private:
    std::unique_ptr<GLShader> m_shader;
    int m_previousTextureLocation = -1;
    int m_currentTextureLocation = -1;
    int m_modelViewProjectioMatrixLocation = -1;
    int m_blendFactorLocation = -1;
};

}
