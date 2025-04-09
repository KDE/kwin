/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/decorationitem.h"
#include "compositor.h"
#include "core/output.h"
#include "core/renderbackend.h"
#include "decorations/decoratedwindow.h"
#include "opengl/eglcontext.h"
#include "opengl/gltexture.h"
#include "scene/workspacescene.h"
#include "window.h"

#include <cmath>

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>

#include <QPainter>

namespace KWin
{

DecorationRenderer::DecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : m_client(client)
    , m_imageSizesDirty(true)
{
    connect(client->decoration(), &KDecoration3::Decoration::damaged,
            this, &DecorationRenderer::addDamage);

    connect(client->decoration(), &KDecoration3::Decoration::bordersChanged,
            this, &DecorationRenderer::invalidate);
    connect(client->decoratedWindow(), &KDecoration3::DecoratedWindow::sizeChanged,
            this, &DecorationRenderer::invalidate);

    invalidate();
}

Decoration::DecoratedWindowImpl *DecorationRenderer::client() const
{
    return m_client;
}

void DecorationRenderer::invalidate()
{
    if (m_client) {
        addDamage(m_client->window()->rect().toAlignedRect());
    }
    m_imageSizesDirty = true;
}

QRegion DecorationRenderer::damage() const
{
    return m_damage;
}

void DecorationRenderer::addDamage(const QRegion &region)
{
    m_damage += region;
    Q_EMIT damaged(region);
}

void DecorationRenderer::resetDamage()
{
    m_damage = QRegion();
}

qreal DecorationRenderer::effectiveDevicePixelRatio() const
{
    // QPainter won't let paint with a device pixel ratio less than 1.
    return std::max(qreal(1.0), devicePixelRatio());
}

qreal DecorationRenderer::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

void DecorationRenderer::setDevicePixelRatio(qreal dpr)
{
    if (m_devicePixelRatio != dpr) {
        m_devicePixelRatio = dpr;
        invalidate();
    }
}

void DecorationRenderer::renderToPainter(QPainter *painter, const QRectF &rect)
{
    client()->decoration()->paint(painter, rect);
}

SceneOpenGLDecorationRenderer::SceneOpenGLDecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : DecorationRenderer(client)
    , m_texture()
{
}

SceneOpenGLDecorationRenderer::~SceneOpenGLDecorationRenderer()
{
    if (WorkspaceScene *scene = Compositor::self()->scene()) {
        scene->openglContext()->makeCurrent();
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

SceneQPainterDecorationRenderer::SceneQPainterDecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : DecorationRenderer(client)
{
}

QImage SceneQPainterDecorationRenderer::image(SceneQPainterDecorationRenderer::DecorationPart part) const
{
    Q_ASSERT(part != DecorationPart::Count);
    return m_images[int(part)];
}

void SceneQPainterDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeImages();
        resetImageSizesDirty();
    }

    auto imageSize = [this](DecorationPart part) {
        return m_images[int(part)].size() / m_images[int(part)].devicePixelRatio();
    };

    const QRect top(QPoint(0, 0), imageSize(DecorationPart::Top));
    const QRect left(QPoint(0, top.height()), imageSize(DecorationPart::Left));
    const QRect right(QPoint(top.width() - imageSize(DecorationPart::Right).width(), top.height()), imageSize(DecorationPart::Right));
    const QRect bottom(QPoint(0, left.y() + left.height()), imageSize(DecorationPart::Bottom));

    const QRect geometry = region.boundingRect();
    auto renderPart = [this](const QRect &rect, const QRect &partRect, int index) {
        if (rect.isEmpty()) {
            return;
        }
        QPainter painter(&m_images[index]);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setWindow(QRect(partRect.topLeft(), partRect.size() * effectiveDevicePixelRatio()));
        painter.setClipRect(rect);
        painter.save();
        // clear existing part
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, Qt::transparent);
        painter.restore();
        client()->decoration()->paint(&painter, rect);
    };

    renderPart(left.intersected(geometry), left, int(DecorationPart::Left));
    renderPart(top.intersected(geometry), top, int(DecorationPart::Top));
    renderPart(right.intersected(geometry), right, int(DecorationPart::Right));
    renderPart(bottom.intersected(geometry), bottom, int(DecorationPart::Bottom));
}

void SceneQPainterDecorationRenderer::resizeImages()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    auto checkAndCreate = [this](int index, const QSizeF &size) {
        auto dpr = effectiveDevicePixelRatio();
        if (m_images[index].size() != size * dpr || m_images[index].devicePixelRatio() != dpr) {
            m_images[index] = QImage(size.toSize() * dpr, QImage::Format_ARGB32_Premultiplied);
            m_images[index].setDevicePixelRatio(dpr);
            m_images[index].fill(Qt::transparent);
        }
    };
    checkAndCreate(int(DecorationPart::Left), left.size());
    checkAndCreate(int(DecorationPart::Right), right.size());
    checkAndCreate(int(DecorationPart::Top), top.size());
    checkAndCreate(int(DecorationPart::Bottom), bottom.size());
}

