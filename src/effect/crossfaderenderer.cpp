/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "crossfaderenderer.h"

#include "globals.h"
#include <opengl/glshadermanager.h>
#include <opengl/gltexture.h>
#include <opengl/glvertexbuffer.h>

namespace KWin
{

CrossFadeRenderer::CrossFadeRenderer()
{
    m_shader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture,
        QStringLiteral(":/effect/shaders/crossfade.vert"),
        QStringLiteral(":/effect/shaders/crossfade.frag"));

    m_modelViewProjectioMatrixLocation = m_shader->uniformLocation("modelViewProjectionMatrix");
    m_blendFactorLocation = m_shader->uniformLocation("blendFactor");
    m_previousTextureLocation = m_shader->uniformLocation("previousTexture");
    m_currentTextureLocation = m_shader->uniformLocation("currentTexture");
}

GLVertexBuffer *texturedRectVbo(const QRectF &deviceGeometry)
{
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const auto opt = vbo->map<GLVertex2D>(6);
    if (!opt) {
        return nullptr;
    }
    const auto map = *opt;

    // first triangle
    map[0] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[1] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };
    map[2] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.bottom()),
        .texcoord = QVector2D(0.0, 0.0),
    };

    // second triangle
    map[3] = GLVertex2D{
        .position = QVector2D(deviceGeometry.left(), deviceGeometry.top()),
        .texcoord = QVector2D(0.0, 1.0),
    };
    map[4] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.top()),
        .texcoord = QVector2D(1.0, 1.0),
    };
    map[5] = GLVertex2D{
        .position = QVector2D(deviceGeometry.right(), deviceGeometry.bottom()),
        .texcoord = QVector2D(1.0, 0.0),
    };

    vbo->unmap();
    return vbo;
}

void CrossFadeRenderer::render(const QMatrix4x4 &modelViewProjectionMatrix, GLTexture *textureFrom, GLTexture *textureTo, const QRectF &deviceGeometry, float blendFactor)
{
    glActiveTexture(GL_TEXTURE1);
    textureFrom->bind();
    glActiveTexture(GL_TEXTURE0);
    textureTo->bind();

    // Clear the background.
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLVertexBuffer *vbo = texturedRectVbo(deviceGeometry);
    if (!vbo) {
        return;
    }

    ShaderManager *sm = ShaderManager::instance();
    sm->pushShader(m_shader.get());
    m_shader->setUniform(m_modelViewProjectioMatrixLocation, modelViewProjectionMatrix);
    m_shader->setUniform(m_blendFactorLocation, blendFactor);
    m_shader->setUniform(m_currentTextureLocation, 0);
    m_shader->setUniform(m_previousTextureLocation, 1);

    vbo->bindArrays();
    vbo->draw(GL_TRIANGLES, 0, 6);
    vbo->unbindArrays();
    sm->popShader();

    glActiveTexture(GL_TEXTURE1);
    textureFrom->unbind();
    glActiveTexture(GL_TEXTURE0);
    textureTo->unbind();
}

}
