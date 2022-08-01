/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "screencastutils.h"

#include <composite.h>
#include <kwingltexture.h>
#include <kwinglutils.h>
#include <output.h>
#include <scene.h>
#include <workspace.h>

#include <QPainter>

namespace KWin
{

RegionScreenCastSource::RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent)
    : ScreenCastSource(parent)
    , m_region(region)
    , m_scale(scale)
{
    Q_ASSERT(m_region.isValid());
    Q_ASSERT(m_scale > 0);
}

QSize RegionScreenCastSource::textureSize() const
{
    return m_region.size() * m_scale;
}

bool RegionScreenCastSource::hasAlphaChannel() const
{
    return true;
}

void RegionScreenCastSource::updateOutput(Output *output)
{
    m_last = output->renderLoop()->lastPresentationTimestamp();

    if (m_renderedTexture) {
        const std::shared_ptr<GLTexture> outputTexture = Compositor::self()->scene()->textureForOutput(output);
        const auto outputGeometry = output->geometry();
        if (!outputTexture || !m_region.intersects(output->geometry())) {
            return;
        }

        GLFramebuffer::pushFramebuffer(m_target.get());
        const QRect geometry({0, 0}, m_target->size());

        ShaderBinder shaderBinder(ShaderTrait::MapTexture);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(m_region);

        const QPoint pos = outputGeometry.topLeft();
        projectionMatrix.translate(pos.x(), pos.y());

        shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

        outputTexture->bind();
        outputTexture->render(output->geometry());
        outputTexture->unbind();
        GLFramebuffer::popFramebuffer();
    }
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::render(GLFramebuffer *target)
{
    if (!m_renderedTexture) {
        m_renderedTexture.reset(new GLTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize()));
        m_target.reset(new GLFramebuffer(m_renderedTexture.get()));
        const auto allOutputs = workspace()->outputs();
        for (auto output : allOutputs) {
            if (output->geometry().intersects(m_region)) {
                updateOutput(output);
            }
        }
    }

    GLFramebuffer::pushFramebuffer(target);
    QRect r(QPoint(), target->size());
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(r);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    m_renderedTexture->bind();
    m_renderedTexture->render(r);
    m_renderedTexture->unbind();

    ShaderManager::instance()->popShader();
    GLFramebuffer::popFramebuffer();
}

void RegionScreenCastSource::render(QImage *image)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLFramebuffer offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, image);
}

}
