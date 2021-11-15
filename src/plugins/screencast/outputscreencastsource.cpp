/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputscreencastsource.h"
#include "screencastutils.h"

#include "abstract_output.h"
#include "composite.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "scene.h"

namespace KWin
{

OutputScreenCastSource::OutputScreenCastSource(AbstractOutput *output, QObject *parent)
    : ScreenCastSource(parent)
    , m_output(output)
{
    connect(m_output, &QObject::destroyed, this, &ScreenCastSource::closed);
}

bool OutputScreenCastSource::hasAlphaChannel() const
{
    return true;
}

QSize OutputScreenCastSource::textureSize() const
{
    return m_output->pixelSize();
}

void OutputScreenCastSource::render(QImage *image)
{
    const QSharedPointer<GLTexture> outputTexture = Compositor::self()->scene()->textureForOutput(m_output);
    if (outputTexture) {
        grabTexture(outputTexture.data(), image);
    }
}

void OutputScreenCastSource::render(GLRenderTarget *target)
{
    const QSharedPointer<GLTexture> outputTexture = Compositor::self()->scene()->textureForOutput(m_output);
    if (!outputTexture) {
        return;
    }

    const QRect geometry(QPoint(), textureSize());

    ShaderBinder shaderBinder(ShaderTrait::MapTexture);
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(geometry);
    shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    GLRenderTarget::pushRenderTarget(target);
    outputTexture->bind();
    outputTexture->render(geometry, geometry, true);
    outputTexture->unbind();
    GLRenderTarget::popRenderTarget();
}

} // namespace KWin
