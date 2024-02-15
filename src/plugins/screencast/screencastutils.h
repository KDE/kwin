/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "opengl/glplatform.h"
#include "opengl/gltexture.h"
#include "opengl/glutils.h"
#include <spa/buffer/buffer.h>
#include <spa/param/video/raw.h>

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

static GLenum closestGLType(spa_video_format format)
{
    switch (format) {
    case SPA_VIDEO_FORMAT_RGB:
        return GL_RGB;
    case SPA_VIDEO_FORMAT_BGR:
        return GL_BGR;
    case SPA_VIDEO_FORMAT_RGBx:
    case SPA_VIDEO_FORMAT_RGBA:
        return GL_RGBA;
    case SPA_VIDEO_FORMAT_BGRA:
    case SPA_VIDEO_FORMAT_BGRx:
        return GL_BGRA;
    default:
        qDebug() << "unknown format" << format;
        return GL_RGBA;
    }
}

static void doGrabTexture(GLTexture *texture, spa_data *spa, spa_video_format format)
{
    const QSize size = texture->size();
    const bool invertNeeded = GLPlatform::instance()->isGLES() ^ (texture->contentTransform() != OutputTransform::FlipY);
    const bool invertNeededAndSupported = invertNeeded && GLPlatform::instance()->supports(GLFeature::PackInvert);
    GLboolean prev;
    if (invertNeededAndSupported) {
        glGetBooleanv(GL_PACK_INVERT_MESA, &prev);
        glPixelStorei(GL_PACK_INVERT_MESA, GL_TRUE);
    }

    texture->bind();
    // BUG: The nvidia driver fails to glGetTexImage
    // Drop driver() == DriverNVidia some time after that's fixed
    if (GLPlatform::instance()->isGLES() || GLPlatform::instance()->driver() == Driver_NVidia) {
        GLFramebuffer fbo(texture);
        GLFramebuffer::pushFramebuffer(&fbo);
        glReadPixels(0, 0, size.width(), size.height(), closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
        GLFramebuffer::popFramebuffer();
    } else if (GLPlatform::instance()->glVersion() >= Version(4, 5)) {
        glGetTextureImage(texture->texture(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->chunk->size, spa->data);
    } else {
        glGetTexImage(texture->target(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
    }

    if (invertNeededAndSupported) {
        if (!prev) {
            glPixelStorei(GL_PACK_INVERT_MESA, prev);
        }
    } else if (invertNeeded) {
        mirrorVertically(static_cast<uchar *>(spa->data), size.height(), spa->chunk->stride);
    }
}

static void grabTexture(GLTexture *texture, spa_data *spa, spa_video_format format)
{
    const OutputTransform contentTransform = texture->contentTransform();
    if (contentTransform == OutputTransform::Normal || contentTransform == OutputTransform::FlipY) {
        doGrabTexture(texture, spa, format);
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
        doGrabTexture(backingTexture.get(), spa, format);
    }
}

} // namespace KWin
