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

#include "opengl/glplatform.h"

#include "compositor.h"
#include "core/output.h"
#include "decorations/decoratedwindow.h"
#include "scene/itemrenderer_opengl.h"
#include "shadow.h"
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
    : WorkspaceScene(std::make_unique<ItemRendererOpenGL>(backend->eglDisplayObject()))
    , m_backend(backend)
{
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

OpenGlContext *WorkspaceSceneOpenGL::openglContext() const
{
    return m_backend->openglContext();
}

std::unique_ptr<DecorationRenderer> WorkspaceSceneOpenGL::createDecorationRenderer(Decoration::DecoratedWindowImpl *impl)
{
    return std::make_unique<SceneOpenGLDecorationRenderer>(impl);
}

std::unique_ptr<ShadowTextureProvider> WorkspaceSceneOpenGL::createShadowTextureProvider(Shadow *shadow)
{
    return std::make_unique<OpenGLShadowTextureProvider>(shadow);
}

bool WorkspaceSceneOpenGL::animationsSupported() const
{
    const auto context = openglContext();
    return context && !context->isSoftwareRenderer();
}

std::pair<std::shared_ptr<GLTexture>, ColorDescription> WorkspaceSceneOpenGL::textureForOutput(Output *output) const
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

    void unregister(ShadowTextureProvider *provider);
    std::shared_ptr<GLTexture> getTexture(ShadowTextureProvider *provider);

private:
    DecorationShadowTextureCache() = default;
    struct Data
    {
        std::shared_ptr<GLTexture> texture;
        QList<ShadowTextureProvider *> providers;
    };
    QHash<KDecoration3::DecorationShadow *, Data> m_cache;
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

void DecorationShadowTextureCache::unregister(ShadowTextureProvider *provider)
{
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        auto &d = it.value();
        // check whether the Vector of Shadows contains our shadow and remove all of them
        auto glIt = d.providers.begin();
        while (glIt != d.providers.end()) {
            if (*glIt == provider) {
                glIt = d.providers.erase(glIt);
            } else {
                glIt++;
            }
        }
        // if there are no shadows any more we can erase the cache entry
        if (d.providers.isEmpty()) {
            it = m_cache.erase(it);
        } else {
            it++;
        }
    }
}

std::shared_ptr<GLTexture> DecorationShadowTextureCache::getTexture(ShadowTextureProvider *provider)
{
    Shadow *shadow = provider->shadow();
    Q_ASSERT(shadow->hasDecorationShadow());
    unregister(provider);
    const auto decoShadow = shadow->decorationShadow().lock();
    Q_ASSERT(decoShadow);
    auto it = m_cache.find(decoShadow.get());
    if (it != m_cache.end()) {
        Q_ASSERT(!it.value().providers.contains(provider));
        it.value().providers << provider;
        return it.value().texture;
    }
    Data d;
    d.providers << provider;
    d.texture = GLTexture::upload(shadow->decorationShadowImage());
    if (!d.texture) {
        return nullptr;
    }
    d.texture->setFilter(GL_LINEAR);
    d.texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_cache.insert(decoShadow.get(), d);
    return d.texture;
}

OpenGLShadowTextureProvider::OpenGLShadowTextureProvider(Shadow *shadow)
    : ShadowTextureProvider(shadow)
{
}

OpenGLShadowTextureProvider::~OpenGLShadowTextureProvider()
{
    if (m_texture) {
        Compositor::self()->scene()->makeOpenGLContextCurrent();
        DecorationShadowTextureCache::instance().unregister(this);
        m_texture.reset();
    }
}

