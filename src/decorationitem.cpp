/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "decorationitem.h"
#include "abstract_client.h"
#include "composite.h"
#include "decorations/decoratedclient.h"
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
    emit damaged(region);
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

} // namespace KWin
