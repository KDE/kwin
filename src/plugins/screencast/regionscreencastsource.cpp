/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "screencastutils.h"

#include "libkwineffects/kwingltexture.h"
#include "libkwineffects/kwinglutils.h"
#include <compositor.h>
#include <core/output.h>
#include <drm_fourcc.h>
#include <scene/workspacescene.h>
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

quint32 RegionScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

void RegionScreenCastSource::updateOutput(Output *output)
{
    m_last = output->renderLoop()->lastPresentationTimestamp();

    if (m_renderedTexture) {
        const auto [outputTexture, colorDescription] = Compositor::self()->scene()->textureForOutput(output);
        const auto outputGeometry = output->geometry();
        if (!outputTexture || !m_region.intersects(output->geometry())) {
            return;
        }

        GLFramebuffer::pushFramebuffer(m_target.get());

        ShaderBinder shaderBinder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(m_region);
        projectionMatrix.translate(outputGeometry.left() / m_scale, outputGeometry.top() / m_scale);

        shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);
        shaderBinder.shader()->setColorspaceUniformsToSRGB(colorDescription);

        outputTexture->render(output->geometry().size(), 1 / m_scale);
        GLFramebuffer::popFramebuffer();
    }
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::ensureTexture()
{
    if (!m_renderedTexture) {
        m_renderedTexture = GLTexture::allocate(GL_RGBA8, textureSize());
        if (!m_renderedTexture) {
            return;
        }
        m_target = std::make_unique<GLFramebuffer>(m_renderedTexture.get());
        const auto allOutputs = workspace()->outputs();
        for (auto output : allOutputs) {
            if (output->geometry().intersects(m_region)) {
                updateOutput(output);
            }
        }
    }
}

void RegionScreenCastSource::render(GLFramebuffer *target)
{
    ensureTexture();

    GLFramebuffer::pushFramebuffer(target);
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(QRect(QPoint(), target->size()));
    shader->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    m_renderedTexture->render(target->size(), m_scale);

    ShaderManager::instance()->popShader();
    GLFramebuffer::popFramebuffer();
}

void RegionScreenCastSource::render(spa_data *spa, spa_video_format format)
{
    ensureTexture();
    grabTexture(m_renderedTexture.get(), spa, format);
}

uint RegionScreenCastSource::refreshRate() const
{
    uint ret = 0;
    const auto allOutputs = workspace()->outputs();
    for (auto output : allOutputs) {
        if (output->geometry().intersects(m_region)) {
            ret = std::max<uint>(ret, output->refreshRate());
        }
    }
    return ret;
}
}

#include "moc_regionscreencastsource.cpp"
