/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_qpainter.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "platformsupport/scenes/qpainter/qpaintersurfacetexture.h"
#include "scene/imageitem.h"
#include "scene/workspacescene_qpainter.h"
#include "window.h"

#include <QPainter>

namespace KWin
{

ItemRendererQPainter::ItemRendererQPainter()
    : m_painter(std::make_unique<QPainter>())
{
}

ItemRendererQPainter::~ItemRendererQPainter()
{
}

std::unique_ptr<ImageItem> ItemRendererQPainter::createImageItem(Item *parent)
{
    return std::make_unique<ImageItem>(parent);
}

QPainter *ItemRendererQPainter::painter() const
{
    return m_painter.get();
}

void ItemRendererQPainter::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    QImage *buffer = renderTarget.image();
    m_painter->begin(buffer);
    m_painter->setWindow(viewport.renderRect().toRect());
}

void ItemRendererQPainter::endFrame()
{
    m_painter->end();
}

void ItemRendererQPainter::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
    m_painter->setCompositionMode(QPainter::CompositionMode_Source);
    for (const QRect &rect : region) {
        m_painter->fillRect(rect, Qt::transparent);
    }
    m_painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void ItemRendererQPainter::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &_region, const WindowPaintData &data)
{
    QRegion region = _region;

    const QRect boundingRect = item->mapToScene(item->boundingRect()).toAlignedRect();
    if (!(mask & (Scene::PAINT_WINDOW_TRANSFORMED | Scene::PAINT_SCREEN_TRANSFORMED))) {
        region &= boundingRect;
    }

    if (region.isEmpty()) {
        return;
    }

    m_painter->save();
    m_painter->setClipRegion(region);
    m_painter->setClipping(true);
    m_painter->setOpacity(data.opacity());

    if (mask & Scene::PAINT_WINDOW_TRANSFORMED) {
        m_painter->translate(data.xTranslation(), data.yTranslation());
        m_painter->scale(data.xScale(), data.yScale());
    }

    renderItem(m_painter.get(), item);

    m_painter->restore();
}

void ItemRendererQPainter::renderItem(QPainter *painter, Item *item) const
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();

    painter->save();
    painter->translate(item->position());
    painter->setOpacity(painter->opacity() * item->opacity());

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->explicitVisible()) {
            renderItem(painter, childItem);
        }
    }

    item->preprocess();
    if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        renderSurfaceItem(painter, surfaceItem);
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        renderDecorationItem(painter, decorationItem);
    } else if (auto imageItem = qobject_cast<ImageItem *>(item)) {
        renderImageItem(painter, imageItem);
    }

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->explicitVisible()) {
            renderItem(painter, childItem);
        }
    }

    painter->restore();
}

void ItemRendererQPainter::renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const
{
    const SurfacePixmap *surfaceTexture = surfaceItem->pixmap();
    if (!surfaceTexture || !surfaceTexture->isValid()) {
        return;
    }

    QPainterSurfaceTexture *platformSurfaceTexture =
        static_cast<QPainterSurfaceTexture *>(surfaceTexture->texture());

    const OutputTransform surfaceToBufferTransform = surfaceItem->bufferTransform();
    const QSizeF transformedSize = surfaceToBufferTransform.map(surfaceItem->destinationSize());

    painter->save();
    switch (surfaceToBufferTransform.kind()) {
    case OutputTransform::Normal:
        break;
    case OutputTransform::Rotate90:
        painter->translate(transformedSize.height(), 0);
        painter->rotate(90);
        break;
    case OutputTransform::Rotate180:
        painter->translate(transformedSize.width(), transformedSize.height());
        painter->rotate(180);
        break;
    case OutputTransform::Rotate270:
        painter->translate(0, transformedSize.width());
        painter->rotate(270);
        break;
    case OutputTransform::FlipX:
        painter->translate(transformedSize.width(), 0);
        painter->scale(-1, 1);
        break;
    case OutputTransform::FlipX90:
        painter->scale(-1, 1);
        painter->rotate(90);
        break;
    case OutputTransform::FlipX180:
        painter->translate(0, transformedSize.height());
        painter->scale(-1, 1);
        painter->rotate(180);
        break;
    case OutputTransform::FlipX270:
        painter->translate(transformedSize.height(), transformedSize.width());
        painter->scale(-1, 1);
        painter->rotate(270);
        break;
    }

    const QRectF sourceBox = surfaceItem->bufferSourceBox();
    const qreal xSourceBoxScale = sourceBox.width() / transformedSize.width();
    const qreal ySourceBoxScale = sourceBox.height() / transformedSize.height();

    const QList<QRectF> shape = surfaceItem->shape();
    for (const QRectF rect : shape) {
        const QRectF target = surfaceToBufferTransform.map(rect, surfaceItem->size());
        const QRectF source(sourceBox.x() + target.x() * xSourceBoxScale,
                            sourceBox.y() + target.y() * ySourceBoxScale,
                            target.width() * xSourceBoxScale,
                            target.height() * ySourceBoxScale);

        painter->drawImage(target, platformSurfaceTexture->image(), source);
    }

    painter->restore();
}

void ItemRendererQPainter::renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const
{
    const auto renderer = static_cast<const SceneQPainterDecorationRenderer *>(decorationItem->renderer());
    QRectF dtr, dlr, drr, dbr;
    decorationItem->window()->layoutDecorationRects(dlr, dtr, drr, dbr);

    painter->drawImage(dtr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Top));
    painter->drawImage(dlr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Left));
    painter->drawImage(drr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Right));
    painter->drawImage(dbr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Bottom));
}

void ItemRendererQPainter::renderImageItem(QPainter *painter, ImageItem *imageItem) const
{
    painter->drawImage(imageItem->rect(), imageItem->image());
}

} // namespace KWin