void OpenGLShadowTextureProvider::update()
{
    if (m_shadow->hasDecorationShadow()) {
        // simplifies a lot by going directly to
        m_texture = DecorationShadowTextureCache::instance().getTexture(this);
        return;
    }

    const QSize top(m_shadow->shadowElement(Shadow::ShadowElementTop).size());
    const QSize topRight(m_shadow->shadowElement(Shadow::ShadowElementTopRight).size());
    const QSize right(m_shadow->shadowElement(Shadow::ShadowElementRight).size());
    const QSize bottom(m_shadow->shadowElement(Shadow::ShadowElementBottom).size());
    const QSize bottomLeft(m_shadow->shadowElement(Shadow::ShadowElementBottomLeft).size());
    const QSize left(m_shadow->shadowElement(Shadow::ShadowElementLeft).size());
    const QSize topLeft(m_shadow->shadowElement(Shadow::ShadowElementTopLeft).size());
    const QSize bottomRight(m_shadow->shadowElement(Shadow::ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return;
    }

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()), m_shadow->shadowElement(Shadow::ShadowElementTopLeft));
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()), m_shadow->shadowElement(Shadow::ShadowElementTop));
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()), m_shadow->shadowElement(Shadow::ShadowElementTopRight));

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()), m_shadow->shadowElement(Shadow::ShadowElementLeft));
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()), m_shadow->shadowElement(Shadow::ShadowElementRight));

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()), m_shadow->shadowElement(Shadow::ShadowElementBottomLeft));
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()), m_shadow->shadowElement(Shadow::ShadowElementBottom));
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height()), m_shadow->shadowElement(Shadow::ShadowElementBottomRight));

    p.end();

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    const auto context = OpenGlContext::currentContext();
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

    m_texture = GLTexture::upload(image);
    if (!m_texture) {
        return;
    }
    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);

    if (m_texture->internalFormat() == GL_R8) {
        // Swizzle red to alpha and all other channels to zero
        m_texture->bind();
        m_texture->setSwizzle(GL_ZERO, GL_ZERO, GL_ZERO, GL_RED);
    }
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedWindowImpl *client)
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
    const int topHeight = std::round(top.height() * devicePixelRatio);
    const int bottomHeight = std::round(bottom.height() * devicePixelRatio);
    const int leftWidth = std::round(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * TexturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * TexturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * TexturePad));

    const QRect dirtyRect = region.boundingRect();

    renderPart(top.intersected(dirtyRect), top, topPosition, devicePixelRatio);
    renderPart(bottom.intersected(dirtyRect), bottom, bottomPosition, devicePixelRatio);
    renderPart(left.intersected(dirtyRect), left, leftPosition, devicePixelRatio, true);
    renderPart(right.intersected(dirtyRect), right, rightPosition, devicePixelRatio, true);
}

void SceneOpenGLDecorationRenderer::renderPart(const QRectF &rect, const QRectF &partRect,
                                               const QPoint &textureOffset,
                                               qreal devicePixelRatio, bool rotated)
{
    if (!rect.isValid() || !m_texture) {
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

    QPoint dirtyOffset = ((rect.topLeft() - partRect.topLeft()) * devicePixelRatio).toPoint();
    if (padding.top() == 0) {
        dirtyOffset.ry() += TexturePad;
    }
    if (padding.left() == 0) {
        dirtyOffset.rx() += TexturePad;
    }
    m_texture->update(image, image.rect(), textureOffset + dirtyOffset);
}

const QMargins SceneOpenGLDecorationRenderer::texturePadForPart(
    const QRectF &rect, const QRectF &partRect)
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

    size.rwidth() = toNativeSize(std::max({top.width(), bottom.width(), left.height(), right.height()}));
    size.rheight() = toNativeSize(top.height()) + toNativeSize(bottom.height()) + toNativeSize(left.width()) + toNativeSize(right.width());

    size.rheight() += 4 * (2 * TexturePad);
    size.rwidth() += 2 * TexturePad;
    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size) {
        return;
    }

    if (!size.isEmpty()) {
        m_texture = GLTexture::allocate(GL_RGBA8, size);
        if (!m_texture) {
            return;
        }
        m_texture->setContentTransform(OutputTransform::FlipY);
        m_texture->setFilter(GL_LINEAR);
        m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    } else {
        m_texture.reset();
    }
}

int SceneOpenGLDecorationRenderer::toNativeSize(double size) const
{
    return std::round(size * effectiveDevicePixelRatio());
}

} // namespace

#include "moc_workspacescene_opengl.cpp"
