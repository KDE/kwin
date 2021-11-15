/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kwinglplatform.h"
#include "kwingltexture.h"

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

static void grabTexture(GLTexture *texture, QImage *image)
{
    Q_ASSERT(texture->size() == image->size());
    const bool invertNeededAndSupported = texture->isYInverted() && GLPlatform::instance()->supports(PackInvert);
    GLboolean prev;
    if (invertNeededAndSupported) {
        glGetBooleanv(GL_PACK_INVERT_MESA, &prev);
        glPixelStorei(GL_PACK_INVERT_MESA, GL_TRUE);
    }

    texture->bind();
    if (GLPlatform::instance()->isGLES()) {
        glReadPixels(0, 0, image->width(), image->height(), image->hasAlphaChannel() ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE, (GLvoid*)image->bits());
    } else if (GLPlatform::instance()->glVersion() >= kVersionNumber(4, 5)) {
        glGetTextureImage(texture->texture(), 0, image->hasAlphaChannel() ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE, image->sizeInBytes(), image->bits());
    } else {
        glGetTexImage(texture->target(), 0, image->hasAlphaChannel() ? GL_BGRA : GL_BGR, GL_UNSIGNED_BYTE, image->bits());
    }

    if (invertNeededAndSupported) {
        if (!prev) {
            glPixelStorei(GL_PACK_INVERT_MESA, prev);
        }
    } else if (texture->isYInverted()) {
        mirrorVertically(image->bits(), image->height(), image->bytesPerLine());
    }
}

} // namespace KWin