DecorationItem::DecorationItem(KDecoration3::Decoration *decoration, Window *window, Item *parent)
    : Item(parent)
    , m_window(window)
    , m_decoration(decoration)
{
    switch (Compositor::self()->backend()->compositingType()) {
    case OpenGLCompositing:
        m_renderer = std::make_unique<SceneOpenGLDecorationRenderer>(window->decoratedWindow());
        break;
    case QPainterCompositing:
        m_renderer = std::make_unique<SceneQPainterDecorationRenderer>(window->decoratedWindow());
        break;
    default:
        Q_UNREACHABLE();
    }

    connect(window, &Window::targetScaleChanged, this, &DecorationItem::updateScale);

    connect(decoration->window(), &KDecoration3::DecoratedWindow::sizeChanged,
            this, &DecorationItem::handleDecorationGeometryChanged);
    connect(decoration, &KDecoration3::Decoration::bordersChanged,
            this, &DecorationItem::handleDecorationGeometryChanged);

    connect(renderer(), &DecorationRenderer::damaged,
            this, qOverload<const QRegion &>(&Item::scheduleRepaint));

    setSize(decoration->size());
    updateScale();
}

QList<QRectF> DecorationItem::shape() const
{
    QRectF left, top, right, bottom;
    m_window->layoutDecorationRects(left, top, right, bottom);
    return {left, top, right, bottom};
}

QRegion DecorationItem::opaque() const
{
    if (m_window->decorationHasAlpha()) {
        return QRegion();
    }
    QRectF left, top, right, bottom;
    m_window->layoutDecorationRects(left, top, right, bottom);

    // We have to map to integers which has rounding issues
    // it's safer for a region to be considered transparent than opaque
    // so always align inwards
    const QMargins roundingPad = QMargins(1, 1, 1, 1);
    QRegion roundedLeft = left.toAlignedRect().marginsRemoved(roundingPad);
    QRegion roundedTop = top.toAlignedRect().marginsRemoved(roundingPad);
    QRegion roundedRight = right.toAlignedRect().marginsRemoved(roundingPad);
    QRegion roundedBottom = bottom.toAlignedRect().marginsRemoved(roundingPad);

    return roundedLeft | roundedTop | roundedRight | roundedBottom;
}

void DecorationItem::preprocess()
{
    const QRegion damage = m_renderer->damage();
    if (!damage.isEmpty()) {
        m_renderer->render(damage);
        m_renderer->resetDamage();
    }
}

void DecorationItem::updateScale()
{
    const double scale = m_window->targetScale();
    if (m_renderer->devicePixelRatio() != scale) {
        m_renderer->setDevicePixelRatio(scale);
        discardQuads();
    }
}

void DecorationItem::handleDecorationGeometryChanged()
{
    setSize(m_decoration->size());
    discardQuads();
}

DecorationRenderer *DecorationItem::renderer() const
{
    return m_renderer.get();
}

Window *DecorationItem::window() const
{
    return m_window;
}

WindowQuad buildQuad(const QRectF &partRect, const QPoint &textureOffset,
                     const qreal devicePixelRatio, bool rotated)
{
    const int p = DecorationRenderer::TexturePad;

    const double x0 = partRect.x();
    const double y0 = partRect.y();
    const double x1 = partRect.x() + partRect.width();
    const double y1 = partRect.y() + partRect.height();

    WindowQuad quad;
    if (rotated) {
        const int u0 = textureOffset.y() + p;
        const int v0 = textureOffset.x() + p;
        const int u1 = textureOffset.y() + p + std::round(partRect.width() * devicePixelRatio);
        const int v1 = textureOffset.x() + p + std::round(partRect.height() * devicePixelRatio);

        quad[0] = WindowVertex(x0, y0, v0, u1); // Top-left
        quad[1] = WindowVertex(x1, y0, v0, u0); // Top-right
        quad[2] = WindowVertex(x1, y1, v1, u0); // Bottom-right
        quad[3] = WindowVertex(x0, y1, v1, u1); // Bottom-left
    } else {
        const int u0 = textureOffset.x() + p;
        const int v0 = textureOffset.y() + p;
        const int u1 = textureOffset.x() + p + std::round(partRect.width() * devicePixelRatio);
        const int v1 = textureOffset.y() + p + std::round(partRect.height() * devicePixelRatio);

        quad[0] = WindowVertex(x0, y0, u0, v0); // Top-left
        quad[1] = WindowVertex(x1, y0, u1, v0); // Top-right
        quad[2] = WindowVertex(x1, y1, u1, v1); // Bottom-right
        quad[3] = WindowVertex(x0, y1, u0, v1); // Bottom-left
    }
    return quad;
}

WindowQuadList DecorationItem::buildQuads() const
{
    if (m_window->frameMargins().isNull()) {
        return WindowQuadList();
    }

    QRectF left, top, right, bottom;
    const qreal devicePixelRatio = m_renderer->effectiveDevicePixelRatio();
    const int texturePad = DecorationRenderer::TexturePad;

    m_window->layoutDecorationRects(left, top, right, bottom);

    const int topHeight = std::round(top.height() * devicePixelRatio);
    const int bottomHeight = std::round(bottom.height() * devicePixelRatio);
    const int leftWidth = std::round(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * texturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * texturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * texturePad));

    WindowQuadList list;
    if (left.isValid()) {
        list.append(buildQuad(left, leftPosition, devicePixelRatio, true));
    }
    if (top.isValid()) {
        list.append(buildQuad(top, topPosition, devicePixelRatio, false));
    }
    if (right.isValid()) {
        list.append(buildQuad(right, rightPosition, devicePixelRatio, true));
    }
    if (bottom.isValid()) {
        list.append(buildQuad(bottom, bottomPosition, devicePixelRatio, false));
    }
    return list;
}

} // namespace KWin

#include "moc_decorationitem.cpp"
