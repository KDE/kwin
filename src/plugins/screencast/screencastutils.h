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

// in-place vertical mirroring
static void mirrorVertically(uchar *data, int height, int stride)
{
    const int halfHeight = height / 2;
    std::vector<uchar> temp(stride);
    for (int y = 0; y < halfHeight; ++y) {
        auto cur = &data[y * stride], dest = &data[(height - y - 1) * stride];
        memcpy(temp.data(), cur, stride);
        memcpy(cur, dest, stride);
        memcpy(dest, temp.data(), stride);
    }
}

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

static void doGrabTexture(GLTexture *texture, QImage *target)
{
    if (texture->size() != target->size()) {
        return;
    }

    const auto context = EglContext::currentContext();
    const QSize size = texture->size();
    const bool invertNeeded = context->isOpenGLES() ^ (texture->contentTransform() != OutputTransform::FlipY);
    const bool invertNeededAndSupported = invertNeeded && context->supportsPackInvert();
    GLboolean prev;
    if (invertNeededAndSupported) {
        glGetBooleanv(GL_PACK_INVERT_MESA, &prev);
        glPixelStorei(GL_PACK_INVERT_MESA, GL_TRUE);
    }

    texture->bind();
    // BUG: The nvidia driver fails to glGetTexImage
    // Drop driver() == DriverNVidia some time after that's fixed
    if (context->isOpenGLES() || context->glPlatform()->driver() == Driver_NVidia) {
        GLFramebuffer fbo(texture);
        GLFramebuffer::pushFramebuffer(&fbo);
        context->glReadnPixels(0, 0, size.width(), size.height(), closestGLType(target->format()), GL_UNSIGNED_BYTE, target->sizeInBytes(), target->bits());
        GLFramebuffer::popFramebuffer();
    } else {
        context->glGetnTexImage(texture->target(), 0, closestGLType(target->format()), GL_UNSIGNED_BYTE, target->sizeInBytes(), target->bits());
    }

    if (invertNeededAndSupported) {
        if (!prev) {
            glPixelStorei(GL_PACK_INVERT_MESA, prev);
        }
    } else if (invertNeeded) {
        mirrorVertically(static_cast<uchar *>(target->bits()), size.height(), target->bytesPerLine());
    }
}

static void grabTexture(GLTexture *texture, QImage *target)
{
    const OutputTransform contentTransform = texture->contentTransform();
    if (contentTransform == OutputTransform::Normal || contentTransform == OutputTransform::FlipY) {
        doGrabTexture(texture, target);
    } else {
        const QSize size = contentTransform.map(texture->size());
        const auto backingTexture = GLTexture::allocate(GL_RGBA8, size);
        if (!backingTexture) {
            return;
        }
        backingTexture->setContentTransform(OutputTransform::FlipY);

        ShaderBinder shaderBinder(ShaderTrait::MapTexture);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.scale(1, -1);
        projectionMatrix.ortho(QRect(QPoint(), size));
        shaderBinder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projectionMatrix);

        GLFramebuffer fbo(backingTexture.get());
        GLFramebuffer::pushFramebuffer(&fbo);
        texture->render(size);
        GLFramebuffer::popFramebuffer();
        doGrabTexture(backingTexture.get(), target);
    }
}

static inline QRegion scaleRegion(const QRegion &_region, qreal scale)
{
    if (scale == 1.) {
        return _region;
    }

    QRegion region;
    for (auto it = _region.begin(), itEnd = _region.end(); it != itEnd; ++it) {
        region += QRect(std::floor(it->x() * scale),
                        std::floor(it->y() * scale),
                        std::ceil(it->width() * scale),
                        std::ceil(it->height() * scale));
    }

    return region;
}

} // namespace KWin
