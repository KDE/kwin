/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/decorationitem.h"
#include "core/gpumanager.h"
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
{
    connect(client->decoration(), &KDecoration3::Decoration::damaged, this, [this](const QRegion &region) {
        addDamage(RegionF(region));
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
        addDamage(m_client->window()->rect());
    }
    m_imageSizesNotDirty.clear();
}

RegionF DecorationRenderer::damage(RenderDevice *device) const
{
    return m_damage[device];
}

void DecorationRenderer::addDamage(const RegionF &region)
{
    const auto &devices = GpuManager::self()->renderDevices();
    for (const auto &device : devices) {
        m_damage[device.get()] += region;
    }
    Q_EMIT damaged(region);
}

void DecorationRenderer::resetDamage(RenderDevice *device)
{
    m_damage.remove(device);
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

Atlas *DecorationRenderer::atlas(RenderDevice *device) const
{
    const auto it = m_atlas.find(device);
    return it == m_atlas.end() ? nullptr : it->second.get();
}

bool DecorationRenderer::needsRepaint(RenderDevice *device) const
{
    return !m_imageSizesNotDirty.contains(device) || !damage(device).isEmpty();
}

void DecorationRenderer::render(ItemRenderer *itemRenderer, const RegionF &region)
{
    const RectF geometry = region.boundingRect();

    RectF decorationRects[4];
    m_client->window()->layoutDecorationRects(decorationRects[int(DecorationPart::Left)],
                                              decorationRects[int(DecorationPart::Top)],
                                              decorationRects[int(DecorationPart::Right)],
                                              decorationRects[int(DecorationPart::Bottom)]);

    const bool resized = !m_imageSizesNotDirty.contains(itemRenderer->renderDevice());
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
        m_imageSizesNotDirty.insert(itemRenderer->renderDevice());
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

    auto &atlas = m_atlas[itemRenderer->renderDevice()];
    if (!atlas) {
        atlas = itemRenderer->createAtlas({m_images[0], m_images[1], m_images[2], m_images[3]});
        return;
    }

    if (resized) {
        atlas->reset({m_images[0], m_images[1], m_images[2], m_images[3]});
    } else {
        for (int i = 0; i < 4; ++i) {
            if (!repainted[i].isEmpty()) {
                atlas->update(i, m_images[i], repainted[i]);
            }
        }
    }
}

void DecorationRenderer::releaseResources(RenderDevice *device)
{
    m_atlas.erase(device);
    m_imageSizesNotDirty.erase(device);
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
            this, qOverload<const RegionF &>(&Item::scheduleRepaint));

    setSize(decoration->size());
    updateScale();
    updateOutline();
}

DecorationItem::~DecorationItem()
{
}

RegionF DecorationItem::shape() const
{
    RectF left, top, right, bottom;
    m_window->layoutDecorationRects(left, top, right, bottom);
    return RegionF(left) | top | right | bottom;
}

RegionF DecorationItem::opaque() const
{
    if (m_window->decorationHasAlpha()) {
        return RegionF();
    } else {
        return shape();
    }
}

void DecorationItem::preprocess(ItemRenderer *renderer)
{
    if (m_renderer->needsRepaint(renderer->renderDevice())) {
        m_renderer->render(renderer, m_renderer->damage(renderer->renderDevice()));
        m_renderer->resetDamage(renderer->renderDevice());
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

Atlas *DecorationItem::atlas(RenderDevice *device) const
{
    return m_renderer ? m_renderer->atlas(device) : nullptr;
}

Window *DecorationItem::window() const
{
    return m_window;
}

WindowQuadList DecorationItem::buildQuads(ItemRenderer *renderer) const
{
    if (m_window->frameMargins().isNull()) {
        return WindowQuadList();
    }

    const Atlas *atlas = m_renderer->atlas(renderer->renderDevice());
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

void DecorationItem::releaseResources(RenderDevice *device)
{
    m_renderer->releaseResources(device);
}

} // namespace KWin

#include "moc_decorationitem.cpp"
