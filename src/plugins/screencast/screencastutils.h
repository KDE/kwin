/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "opengl/glplatform.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"

namespace KWin
{

static GLenum closestGLType(QImage::Format format)
{
    switch (format) {
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGB32:
        return GL_BGRA;
    default:
        qDebug() << "unknown format" << format;
        return GL_RGBA;
    }
}

static void grabTexture(GLTexture *texture, QImage *target)
{
    const QSize size = texture->size();
    EglContext *context = EglContext::currentContext();

    texture->bind();
    GLFramebuffer fbo(texture);
    context->pushFramebuffer(&fbo);
    context->glReadnPixels(0, 0, size.width(), size.height(), closestGLType(target->format()), GL_UNSIGNED_BYTE, target->sizeInBytes(), target->bits());
    context->popFramebuffer();
}

static inline Region scaleRegion(const Region &region, qreal scale, const Rect &bounds)
{
    return region.scaledAndRoundedOut(scale) & bounds;
}

} // namespace KWin
