/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter.h"
#include "qpaintersurfacetexture.h"
// KWin
#include "abstract_client.h"
#include "composite.h"
#include "cursor.h"
#include "decorations/decoratedclient.h"
#include "deleted.h"
#include "effects.h"
#include "main.h"
#include "renderloop.h"
#include "screens.h"
#include "surfaceitem.h"
#include "toplevel.h"
#include "platform.h"
#include "windowitem.h"
#include "abstract_output.h"

#include <kwinoffscreenquickview.h>
// Qt
#include <QDebug>
#include <QPainter>
#include <KDecoration2/Decoration>

#include <cmath>

namespace KWin
{

//****************************************
// SceneQPainter
//****************************************
SceneQPainter *SceneQPainter::createScene(QPainterBackend *backend, QObject *parent)
{
    return new SceneQPainter(backend, parent);
}

SceneQPainter::SceneQPainter(QPainterBackend *backend, QObject *parent)
    : Scene(parent)
    , m_backend(backend)
    , m_painter(new QPainter())
{
}

SceneQPainter::~SceneQPainter()
{
}

bool SceneQPainter::initFailed() const
{
    return false;
}

void SceneQPainter::paintGenericScreen(int mask, const ScreenPaintData &data)
{
    m_painter->save();
    m_painter->translate(data.xTranslation(), data.yTranslation());
    m_painter->scale(data.xScale(), data.yScale());
    Scene::paintGenericScreen(mask, data);
    m_painter->restore();
}

void SceneQPainter::paint(AbstractOutput *output, const QRegion &damage, const QList<Toplevel *> &toplevels,
                          RenderLoop *renderLoop)
{
    Q_ASSERT(kwinApp()->platform()->isPerScreenRenderingEnabled());
    painted_screen = output;

    createStackingOrder(toplevels);

    const QRegion repaint = m_backend->beginFrame(output);
    const QRect geometry = output->geometry();

    QImage *buffer = m_backend->bufferForScreen(output);
    if (buffer && !buffer->isNull()) {
        renderLoop->beginFrame();
        m_painter->begin(buffer);
        m_painter->setWindow(geometry);

        QRegion updateRegion, validRegion;
        paintScreen(damage.intersected(geometry), repaint, &updateRegion, &validRegion, renderLoop);
        paintCursor(output, updateRegion);

        m_painter->end();
        renderLoop->endFrame();
        m_backend->endFrame(output, validRegion, updateRegion);
    }

    // do cleanup
    clearStackingOrder();
}

void SceneQPainter::paintBackground(const QRegion &region)
{
    for (const QRect &rect : region) {
        m_painter->fillRect(rect, Qt::black);
    }
}

void SceneQPainter::paintCursor(AbstractOutput *output, const QRegion &rendered)
{
    if (!output || output->usesSoftwareCursor() || Cursors::self()->isCursorHidden()) {
        return;
    }

    Cursor* cursor = Cursors::self()->currentCursor();
    const QImage img = cursor->image();
    if (img.isNull()) {
        return;
    }

    m_painter->save();
    m_painter->setClipRegion(rendered.intersected(cursor->geometry()));
    m_painter->drawImage(cursor->geometry(), img);
    m_painter->restore();
}

void SceneQPainter::paintOffscreenQuickView(OffscreenQuickView *w)
{
    QPainter *painter = effects->scenePainter();
    const QImage buffer = w->bufferAsImage();
    if (buffer.isNull()) {
        return;
    }
    painter->drawImage(w->geometry(), buffer);
}

Scene::Window *SceneQPainter::createWindow(Toplevel *toplevel)
{
    return new SceneQPainter::Window(this, toplevel);
}

Scene::EffectFrame *SceneQPainter::createEffectFrame(EffectFrameImpl *frame)
{
    return new QPainterEffectFrame(frame, this);
}

Shadow *SceneQPainter::createShadow(Toplevel *toplevel)
{
    return new SceneQPainterShadow(toplevel);
}

QImage *SceneQPainter::qpainterRenderBuffer(AbstractOutput *output) const
{
    return m_backend->bufferForScreen(output);
}

//****************************************
// SceneQPainter::Window
//****************************************
SceneQPainter::Window::Window(SceneQPainter *scene, Toplevel *c)
    : Scene::Window(c)
    , m_scene(scene)
{
}

SceneQPainter::Window::~Window()
{
}

void SceneQPainter::Window::performPaint(int mask, const QRegion &_region, const WindowPaintData &data)
{
    QRegion region = _region;

    const QRect boundingRect = windowItem()->mapToGlobal(windowItem()->boundingRect());
    if (!(mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        region &= boundingRect;

    if (region.isEmpty())
        return;

    QPainter *scenePainter = m_scene->scenePainter();
    QPainter *painter = scenePainter;
    painter->save();
    painter->setClipRegion(region);
    painter->setClipping(true);

    if (mask & PAINT_WINDOW_TRANSFORMED) {
        painter->translate(data.xTranslation(), data.yTranslation());
        painter->scale(data.xScale(), data.yScale());
    }

    const bool opaque = qFuzzyCompare(1.0, data.opacity());
    QImage tempImage;
    QPainter tempPainter;
    if (!opaque) {
        // need a temp render target which we later on blit to the screen
        tempImage = QImage(boundingRect.size(), QImage::Format_ARGB32_Premultiplied);
        tempImage.fill(Qt::transparent);
        tempPainter.begin(&tempImage);
        tempPainter.save();
        tempPainter.translate(-boundingRect.topLeft());
        painter = &tempPainter;
    }

    renderItem(painter, windowItem());

    if (!opaque) {
        tempPainter.restore();
        tempPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QColor translucent(Qt::transparent);
        translucent.setAlphaF(data.opacity());
        tempPainter.fillRect(QRect(QPoint(0, 0), boundingRect.size()), translucent);
        tempPainter.end();
        painter = scenePainter;
        painter->drawImage(boundingRect.topLeft(), tempImage);
    }

    painter->restore();
}

void SceneQPainter::Window::renderItem(QPainter *painter, Item *item) const
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();

    painter->save();
    painter->translate(item->position());

    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->isVisible()) {
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
        if (childItem->isVisible()) {
            renderItem(painter, childItem);
        }
    }

    painter->restore();
}

void SceneQPainter::Window::renderSurfaceItem(QPainter *painter, SurfaceItem *surfaceItem) const
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

