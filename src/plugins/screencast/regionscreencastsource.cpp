/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "screencastutils.h"

#include <abstract_wayland_output.h>
#include <composite.h>
#include <kwingltexture.h>
#include <scene.h>
#include <kwinglutils.h>
#include <platform.h>

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

void RegionScreenCastSource::updateOutput(AbstractWaylandOutput *output)
{
    m_last = output->renderLoop()->lastPresentationTimestamp();

    if (!m_renderedTexture.isNull()) {
        const QSharedPointer<GLTexture> outputTexture = Compositor::self()->scene()->textureForOutput(output);
        const auto outputGeometry = output->geometry();
        if (!outputTexture || !m_region.intersects(output->geometry())) {
            return;
        }

        GLRenderTarget::pushRenderTarget(m_target.data());
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
        GLRenderTarget::popRenderTarget();
    }
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::render(GLRenderTarget *target)
{
    if (!m_renderedTexture) {
        m_renderedTexture.reset(new GLTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize()));
        m_target.reset(new GLRenderTarget(m_renderedTexture.data()));
        const auto allOutputs = kwinApp()->platform()->enabledOutputs();
        for (auto output : allOutputs) {
            AbstractWaylandOutput *streamOutput = qobject_cast<AbstractWaylandOutput *>(output);
            if (streamOutput->geometry().intersects(m_region)) {
                updateOutput(streamOutput);
            }
        }
    }

    GLRenderTarget::pushRenderTarget(target);
    QRect r(QPoint(), target->size());
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(r);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    m_renderedTexture->bind();
    m_renderedTexture->render(r);
    m_renderedTexture->unbind();

    ShaderManager::instance()->popShader();
    GLRenderTarget::popRenderTarget();
}

void RegionScreenCastSource::render(QImage *image)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLRenderTarget offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, image);
}

}
