/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/decorationitem.h"
#include "decorations/decoratedwindow.h"
#include "scene/atlas.h"
#include "scene/itemrenderer.h"
#include "scene/outlinedborderitem.h"
#include "scene/scene.h"
#include "window.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>

#include <QPainter>

namespace KWin
{

DecorationRenderer::DecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : m_client(client)
    , m_imageSizesDirty(true)
{
    connect(client->decoration(), &KDecoration3::Decoration::damaged, this, [this](const QRegion &region) {
        addDamage(Region(region));
    });

    connect(client->decoration(), &KDecoration3::Decoration::bordersChanged,
            this, &DecorationRenderer::invalidate);
    connect(client->decoratedWindow(), &KDecoration3::DecoratedWindow::sizeChanged,
            this, &DecorationRenderer::invalidate);

    invalidate();
}

DecorationRenderer::~DecorationRenderer()
{
}

void DecorationRenderer::invalidate()
{
    if (m_client) {
        addDamage(m_client->window()->rect().roundedOut());
    }
    m_imageSizesDirty = true;
}

Region DecorationRenderer::damage() const
{
    return m_damage;
}

void DecorationRenderer::addDamage(const Region &region)
{
    m_damage += region;
    Q_EMIT damaged(region);
}

void DecorationRenderer::resetDamage()
{
    m_damage = Region();
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

Atlas *DecorationRenderer::atlas() const
{
    return m_atlas.get();
}

void DecorationRenderer::render(ItemRenderer *itemRenderer, const Region &region)
{
    const Rect geometry = region.boundingRect();

    RectF decorationRects[4];
    m_client->window()->layoutDecorationRects(decorationRects[int(DecorationPart::Left)],
                                              decorationRects[int(DecorationPart::Top)],
                                              decorationRects[int(DecorationPart::Right)],
                                              decorationRects[int(DecorationPart::Bottom)]);

    const bool resized = std::exchange(m_imageSizesDirty, false);
    if (resized) {
        const qreal dpr = effectiveDevicePixelRatio();

        for (int i = 0; i < 4; ++i) {
            const QSize nativeSize = decorationRects[i]
                .scaled(dpr)
                .rounded()
                .size();
            if (m_images[i].size() != nativeSize || m_images[i].devicePixelRatio() != dpr) {
                m_images[i] = QImage(nativeSize, QImage::Format_ARGB32_Premultiplied);
                m_images[i].setDevicePixelRatio(dpr);
                m_images[i].fill(Qt::transparent);
            }
        }
    }

    const auto renderPart = [this](QImage &image, const RectF &partRect, const RectF &damageRect) {
        const RectF dirtyRect = partRect.intersected(damageRect);
        if (dirtyRect.isEmpty()) {
            return Rect();
        }

        const Rect nativeDirtyRect = dirtyRect
            .scaled(image.devicePixelRatio())
            .roundedOut();
        const RectF snappedDirtyRect = nativeDirtyRect.scaled(1 / image.devicePixelRatio());

        const Rect nativePartRect = partRect
            .scaled(image.devicePixelRatio())
            .rounded();
        const RectF snappedPartRect = nativePartRect.scaled(1 / image.devicePixelRatio());

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(-snappedPartRect.topLeft());
        painter.setClipRect(snappedDirtyRect);

        // clear existing part
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(snappedDirtyRect, Qt::transparent);
        painter.restore();

        m_client->decoration()->paint(&painter, snappedDirtyRect);

        return nativeDirtyRect
            .translated(-nativePartRect.topLeft())
            .intersected(image.rect());
    };

    Rect repainted[4];
    for (int i = 0; i < 4; ++i) {
        repainted[i] = renderPart(m_images[i], decorationRects[i], geometry);
    }

    if (!m_atlas) {
        m_atlas = itemRenderer->createAtlas({m_images[0], m_images[1], m_images[2], m_images[3]});
        return;
    }

    if (resized) {
        m_atlas->reset({m_images[0], m_images[1], m_images[2], m_images[3]});
    } else {
        for (int i = 0; i < 4; ++i) {
            if (!repainted[i].isEmpty()) {
                m_atlas->update(i, m_images[i], repainted[i]);
            }
        }
    }
}

DecorationItem::DecorationItem(KDecoration3::Decoration *decoration, Window *window, Item *parent)
    : Item(parent)
    , m_window(window)
    , m_decoration(decoration)
    , m_renderer(std::make_unique<DecorationRenderer>(window->decoratedWindow()))
{
    connect(window, &Window::targetScaleChanged, this, &DecorationItem::updateScale);

    connect(decoration->window(), &KDecoration3::DecoratedWindow::sizeChanged,
            this, &DecorationItem::handleDecorationGeometryChanged);
    connect(decoration, &KDecoration3::Decoration::bordersChanged,
            this, &DecorationItem::handleDecorationGeometryChanged);
    connect(decoration, &KDecoration3::Decoration::borderOutlineChanged,
            this, &DecorationItem::updateOutline);

    connect(m_renderer.get(), &DecorationRenderer::damaged,
            this, qOverload<const Region &>(&Item::scheduleRepaint));

    setSize(decoration->size());
    updateScale();
    updateOutline();
}

DecorationItem::~DecorationItem()
{
}

QList<RectF> DecorationItem::shape() const
{
    RectF left, top, right, bottom;
    m_window->layoutDecorationRects(left, top, right, bottom);
    return {left, top, right, bottom};
}

Region DecorationItem::opaque() const
{
    if (m_window->decorationHasAlpha()) {
        return Region();
    }
    RectF left, top, right, bottom;
    m_window->layoutDecorationRects(left, top, right, bottom);

    // We have to map to integers which has rounding issues
    // it's safer for a region to be considered transparent than opaque
    // so always align inwards
    const QMargins roundingPad = QMargins(1, 1, 1, 1);
    Region roundedLeft = left.roundedOut().marginsRemoved(roundingPad);
    Region roundedTop = top.roundedOut().marginsRemoved(roundingPad);
    Region roundedRight = right.roundedOut().marginsRemoved(roundingPad);
    Region roundedBottom = bottom.roundedOut().marginsRemoved(roundingPad);

    return roundedLeft | roundedTop | roundedRight | roundedBottom;
}

void DecorationItem::preprocess()
{
    const Region damage = m_renderer->damage();
    if (!damage.isEmpty()) {
        m_renderer->render(scene()->renderer(), damage);
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

void DecorationItem::updateOutline()
{
    if (m_decoration->borderOutline().isNull()) {
        m_outlineItem.reset();
    } else {
        const auto outline = BorderOutline::from(m_decoration->borderOutline());
        if (!m_outlineItem) {
            m_outlineItem = std::make_unique<OutlinedBorderItem>(rect(), outline, this);
        } else {
            m_outlineItem->setOutline(outline);
        }
    }
}

void DecorationItem::handleDecorationGeometryChanged()
{
    setSize(m_decoration->size());
    discardQuads();

    if (m_outlineItem) {
        m_outlineItem->setInnerRect(rect());
    }
}

Atlas *DecorationItem::atlas() const
{
    return m_renderer ? m_renderer->atlas() : nullptr;
}

Window *DecorationItem::window() const
{
    return m_window;
}

WindowQuadList DecorationItem::buildQuads() const
{
    if (m_window->frameMargins().isNull()) {
        return WindowQuadList();
    }

    const Atlas *atlas = m_renderer->atlas();
    if (!atlas) {
        return WindowQuadList();
    }

    RectF decorationRects[4];
    m_window->layoutDecorationRects(decorationRects[int(DecorationRenderer::DecorationPart::Left)],
                                    decorationRects[int(DecorationRenderer::DecorationPart::Top)],
                                    decorationRects[int(DecorationRenderer::DecorationPart::Right)],
                                    decorationRects[int(DecorationRenderer::DecorationPart::Bottom)]);

    WindowQuadList list;
    list.reserve(4);

    for (int i = 0; i < 4; ++i) {
        const auto rect = decorationRects[i];
        if (rect.isEmpty()) {
            continue;
        }

        WindowQuad quad;

        const auto sprite = atlas->sprite(i);
        if (sprite.rotated) {
            quad[0] = WindowVertex(rect.topLeft(), sprite.geometry.bottomLeft());
            quad[1] = WindowVertex(rect.topRight(), sprite.geometry.topLeft());
            quad[2] = WindowVertex(rect.bottomRight(), sprite.geometry.topRight());
            quad[3] = WindowVertex(rect.bottomLeft(), sprite.geometry.bottomRight());
        } else {
            quad[0] = WindowVertex(rect.topLeft(), sprite.geometry.topLeft());
            quad[1] = WindowVertex(rect.topRight(), sprite.geometry.topRight());
            quad[2] = WindowVertex(rect.bottomRight(), sprite.geometry.bottomRight());
            quad[3] = WindowVertex(rect.bottomLeft(), sprite.geometry.bottomLeft());
        }

        list.append(quad);
    }

    return list;
}

} // namespace KWin

#include "moc_decorationitem.cpp"
