/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/opengl/ninepatch.h"
#include "main.h"
#include "opengl/eglcontext.h"
#include "opengl/gltexture.h"
#include "scene/workspacescene.h"

#include <QPainter>

namespace KWin
{

std::unique_ptr<NinePatchOpenGL> NinePatchOpenGL::create(const QImage &image)
{
    auto texture = GLTexture::upload(image);
    if (!texture) {
        return nullptr;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);

    return std::make_unique<NinePatchOpenGL>(std::move(texture));
}

std::unique_ptr<NinePatchOpenGL> NinePatchOpenGL::create(const QImage &topLeftPatch,
                                                         const QImage &topPatch,
                                                         const QImage &topRightPatch,
                                                         const QImage &rightPatch,
                                                         const QImage &bottomRightPatch,
                                                         const QImage &bottomPatch,
                                                         const QImage &bottomLeftPatch,
                                                         const QImage &leftPatch)
{
    const QSize top(topPatch.size());
    const QSize topRight(topRightPatch.size());
    const QSize right(rightPatch.size());
    const QSize bottom(bottomPatch.size());
    const QSize bottomLeft(bottomLeftPatch.size());
    const QSize left(leftPatch.size());
    const QSize topLeft(topLeftPatch.size());
    const QSize bottomRight(bottomRightPatch.size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return nullptr;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()), topLeftPatch);
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()), topPatch);
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()), topRightPatch);

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()), leftPatch);
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()), rightPatch);

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()), bottomLeftPatch);
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()), bottomPatch);
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height()), bottomRightPatch);

    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    const auto context = EglContext::currentContext();
    if (!context->isOpenGLES() && context->supportsTextureSwizzle() && context->supportsRGTextures()) {
        QImage alphaImage(image.size(), QImage::Format_Alpha8);
        bool alphaOnly = true;

        for (ptrdiff_t y = 0; alphaOnly && y < image.height(); y++) {
            const uint32_t *const src = reinterpret_cast<const uint32_t *>(image.scanLine(y));
            uint8_t *const dst = reinterpret_cast<uint8_t *>(alphaImage.scanLine(y));

            for (ptrdiff_t x = 0; x < image.width(); x++) {
                if (src[x] & 0x00ffffff) {
                    alphaOnly = false;
                }

                dst[x] = qAlpha(src[x]);
            }
        }

        if (alphaOnly) {
            image = alphaImage;
        }
    }

    auto texture = GLTexture::upload(image);
    if (!texture) {
        return nullptr;
    }

    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);

    if (texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        texture->bind();
        texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }

    return std::make_unique<NinePatchOpenGL>(std::move(texture));
}

NinePatchOpenGL::NinePatchOpenGL(std::unique_ptr<GLTexture> &&texture)
    : m_texture(std::move(texture))
{
}

NinePatchOpenGL::~NinePatchOpenGL()
{
    // FIXME: It should not be attached to the workspace scene.
    kwinApp()->scene()->openglContext()->makeCurrent();
}

GLTexture *NinePatchOpenGL::texture() const
{
    return m_texture.get();
}

} // namespace KWin
