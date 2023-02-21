/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/decorationitem.h"
#include "composite.h"
#include "core/output.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "window.h"

#include <cmath>

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>

#include <QPainter>

namespace KWin
{

DecorationRenderer::DecorationRenderer(Decoration::DecoratedClientImpl *client)
    : m_client(client)
    , m_imageSizesDirty(true)
{
    connect(client->decoration(), &KDecoration2::Decoration::damaged,
            this, &DecorationRenderer::addDamage);

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

QImage DecorationRenderer::renderToImage(const QRect &geo)
{
    Q_ASSERT(m_client);

    // Guess the pixel format of the X pixmap into which the QImage will be copied.
    QImage::Format format;
    const int depth = client()->window()->depth();
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

    QImage image(geo.width() * m_devicePixelRatio, geo.height() * m_devicePixelRatio, format);
    image.setDevicePixelRatio(m_devicePixelRatio);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setWindow(QRect(geo.topLeft(), geo.size() * effectiveDevicePixelRatio()));
    p.setClipRect(geo);
    renderToPainter(&p, geo);
    return image;
}

void DecorationRenderer::renderToPainter(QPainter *painter, const QRect &rect)
{
    client()->decoration()->paint(painter, rect);
}

DecorationItem::DecorationItem(KDecoration2::Decoration *decoration, Window *window, Scene *scene, Item *parent)
    : Item(scene, parent)
    , m_window(window)
{
    m_renderer.reset(Compositor::self()->scene()->createDecorationRenderer(window->decoratedClient()));

    connect(window, &Window::frameGeometryChanged,
            this, &DecorationItem::handleFrameGeometryChanged);
    connect(window, &Window::windowClosed,
            this, &DecorationItem::handleWindowClosed);
    connect(window, &Window::screenChanged,
            this, &DecorationItem::handleOutputChanged);

    connect(decoration, &KDecoration2::Decoration::bordersChanged,
            this, &DecorationItem::discardQuads);

    connect(renderer(), &DecorationRenderer::damaged,
            this, qOverload<const QRegion &>(&Item::scheduleRepaint));

    // this toSize is to match that DecoratedWindow also rounds
    setSize(window->size().toSize());
    handleOutputChanged();
}

QVector<QRectF> DecorationItem::shape() const
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
    return QRegion(left.toRect()).united(top.toRect()).united(right.toRect()).united(bottom.toRect());
}

void DecorationItem::preprocess()
{
    const QRegion damage = m_renderer->damage();
    if (!damage.isEmpty()) {
        m_renderer->render(damage);
        m_renderer->resetDamage();
    }
}

void DecorationItem::handleOutputChanged()
{
    if (m_output) {
        disconnect(m_output, &Output::scaleChanged, this, &DecorationItem::handleOutputScaleChanged);
    }

    m_output = m_window->output();

    if (m_output) {
        handleOutputScaleChanged();
        connect(m_output, &Output::scaleChanged, this, &DecorationItem::handleOutputScaleChanged);
    }
}

void DecorationItem::handleOutputScaleChanged()
{
    const qreal dpr = m_output->scale();
    if (m_renderer->devicePixelRatio() != dpr) {
        m_renderer->setDevicePixelRatio(dpr);
        discardQuads();
    }
}

void DecorationItem::handleFrameGeometryChanged()
{
    setSize(m_window->size().toSize());
}

void DecorationItem::handleWindowClosed(Window *original, Deleted *deleted)
{
    m_window = deleted;

    // If the decoration is about to be destroyed, render the decoration for the last time.
    effects->makeOpenGLContextCurrent();
    preprocess();
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
    const QRectF &r = partRect;
    const int p = DecorationRenderer::TexturePad;

    const int x0 = r.x();
    const int y0 = r.y();
    const int x1 = r.x() + r.width();
    const int y1 = r.y() + r.height();

    WindowQuad quad;
    if (rotated) {
        const int u0 = textureOffset.y() + p;
        const int v0 = textureOffset.x() + p;
        const int u1 = textureOffset.y() + p + (r.width() * devicePixelRatio);
        const int v1 = textureOffset.x() + p + (r.height() * devicePixelRatio);

        quad[0] = WindowVertex(x0, y0, v0, u1); // Top-left
        quad[1] = WindowVertex(x1, y0, v0, u0); // Top-right
        quad[2] = WindowVertex(x1, y1, v1, u0); // Bottom-right
        quad[3] = WindowVertex(x0, y1, v1, u1); // Bottom-left
    } else {
        const int u0 = textureOffset.x() + p;
        const int v0 = textureOffset.y() + p;
        const int u1 = textureOffset.x() + p + (r.width() * devicePixelRatio);
        const int v1 = textureOffset.y() + p + (r.height() * devicePixelRatio);

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

    const int topHeight = std::ceil(top.height() * devicePixelRatio);
    const int bottomHeight = std::ceil(bottom.height() * devicePixelRatio);
    const int leftWidth = std::ceil(left.width() * devicePixelRatio);

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
