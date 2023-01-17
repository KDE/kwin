/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2009, 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    Based on glcompmgr code by Felix Bellaby.
    Using code from Compiz and Beryl.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "workspacescene_opengl.h"

#include <kwinglplatform.h>

#include "composite.h"
#include "core/output.h"
#include "decorations/decoratedclient.h"
#include "scene/itemrenderer_opengl.h"
#include "window.h"

#include <cmath>
#include <cstddef>

#include <QMatrix4x4>
#include <QPainter>
#include <QStringList>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>

namespace KWin
{

/************************************************
 * SceneOpenGL
 ***********************************************/

WorkspaceSceneOpenGL::WorkspaceSceneOpenGL(OpenGLBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererOpenGL>())
    , m_backend(backend)
{
    // It is not legal to not have a vertex array object bound in a core context
    if (!GLPlatform::instance()->isGLES() && hasGLExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
    }
}

WorkspaceSceneOpenGL::~WorkspaceSceneOpenGL()
{
    makeOpenGLContextCurrent();
}

bool WorkspaceSceneOpenGL::makeOpenGLContextCurrent()
{
    return m_backend->makeCurrent();
}

void WorkspaceSceneOpenGL::doneOpenGLContextCurrent()
{
    m_backend->doneCurrent();
}

bool WorkspaceSceneOpenGL::supportsNativeFence() const
{
    return m_backend->supportsNativeFence();
}

std::unique_ptr<Shadow> WorkspaceSceneOpenGL::createShadow(Window *window)
{
    return std::make_unique<SceneOpenGLShadow>(window);
}

DecorationRenderer *WorkspaceSceneOpenGL::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneOpenGLDecorationRenderer(impl);
}

bool WorkspaceSceneOpenGL::animationsSupported() const
{
    return !GLPlatform::instance()->isSoftwareEmulation();
}

std::shared_ptr<GLTexture> WorkspaceSceneOpenGL::textureForOutput(Output *output) const
{
    return m_backend->textureForOutput(output);
}

//****************************************
// SceneOpenGL::Shadow
//****************************************
class DecorationShadowTextureCache
{
public:
    ~DecorationShadowTextureCache();
    DecorationShadowTextureCache(const DecorationShadowTextureCache &) = delete;
    static DecorationShadowTextureCache &instance();

    void unregister(SceneOpenGLShadow *shadow);
    std::shared_ptr<GLTexture> getTexture(SceneOpenGLShadow *shadow);

private:
    DecorationShadowTextureCache() = default;
    struct Data
    {
        std::shared_ptr<GLTexture> texture;
        QVector<SceneOpenGLShadow *> shadows;
    };
    QHash<KDecoration2::DecorationShadow *, Data> m_cache;
};

DecorationShadowTextureCache &DecorationShadowTextureCache::instance()
{
    static DecorationShadowTextureCache s_instance;
    return s_instance;
}

DecorationShadowTextureCache::~DecorationShadowTextureCache()
{
    Q_ASSERT(m_cache.isEmpty());
}

void DecorationShadowTextureCache::unregister(SceneOpenGLShadow *shadow)
{
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        auto &d = it.value();
        // check whether the Vector of Shadows contains our shadow and remove all of them
        auto glIt = d.shadows.begin();
        while (glIt != d.shadows.end()) {
            if (*glIt == shadow) {
                glIt = d.shadows.erase(glIt);
            } else {
                glIt++;
            }
        }
        // if there are no shadows any more we can erase the cache entry
        if (d.shadows.isEmpty()) {
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
}

std::shared_ptr<GLTexture> DecorationShadowTextureCache::getTexture(SceneOpenGLShadow *shadow)
{
    Q_ASSERT(shadow->hasDecorationShadow());
    unregister(shadow);
    const auto &decoShadow = shadow->decorationShadow().toStrongRef();
    Q_ASSERT(!decoShadow.isNull());
    auto it = m_cache.find(decoShadow.data());
    if (it != m_cache.end()) {
        Q_ASSERT(!it.value().shadows.contains(shadow));
        it.value().shadows << shadow;
        return it.value().texture;
    }
    Data d;
    d.shadows << shadow;
    d.texture = std::make_shared<GLTexture>(shadow->decorationShadowImage());
    m_cache.insert(decoShadow.data(), d);
    return d.texture;
}

SceneOpenGLShadow::SceneOpenGLShadow(Window *window)
    : Shadow(window)
{
}

SceneOpenGLShadow::~SceneOpenGLShadow()
{
    WorkspaceScene *scene = Compositor::self()->scene();
    if (scene) {
        scene->makeOpenGLContextCurrent();
        DecorationShadowTextureCache::instance().unregister(this);
        m_texture.reset();
    }
}

bool SceneOpenGLShadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        // simplifies a lot by going directly to
        WorkspaceScene *scene = Compositor::self()->scene();
        scene->makeOpenGLContextCurrent();
        m_texture = DecorationShadowTextureCache::instance().getTexture(this);

        return true;
    }
    const QSize top(shadowElement(ShadowElementTop).size());
    const QSize topRight(shadowElement(ShadowElementTopRight).size());
    const QSize right(shadowElement(ShadowElementRight).size());
    const QSize bottom(shadowElement(ShadowElementBottom).size());
    const QSize bottomLeft(shadowElement(ShadowElementBottomLeft).size());
    const QSize left(shadowElement(ShadowElementLeft).size());
    const QSize topLeft(shadowElement(ShadowElementTopLeft).size());
    const QSize bottomRight(shadowElement(ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()), shadowElement(ShadowElementTopLeft));
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()), shadowElement(ShadowElementTop));
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()), shadowElement(ShadowElementTopRight));

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()), shadowElement(ShadowElementLeft));
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()), shadowElement(ShadowElementRight));

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()), shadowElement(ShadowElementBottomLeft));
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()), shadowElement(ShadowElementBottom));
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height()), shadowElement(ShadowElementBottomRight));

    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    if (!GLPlatform::instance()->isGLES() && GLTexture::supportsSwizzle() && GLTexture::supportsFormatRG()) {
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

    WorkspaceScene *scene = Compositor::self()->scene();
    scene->makeOpenGLContextCurrent();
    m_texture = std::make_shared<GLTexture>(image);

    if (m_texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        m_texture->bind();
        m_texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }

    return true;
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : DecorationRenderer(client)
    , m_texture()
{
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer()
{
    if (WorkspaceScene *scene = Compositor::self()->scene()) {
        scene->makeOpenGLContextCurrent();
    }
}

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const QRect &viewport)
{
    Q_ASSERT(image.depth() == 32);
    if (viewport.isEmpty()) {
        image = {};
        return;
    }

    const QRect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
}

void SceneOpenGLDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeTexture();
        resetImageSizesDirty();
    }

    if (!m_texture) {
        // for invalid sizes we get no texture, see BUG 361551
        return;
    }

    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    const qreal devicePixelRatio = effectiveDevicePixelRatio();
    const int topHeight = std::ceil(top.height() * devicePixelRatio);
    const int bottomHeight = std::ceil(bottom.height() * devicePixelRatio);
    const int leftWidth = std::ceil(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * TexturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * TexturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * TexturePad));

    const QRect dirtyRect = region.boundingRect();

    renderPart(top.toRect().intersected(dirtyRect), top.toRect(), topPosition, devicePixelRatio);
    renderPart(bottom.toRect().intersected(dirtyRect), bottom.toRect(), bottomPosition, devicePixelRatio);
    renderPart(left.toRect().intersected(dirtyRect), left.toRect(), leftPosition, devicePixelRatio, true);
    renderPart(right.toRect().intersected(dirtyRect), right.toRect(), rightPosition, devicePixelRatio, true);
}

