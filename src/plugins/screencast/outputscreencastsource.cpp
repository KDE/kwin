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
#include "renderloop.h"
#include "renderoutput.h"
#include "scene.h"

#include <QPainter>

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
    const auto textures = getTextures(m_output);
    if (textures.isEmpty()) {
        return;
    }
    if (textures.size() == 1) {
        const auto outputTexture = textures.values().constFirst();
        if (outputTexture) {
            grabTexture(outputTexture.data(), image);
        }
    } else {
        for (auto it = textures.constBegin(); it != textures.constEnd(); it++) {
            const auto output = it.key();
            const auto outputTexture = it.value();
            QImage tmp(output->pixelSize(), image->format());
            if (outputTexture) {
                grabTexture(outputTexture.data(), &tmp);
                QPainter painter;
                painter.drawImage(output->relativePixelGeometry(), tmp, QRect(QPoint(), tmp.size()));
            }
        }
    }
}

void OutputScreenCastSource::render(GLRenderTarget *target)
{
    const auto textures = getTextures(m_output);
    if (textures.isEmpty()) {
        return;
    }

    for (auto it = textures.constBegin(); it != textures.constEnd(); it++) {
        const auto output = it.key();
        const auto outputTexture = it.value();
        const QRect geometry = output->relativePixelGeometry();

        ShaderBinder shaderBinder(ShaderTrait::MapTexture);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(geometry);
        shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

        GLRenderTarget::pushRenderTarget(target);
        outputTexture->bind();
        outputTexture->render(geometry);
        outputTexture->unbind();
        GLRenderTarget::popRenderTarget();
    }
}

std::chrono::nanoseconds OutputScreenCastSource::clock() const
{
    return m_output->renderLoop()->lastPresentationTimestamp();
}

} // namespace KWin
