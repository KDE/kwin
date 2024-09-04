/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "screencastutils.h"

#include "compositor.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "cursor.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include "scene/workspacescene.h"

#include <drm_fourcc.h>

namespace KWin
{

OutputScreenCastSource::OutputScreenCastSource(Output *output, QObject *parent)
    : ScreenCastSource(parent)
    , m_output(output)
{
    connect(m_output, &QObject::destroyed, this, &ScreenCastSource::closed);
    connect(m_output, &Output::enabledChanged, this, [this] {
        if (!m_output->isEnabled()) {
            Q_EMIT closed();
        }
    });
}

OutputScreenCastSource::~OutputScreenCastSource()
{
    pause();
}

quint32 OutputScreenCastSource::drmFormat() const
{
    return DRM_FORMAT_ARGB8888;
}

QSize OutputScreenCastSource::textureSize() const
{
    return m_output->pixelSize();
}

qreal OutputScreenCastSource::devicePixelRatio() const
{
    return m_output->scale();
}

void OutputScreenCastSource::render(QImage *target)
{
    const auto [outputTexture, colorDescription] = Compositor::self()->scene()->textureForOutput(m_output);
    if (outputTexture) {
        grabTexture(outputTexture.get(), target);
    }
}

void OutputScreenCastSource::render(GLFramebuffer *target)
{
    const auto [outputTexture, colorDescription] = Compositor::self()->scene()->textureForOutput(m_output);
    if (!outputTexture) {
        return;
    }

    const bool yuv = colorDescription.yuvCoefficients() != YUVMatrixCoefficients::Identity;
    ShaderBinder shaderBinder((yuv ? ShaderTrait::MapYUVTexture : ShaderTrait::MapTexture) | ShaderTrait::TransformColorspace);
    QMatrix4x4 projectionMatrix;
    projectionMatrix.scale(1, -1);
    projectionMatrix.ortho(QRect(QPoint(), textureSize()));
    shaderBinder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projectionMatrix);
    shaderBinder.shader()->setColorspaceUniforms(colorDescription, ColorDescription::sRGB, RenderingIntent::RelativeColorimetricWithBPC);
    if (yuv) {
        shaderBinder.shader()->setUniform(GLShader::Mat4Uniform::YuvToRgb, colorDescription.yuvMatrix());
        shaderBinder.shader()->setUniform(GLShader::IntUniform::Sampler, 0);
        shaderBinder.shader()->setUniform(GLShader::IntUniform::Sampler1, 1);
    } else {
        shaderBinder.shader()->setUniform(GLShader::IntUniform::Sampler, 0);
    }

    GLFramebuffer::pushFramebuffer(target);
    outputTexture->render(textureSize());
    GLFramebuffer::popFramebuffer();
}

std::chrono::nanoseconds OutputScreenCastSource::clock() const
{
    return m_output->renderLoop()->lastPresentationTimestamp();
}

uint OutputScreenCastSource::refreshRate() const
{
    return m_output->refreshRate();
}

void OutputScreenCastSource::report(const QRegion &damage)
{
    if (!damage.isEmpty()) {
        Q_EMIT frame(scaleRegion(damage, m_output->scale()));
    }
}

void OutputScreenCastSource::resume()
{
    if (m_active) {
        return;
    }

    connect(m_output, &Output::outputChange, this, &OutputScreenCastSource::report);
    report(m_output->rect());

    m_active = true;
}

void OutputScreenCastSource::pause()
{
    if (!m_active) {
        return;
    }

    if (m_output) {
        disconnect(m_output, &Output::outputChange, this, &OutputScreenCastSource::report);
    }

    m_active = false;
}

bool OutputScreenCastSource::includesCursor(Cursor *cursor) const
{
    if (Cursors::self()->isCursorHidden()) {
        return false;
    }

    return cursor->isOnOutput(m_output);
}

QPointF OutputScreenCastSource::mapFromGlobal(const QPointF &point) const
{
    return m_output->mapFromGlobal(point);
}

QRectF OutputScreenCastSource::mapFromGlobal(const QRectF &rect) const
{
    return m_output->mapFromGlobal(rect);
}

} // namespace KWin

#include "moc_outputscreencastsource.cpp"