void SceneOpenGLDecorationRenderer::renderPart(const QRect &rect, const QRect &partRect,
                                               const QPoint &textureOffset,
                                               qreal devicePixelRatio, bool rotated)
{
    if (!rect.isValid()) {
        return;
    }
    // We allow partial decoration updates and it might just so happen that the
    // dirty region is completely contained inside the decoration part, i.e.
    // the dirty region doesn't touch any of the decoration's edges. In that
    // case, we should **not** pad the dirty region.
    const QMargins padding = texturePadForPart(rect, partRect);
    int verticalPadding = padding.top() + padding.bottom();
    int horizontalPadding = padding.left() + padding.right();

    QSize imageSize(toNativeSize(rect.width()), toNativeSize(rect.height()));
    if (rotated) {
        imageSize = QSize(imageSize.height(), imageSize.width());
    }
    QSize paddedImageSize = imageSize;
    paddedImageSize.rheight() += verticalPadding;
    paddedImageSize.rwidth() += horizontalPadding;
    QImage image(paddedImageSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);

    QRect padClip = QRect(padding.left(), padding.top(), imageSize.width(), imageSize.height());
    QPainter painter(&image);
    const qreal inverseScale = 1.0 / devicePixelRatio;
    painter.scale(inverseScale, inverseScale);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(padClip);
    painter.translate(padding.left(), padding.top());
    if (rotated) {
        painter.translate(0, imageSize.height());
        painter.rotate(-90);
    }
    painter.scale(devicePixelRatio, devicePixelRatio);
    painter.translate(-rect.topLeft());
    renderToPainter(&painter, rect);
    painter.end();

    // fill padding pixels by copying from the neighbour row
    clamp(image, padClip);

    QPoint dirtyOffset = (rect.topLeft() - partRect.topLeft()) * devicePixelRatio;
    if (padding.top() == 0) {
        dirtyOffset.ry() += TexturePad;
    }
    if (padding.left() == 0) {
        dirtyOffset.rx() += TexturePad;
    }
    m_texture->update(image, textureOffset + dirtyOffset);
}

const QMargins SceneOpenGLDecorationRenderer::texturePadForPart(
    const QRect &rect, const QRect &partRect)
{
    QMargins result = QMargins(0, 0, 0, 0);
    if (rect.top() == partRect.top()) {
        result.setTop(TexturePad);
    }
    if (rect.bottom() == partRect.bottom()) {
        result.setBottom(TexturePad);
    }
    if (rect.left() == partRect.left()) {
        result.setLeft(TexturePad);
    }
    if (rect.right() == partRect.right()) {
        result.setRight(TexturePad);
    }
    return result;
}

static int align(int value, int align)
{
    return (value + align - 1) & ~(align - 1);
}

void SceneOpenGLDecorationRenderer::resizeTexture()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);
    QSize size;

    size.rwidth() = toNativeSize(std::max(std::max(top.width(), bottom.width()),
                                          std::max(left.height(), right.height())));
    size.rheight() = toNativeSize(top.height()) + toNativeSize(bottom.height()) + toNativeSize(left.width()) + toNativeSize(right.width());

    size.rheight() += 4 * (2 * TexturePad);
    size.rwidth() += 2 * TexturePad;
    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size) {
        return;
    }

    if (!size.isEmpty()) {
        m_texture.reset(new GLTexture(GL_RGBA8, size.width(), size.height()));
        m_texture->setYInverted(true);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_texture->clear();
    } else {
        m_texture.reset();
    }
}

int SceneOpenGLDecorationRenderer::toNativeSize(int size) const
{
    return std::ceil(size * effectiveDevicePixelRatio());
}

} // namespace