    const QRegion shape = surfaceItem->shape();
    for (const QRectF rect : shape) {
        const QMatrix4x4 matrix = surfaceItem->surfaceToBufferMatrix();
        const QPointF bufferTopLeft = matrix.map(rect.topLeft());
        const QPointF bufferBottomRight = matrix.map(rect.bottomRight());

        painter->drawImage(rect, platformSurfaceTexture->image(),
                           QRectF(bufferTopLeft, bufferBottomRight));
    }
}

void SceneQPainter::Window::renderDecorationItem(QPainter *painter, DecorationItem *decorationItem) const
{
    const auto renderer = static_cast<const SceneQPainterDecorationRenderer *>(decorationItem->renderer());
    QRect dtr, dlr, drr, dbr;
    if (auto client = qobject_cast<AbstractClient *>(toplevel)) {
        client->layoutDecorationRects(dlr, dtr, drr, dbr);
    } else if (auto deleted = qobject_cast<Deleted *>(toplevel)) {
        deleted->layoutDecorationRects(dlr, dtr, drr, dbr);
    } else {
        return;
    }

    painter->drawImage(dtr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Top));
    painter->drawImage(dlr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Left));
    painter->drawImage(drr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Right));
    painter->drawImage(dbr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Bottom));
}

DecorationRenderer *SceneQPainter::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneQPainterDecorationRenderer(impl);
}

SurfaceTexture *SceneQPainter::createSurfaceTextureInternal(SurfacePixmapInternal *pixmap)
{
    return m_backend->createSurfaceTextureInternal(pixmap);
}

