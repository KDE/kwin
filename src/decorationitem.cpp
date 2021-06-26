/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "decorationitem.h"
#include "abstract_client.h"
#include "composite.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "scene.h"
#include "utils.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <QPainter>

namespace KWin
{

DecorationRenderer::DecorationRenderer(Decoration::DecoratedClientImpl *client)
    : m_client(client)
    , m_imageSizesDirty(true)
{
    connect(client->decoration(), &KDecoration2::Decoration::damaged,
            this, &DecorationRenderer::addDamage);

    connect(client->client(), &AbstractClient::screenScaleChanged,
            this, &DecorationRenderer::invalidate);
    connect(client->decoration(), &KDecoration2::Decoration::bordersChanged,
            this, &DecorationRenderer::invalidate);
    connect(client->decoratedClient(), &KDecoration2::DecoratedClient::sizeChanged,
            this, &DecorationRenderer::invalidate);

    invalidate();
}

Decoration::DecoratedClientImpl *DecorationRenderer::client() const
{
    return m_client;
}

void DecorationRenderer::invalidate()
{
    addDamage(m_client->client()->rect());
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

QImage DecorationRenderer::renderToImage(const QRect &geo)
{
    Q_ASSERT(m_client);
    auto dpr = client()->client()->screenScale();

    // Guess the pixel format of the X pixmap into which the QImage will be copied.
    QImage::Format format;
    const int depth = client()->client()->depth();
    switch (depth) {
    case 30:
        format = QImage::Format_A2RGB30_Premultiplied;
        break;
    case 24:
    case 32:
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    default:
        qCCritical(KWIN_CORE) << "Unsupported client depth" << depth;
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    };

    QImage image(geo.width() * dpr, geo.height() * dpr, format);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setWindow(QRect(geo.topLeft(), geo.size() * qPainterEffectiveDevicePixelRatio(&p)));
    p.setClipRect(geo);
    renderToPainter(&p, geo);
    return image;
}

void DecorationRenderer::renderToPainter(QPainter *painter, const QRect &rect)
{
    client()->decoration()->paint(painter, rect);
}

DecorationItem::DecorationItem(KDecoration2::Decoration *decoration, Scene::Window *window, Item *parent)
    : Item(window, parent)
{
    AbstractClient *client = qobject_cast<AbstractClient *>(window->window());
    m_renderer.reset(Compositor::self()->scene()->createDecorationRenderer(client->decoratedClient()));

    connect(client, &Toplevel::frameGeometryChanged,
            this, &DecorationItem::handleFrameGeometryChanged);

    connect(client, &Toplevel::screenScaleChanged,
            this, &DecorationItem::discardQuads);
    connect(decoration, &KDecoration2::Decoration::bordersChanged,
            this, &DecorationItem::discardQuads);

    connect(renderer(), &DecorationRenderer::damaged,
            this, &DecorationItem::scheduleRepaint);

    // If the decoration is about to be destroyed, render the decoration for the last time.
    connect(client, &Toplevel::markedAsZombie, this, &DecorationItem::preprocess);

    setSize(client->size());
}

void DecorationItem::preprocess()
{
    const QRegion damage = m_renderer->damage();
    if (!damage.isEmpty()) {
        m_renderer->render(damage);
        m_renderer->resetDamage();
    }
}

void DecorationItem::handleFrameGeometryChanged()
{
    setSize(window()->size());
}

DecorationRenderer *DecorationItem::renderer() const
{
    return m_renderer.data();
}

WindowQuadList DecorationItem::buildQuads() const
{
    const Toplevel *toplevel = window()->window();
    if (toplevel->frameMargins().isNull()) {
        return WindowQuadList();
    }

    QRect rects[4];

    if (const AbstractClient *client = qobject_cast<const AbstractClient *>(toplevel)) {
        client->layoutDecorationRects(rects[0], rects[1], rects[2], rects[3]);
    } else if (const Deleted *deleted = qobject_cast<const Deleted *>(toplevel)) {
        deleted->layoutDecorationRects(rects[0], rects[1], rects[2], rects[3]);
    }

    const qreal textureScale = toplevel->screenScale();
    const int padding = 1;

    const QPoint topSpritePosition(padding, padding);
    const QPoint bottomSpritePosition(padding, topSpritePosition.y() + rects[1].height() + 2 * padding);
    const QPoint leftSpritePosition(bottomSpritePosition.y() + rects[3].height() + 2 * padding, padding);
    const QPoint rightSpritePosition(leftSpritePosition.x() + rects[0].width() + 2 * padding, padding);

    const QPoint offsets[4] = {
        QPoint(-rects[0].x(), -rects[0].y()) + leftSpritePosition,
        QPoint(-rects[1].x(), -rects[1].y()) + topSpritePosition,
        QPoint(-rects[2].x(), -rects[2].y()) + rightSpritePosition,
        QPoint(-rects[3].x(), -rects[3].y()) + bottomSpritePosition,
    };

    const Qt::Orientation orientations[4] = {
        Qt::Vertical,   // Left
        Qt::Horizontal, // Top
        Qt::Vertical,   // Right
        Qt::Horizontal, // Bottom
    };

    const QRegion shape = QRegion(toplevel->rect()) - toplevel->transparentRect();

    WindowQuadList list;
    list.reserve(shape.rectCount());

    for (int i = 0; i < 4; i++) {
        const QRegion intersectedRegion = (shape & rects[i]);
        for (const QRect &r : intersectedRegion) {
            if (!r.isValid()) {
                continue;
            }

            const int x0 = r.x();
            const int y0 = r.y();
            const int x1 = r.x() + r.width();
            const int y1 = r.y() + r.height();

            const int u0 = (x0 + offsets[i].x()) * textureScale;
            const int v0 = (y0 + offsets[i].y()) * textureScale;
            const int u1 = (x1 + offsets[i].x()) * textureScale;
            const int v1 = (y1 + offsets[i].y()) * textureScale;

            WindowQuad quad;

            if (orientations[i] == Qt::Vertical) {
                quad[0] = WindowVertex(x0, y0, v0, u0); // Top-left
                quad[1] = WindowVertex(x1, y0, v0, u1); // Top-right
                quad[2] = WindowVertex(x1, y1, v1, u1); // Bottom-right
                quad[3] = WindowVertex(x0, y1, v1, u0); // Bottom-left
            } else {
                quad[0] = WindowVertex(x0, y0, u0, v0); // Top-left
                quad[1] = WindowVertex(x1, y0, u1, v0); // Top-right
                quad[2] = WindowVertex(x1, y1, u1, v1); // Bottom-right
                quad[3] = WindowVertex(x0, y1, u0, v1); // Bottom-left
            }

            list.append(quad);
        }
    }

    return list;
}

} // namespace KWin
