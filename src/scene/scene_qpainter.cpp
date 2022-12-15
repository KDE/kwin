/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter.h"
#include "qpaintersurfacetexture.h"
// KWin
#include "core/output.h"
#include "decorations/decoratedclient.h"
#include "effects.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "window.h"

#include <kwinoffscreenquickview.h>
// Qt
#include <KDecoration2/Decoration>
#include <QDebug>
#include <QPainter>

#include <cmath>

namespace KWin
{

//****************************************
// SceneQPainter
//****************************************

SceneQPainter::SceneQPainter(QPainterBackend *backend)
    : m_backend(backend)
    , m_painter(new QPainter())
{
}

SceneQPainter::~SceneQPainter()
{
}

void SceneQPainter::paint(RenderTarget *target, const QRegion &region)
{
    QImage *buffer = std::get<QImage *>(target->nativeHandle());
    if (buffer && !buffer->isNull()) {
        m_painter->begin(buffer);
        m_painter->setWindow(painted_screen->geometry());
        paintScreen(region);
        m_painter->end();
    }
}

void SceneQPainter::paintBackground(const QRegion &region)
{
    for (const QRect &rect : region) {
        m_painter->fillRect(rect, Qt::black);
    }
}

void SceneQPainter::paintOffscreenQuickView(OffscreenQuickView *w)
{
    QPainter *painter = effects->scenePainter();
    const QImage buffer = w->bufferAsImage();
    if (buffer.isNull()) {
        return;
    }
    painter->save();
    painter->setOpacity(w->opacity());
    painter->drawImage(w->geometry(), buffer);
    painter->restore();
}

Shadow *SceneQPainter::createShadow(Window *window)
{
    return new SceneQPainterShadow(window);
}

void SceneQPainter::render(Item *item, int mask, const QRegion &_region, const WindowPaintData &data)
{
    QRegion region = _region;

    const QRect boundingRect = item->mapToGlobal(item->boundingRect()).toAlignedRect();
    if (!(mask & (Scene::PAINT_WINDOW_TRANSFORMED | Scene::PAINT_SCREEN_TRANSFORMED))) {
        region &= boundingRect;
    }

    if (region.isEmpty()) {
        return;
    }

    QPainter *painter = scenePainter();
    painter->save();
    painter->setClipRegion(region);
    painter->setClipping(true);
    painter->setOpacity(data.opacity());

    if (mask & Scene::PAINT_WINDOW_TRANSFORMED) {
        painter->translate(data.xTranslation(), data.yTranslation());
        painter->scale(data.xScale(), data.yScale());
    }

    renderItem(painter, item);

    painter->restore();
}

void SceneQPainter::renderItem(QPainter *painter, Item *item) const
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

void SceneQPainter::renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const
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

void SceneQPainter::renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const
{
    const auto renderer = static_cast<const SceneQPainterDecorationRenderer *>(decorationItem->renderer());
    QRectF dtr, dlr, drr, dbr;
    decorationItem->window()->layoutDecorationRects(dlr, dtr, drr, dbr);

    painter->drawImage(dtr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Top));
    painter->drawImage(dlr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Left));
    painter->drawImage(drr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Right));
    painter->drawImage(dbr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Bottom));
}

DecorationRenderer *SceneQPainter::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneQPainterDecorationRenderer(impl);
}

std::unique_ptr<SurfaceTexture> SceneQPainter::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backend->createSurfaceTextureInternal(pixmap);
}

std::unique_ptr<SurfaceTexture> SceneQPainter::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backend->createSurfaceTextureWayland(pixmap);
}

//****************************************
// QPainterShadow
//****************************************
SceneQPainterShadow::SceneQPainterShadow(Window *window)
    : Shadow(window)
{
}

SceneQPainterShadow::~SceneQPainterShadow()
{
}

bool SceneQPainterShadow::prepareBackend()
{
    return true;
}

//****************************************
// QPainterDecorationRenderer
//****************************************
SceneQPainterDecorationRenderer::SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client)
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

} // KWin