SurfaceTexture *SceneQPainter::createSurfaceTextureWayland(SurfacePixmapWayland *pixmap)
{
    return m_backend->createSurfaceTextureWayland(pixmap);
}

QPainterEffectFrame::QPainterEffectFrame(EffectFrameImpl *frame, SceneQPainter *scene)
    : Scene::EffectFrame(frame)
    , m_scene(scene)
{
}

QPainterEffectFrame::~QPainterEffectFrame()
{
}

void QPainterEffectFrame::render(const QRegion &region, double opacity, double frameOpacity)
{
    Q_UNUSED(region)
    Q_UNUSED(opacity)
    // TODO: adjust opacity
    if (m_effectFrame->geometry().isEmpty()) {
        return; // Nothing to display
    }
    QPainter *painter = m_scene->scenePainter();


    // Render the actual frame
    if (m_effectFrame->style() == EffectFrameUnstyled) {
        painter->save();
        painter->setPen(Qt::NoPen);
        QColor color(Qt::black);
        color.setAlphaF(frameOpacity);
        painter->setBrush(color);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawRoundedRect(m_effectFrame->geometry().adjusted(-5, -5, 5, 5), 5.0, 5.0);
        painter->restore();
    } else if (m_effectFrame->style() == EffectFrameStyled) {
        qreal left, top, right, bottom;
        m_effectFrame->frame().getMargins(left, top, right, bottom);   // m_geometry is the inner geometry
        QRect geom = m_effectFrame->geometry().adjusted(-left, -top, right, bottom);
        painter->drawPixmap(geom, m_effectFrame->frame().framePixmap());
    }
    if (!m_effectFrame->selection().isNull()) {
        painter->drawPixmap(m_effectFrame->selection(), m_effectFrame->selectionFrame().framePixmap());
    }

    // Render icon
    if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
        const QPoint topLeft(m_effectFrame->geometry().x(),
                             m_effectFrame->geometry().center().y() - m_effectFrame->iconSize().height() / 2);

        const QRect geom = QRect(topLeft, m_effectFrame->iconSize());
        painter->drawPixmap(geom, m_effectFrame->icon().pixmap(m_effectFrame->iconSize()));
    }

    // Render text
    if (!m_effectFrame->text().isEmpty()) {
        // Determine position on texture to paint text
        QRect rect(QPoint(0, 0), m_effectFrame->geometry().size());
        if (!m_effectFrame->icon().isNull() && !m_effectFrame->iconSize().isEmpty()) {
            rect.setLeft(m_effectFrame->iconSize().width());
        }

        // If static size elide text as required
        QString text = m_effectFrame->text();
        if (m_effectFrame->isStatic()) {
            QFontMetrics metrics(m_effectFrame->text());
            text = metrics.elidedText(text, Qt::ElideRight, rect.width());
        }

        painter->save();
        painter->setFont(m_effectFrame->font());
        if (m_effectFrame->style() == EffectFrameStyled) {
            painter->setPen(m_effectFrame->styledTextColor());
        } else {
            // TODO: What about no frame? Custom color setting required
            painter->setPen(Qt::white);
        }
        painter->drawText(rect.translated(m_effectFrame->geometry().topLeft()), m_effectFrame->alignment(), text);
        painter->restore();
    }
}

//****************************************
// QPainterShadow
//****************************************
SceneQPainterShadow::SceneQPainterShadow(Toplevel* toplevel)
    : Shadow(toplevel)
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
    QRect left, top, right, bottom;
    client()->client()->layoutDecorationRects(left, top, right, bottom);

    auto checkAndCreate = [this](int index, const QSize &size) {
        auto dpr = effectiveDevicePixelRatio();
        if (m_images[index].size() != size * dpr ||
            m_images[index].devicePixelRatio() != dpr)
        {
            m_images[index] = QImage(size * dpr, QImage::Format_ARGB32_Premultiplied);
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
