/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemrenderer_qpainter.h"
#include "platformsupport/scenes/qpainter/qpaintersurfacetexture.h"
#include "scene/scene_qpainter.h"

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

QPainter *ItemRendererQPainter::painter() const
{
    return m_painter.get();
}

void ItemRendererQPainter::beginFrame(RenderTarget *renderTarget)
{
    QImage *buffer = std::get<QImage *>(renderTarget->nativeHandle());
    m_painter->begin(buffer);
    m_painter->setWindow(renderTargetRect());
}

void ItemRendererQPainter::endFrame()
{
    m_painter->end();
}

void ItemRendererQPainter::renderBackground(const QRegion &region)
{
    for (const QRect &rect : region) {
        m_painter->fillRect(rect, Qt::black);
    }
}

void ItemRendererQPainter::renderItem(Item *item, int mask, const QRegion &_region, const WindowPaintData &data)
{
    QRegion region = _region;

    const QRect boundingRect = item->mapToGlobal(item->boundingRect()).toAlignedRect();
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
    if (!platformSurfaceTexture->isValid()) {
        platformSurfaceTexture->create();
    } else {
        platformSurfaceTexture->update(surfaceItem->damage());
    }
    surfaceItem->resetDamage();

    const QVector<QRectF> shape = surfaceItem->shape();
    for (const QRectF rect : shape) {
        const QMatrix4x4 matrix = surfaceItem->surfaceToBufferMatrix();
        const QPointF bufferTopLeft = matrix.map(rect.topLeft());
        const QPointF bufferBottomRight = matrix.map(rect.bottomRight());

        painter->drawImage(rect, platformSurfaceTexture->image(),
                           QRectF(bufferTopLeft, bufferBottomRight));
    }
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

} // namespace KWin
