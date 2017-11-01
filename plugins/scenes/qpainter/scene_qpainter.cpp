/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "scene_qpainter.h"
// KWin
#include "client.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "main.h"
#include "screens.h"
#include "toplevel.h"
#include "platform.h"
#include "wayland_server.h"
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/subcompositor_interface.h>
#include <KWayland/Server/surface_interface.h>
#include "decorations/decoratedclient.h"
// Qt
#include <QDebug>
#include <QPainter>
#include <KDecoration2/Decoration>

namespace KWin
{

//****************************************
// SceneQPainter
//****************************************
SceneQPainter *SceneQPainter::createScene(QObject *parent)
{
    QScopedPointer<QPainterBackend> backend(kwinApp()->platform()->createQPainterBackend());
    if (backend.isNull()) {
        return nullptr;
    }
    if (backend->isFailed()) {
        return NULL;
    }
    return new SceneQPainter(backend.take(), parent);
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

CompositingType SceneQPainter::compositingType() const
{
    return QPainterCompositing;
}

bool SceneQPainter::initFailed() const
{
    return false;
}

void SceneQPainter::paintGenericScreen(int mask, ScreenPaintData data)
{
    m_painter->save();
    m_painter->translate(data.xTranslation(), data.yTranslation());
    m_painter->scale(data.xScale(), data.yScale());
    Scene::paintGenericScreen(mask, data);
    m_painter->restore();
}

qint64 SceneQPainter::paint(QRegion damage, ToplevelList toplevels)
{
    QElapsedTimer renderTimer;
    renderTimer.start();

    createStackingOrder(toplevels);

    int mask = 0;
    m_backend->prepareRenderingFrame();
    if (m_backend->perScreenRendering()) {
        const bool needsFullRepaint = m_backend->needsFullRepaint();
        if (needsFullRepaint) {
            mask |= Scene::PAINT_SCREEN_BACKGROUND_FIRST;
            damage = screens()->geometry();
        }
        QRegion overallUpdate;
        for (int i = 0; i < screens()->count(); ++i) {
            const QRect geometry = screens()->geometry(i);
            QImage *buffer = m_backend->bufferForScreen(i);
            if (!buffer || buffer->isNull()) {
                continue;
            }
            m_painter->begin(buffer);
            m_painter->save();
            m_painter->setWindow(geometry);

            QRegion updateRegion, validRegion;
            paintScreen(&mask, damage.intersected(geometry), QRegion(), &updateRegion, &validRegion);
            overallUpdate = overallUpdate.united(updateRegion);
            paintCursor();

            m_painter->restore();
            m_painter->end();
        }
        m_backend->showOverlay();
        m_backend->present(mask, overallUpdate);
    } else {
        m_painter->begin(m_backend->buffer());
        m_painter->setClipping(true);
        m_painter->setClipRegion(damage);
        if (m_backend->needsFullRepaint()) {
            mask |= Scene::PAINT_SCREEN_BACKGROUND_FIRST;
            damage = screens()->geometry();
        }
        QRegion updateRegion, validRegion;
        paintScreen(&mask, damage, QRegion(), &updateRegion, &validRegion);

        paintCursor();
        m_backend->showOverlay();

        m_painter->end();
        m_backend->present(mask, updateRegion);
    }

    // do cleanup
    clearStackingOrder();

    emit frameRendered();

    return renderTimer.nsecsElapsed();
}

void SceneQPainter::paintBackground(QRegion region)
{
    m_painter->setBrush(Qt::black);
    m_painter->drawRects(region.rects());
}

void SceneQPainter::paintCursor()
{
    if (!kwinApp()->platform()->usesSoftwareCursor()) {
        return;
    }
    const QImage img = kwinApp()->platform()->softwareCursor();
    if (img.isNull()) {
        return;
    }
    const QPoint cursorPos = Cursor::pos();
    const QPoint hotspot = kwinApp()->platform()->softwareCursorHotspot();
    m_painter->drawImage(cursorPos - hotspot, img);
    kwinApp()->platform()->markCursorAsRendered();
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

void SceneQPainter::screenGeometryChanged(const QSize &size)
{
    Scene::screenGeometryChanged(size);
    m_backend->screenGeometryChanged(size);
}

QImage *SceneQPainter::qpainterRenderBuffer() const
{
    return m_backend->buffer();
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
    discardShape();
}

static void paintSubSurface(QPainter *painter, const QPoint &pos, QPainterWindowPixmap *pixmap)
{
    QPoint p = pos;
    if (!pixmap->subSurface().isNull()) {
        p += pixmap->subSurface()->position();
    }

    painter->drawImage(QRect(pos, pixmap->size()), pixmap->image());
    const auto &children = pixmap->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        auto pixmap = static_cast<QPainterWindowPixmap*>(*it);
        if (pixmap->subSurface().isNull() || pixmap->subSurface()->surface().isNull() || !pixmap->subSurface()->surface()->isMapped()) {
            continue;
        }
        paintSubSurface(painter, p, pixmap);
    }
}

void SceneQPainter::Window::performPaint(int mask, QRegion region, WindowPaintData data)
{
    if (!(mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        region &= toplevel->visibleRect();

    if (region.isEmpty())
        return;
    QPainterWindowPixmap *pixmap = windowPixmap<QPainterWindowPixmap>();
    if (!pixmap || !pixmap->isValid()) {
        return;
    }
    if (!toplevel->damage().isEmpty()) {
        pixmap->updateBuffer();
        toplevel->resetDamage();
    }

    QPainter *scenePainter = m_scene->scenePainter();
    QPainter *painter = scenePainter;
    painter->save();
    painter->setClipRegion(region);
    painter->setClipping(true);

    painter->translate(x(), y());
    if (mask & PAINT_WINDOW_TRANSFORMED) {
        painter->translate(data.xTranslation(), data.yTranslation());
        painter->scale(data.xScale(), data.yScale());
    }

    const bool opaque = qFuzzyCompare(1.0, data.opacity());
    QImage tempImage;
    QPainter tempPainter;
    if (!opaque) {
        // need a temp render target which we later on blit to the screen
        tempImage = QImage(toplevel->visibleRect().size(), QImage::Format_ARGB32_Premultiplied);
        tempImage.fill(Qt::transparent);
        tempPainter.begin(&tempImage);
        tempPainter.save();
        tempPainter.translate(toplevel->geometry().topLeft() - toplevel->visibleRect().topLeft());
        painter = &tempPainter;
    }
    renderShadow(painter);
    renderWindowDecorations(painter);

    // render content
    const QRect target = QRect(toplevel->clientPos(), toplevel->clientSize());
    QSize srcSize = pixmap->image().size();
    if (pixmap->surface() && pixmap->surface()->scale() == 1 && srcSize != toplevel->clientSize()) {
        // special case for XWayland windows
        srcSize = toplevel->clientSize();
    }
    const QRect src = QRect(toplevel->clientPos() + toplevel->clientContentPos(), srcSize);
    painter->drawImage(target, pixmap->image(), src);

    // render subsurfaces
    const auto &children = pixmap->children();
    for (auto pixmap : children) {
        if (pixmap->subSurface().isNull() || pixmap->subSurface()->surface().isNull() || !pixmap->subSurface()->surface()->isMapped()) {
            continue;
        }
        paintSubSurface(painter, toplevel->clientPos(), static_cast<QPainterWindowPixmap*>(pixmap));
    }

    if (!opaque) {
        tempPainter.restore();
        tempPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QColor translucent(Qt::transparent);
        translucent.setAlphaF(data.opacity());
        tempPainter.fillRect(QRect(QPoint(0, 0), toplevel->visibleRect().size()), translucent);
        tempPainter.end();
        painter = scenePainter;
        painter->drawImage(toplevel->visibleRect().topLeft() - toplevel->geometry().topLeft(), tempImage);
    }

    painter->restore();
}

void SceneQPainter::Window::renderShadow(QPainter* painter)
{
    if (!toplevel->shadow()) {
        return;
    }
    SceneQPainterShadow *shadow = static_cast<SceneQPainterShadow *>(toplevel->shadow());
    const QPixmap &topLeft     = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTopLeft);
    const QPixmap &top         = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTop);
    const QPixmap &topRight    = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementTopRight);
    const QPixmap &bottomLeft  = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottomLeft);
    const QPixmap &bottom      = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottom);
    const QPixmap &bottomRight = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementBottomRight);
    const QPixmap &left        = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementLeft);
    const QPixmap &right       = shadow->shadowPixmap(SceneQPainterShadow::ShadowElementRight);

    const int leftOffset   = shadow->leftOffset();
    const int topOffset    = shadow->topOffset();
    const int rightOffset  = shadow->rightOffset();
    const int bottomOffset = shadow->bottomOffset();

    // top left
    painter->drawPixmap(-leftOffset, -topOffset, topLeft);
    // top right
    painter->drawPixmap(toplevel->width() - topRight.width() + rightOffset, -topOffset, topRight);
    // bottom left
    painter->drawPixmap(-leftOffset, toplevel->height() - bottomLeft.height() + bottomOffset, bottomLeft);
    // bottom right
    painter->drawPixmap(toplevel->width() - bottomRight.width() + rightOffset,
                        toplevel->height() - bottomRight.height() + bottomOffset,
                        bottomRight);
    // top
    painter->drawPixmap(topLeft.width() - leftOffset, -topOffset,
                        toplevel->width() - topLeft.width() - topRight.width() + leftOffset + rightOffset,
                        top.height(),
                        top);
    // left
    painter->drawPixmap(-leftOffset, topLeft.height() - topOffset, left.width(),
                        toplevel->height() - topLeft.height() - bottomLeft.height() + topOffset + bottomOffset,
                        left);
    // right
    painter->drawPixmap(toplevel->width() - right.width() + rightOffset,
                        topRight.height() - topOffset,
                        right.width(),
                        toplevel->height() - topRight.height() - bottomRight.height() + topOffset + bottomOffset,
                        right);
    // bottom
    painter->drawPixmap(bottomLeft.width() - leftOffset,
                        toplevel->height() - bottom.height() + bottomOffset,
                        toplevel->width() - bottomLeft.width() - bottomRight.width() + leftOffset + rightOffset,
                        bottom.height(),
                        bottom);
}

void SceneQPainter::Window::renderWindowDecorations(QPainter *painter)
{
    // TODO: custom decoration opacity
    AbstractClient *client = dynamic_cast<AbstractClient*>(toplevel);
    Deleted *deleted = dynamic_cast<Deleted*>(toplevel);
    if (!client && !deleted) {
        return;
    }

    bool noBorder = true;
    const SceneQPainterDecorationRenderer *renderer = nullptr;
    QRect dtr, dlr, drr, dbr;
    if (client && !client->noBorder()) {
        if (client->isDecorated()) {
            if (SceneQPainterDecorationRenderer *r = static_cast<SceneQPainterDecorationRenderer *>(client->decoratedClient()->renderer())) {
                r->render();
                renderer = r;
            }
        }
        client->layoutDecorationRects(dlr, dtr, drr, dbr);
        noBorder = false;
    } else if (deleted && !deleted->noBorder()) {
        noBorder = false;
        deleted->layoutDecorationRects(dlr, dtr, drr, dbr);
        renderer = static_cast<const SceneQPainterDecorationRenderer *>(deleted->decorationRenderer());
    }
    if (noBorder || !renderer) {
        return;
    }

    painter->drawImage(dtr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Top));
    painter->drawImage(dlr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Left));
    painter->drawImage(drr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Right));
    painter->drawImage(dbr, renderer->image(SceneQPainterDecorationRenderer::DecorationPart::Bottom));
}

WindowPixmap *SceneQPainter::Window::createWindowPixmap()
{
    return new QPainterWindowPixmap(this);
}

Decoration::Renderer *SceneQPainter::createDecorationRenderer(Decoration::DecoratedClientImpl *impl)
{
    return new SceneQPainterDecorationRenderer(impl);
}

//****************************************
// QPainterWindowPixmap
//****************************************
QPainterWindowPixmap::QPainterWindowPixmap(Scene::Window *window)
    : WindowPixmap(window)
{
}

QPainterWindowPixmap::QPainterWindowPixmap(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface, WindowPixmap *parent)
    : WindowPixmap(subSurface, parent)
{
}

QPainterWindowPixmap::~QPainterWindowPixmap()
{
}

void QPainterWindowPixmap::create()
{
    if (isValid()) {
        return;
    }
    KWin::WindowPixmap::create();
    if (!isValid()) {
        return;
    }
    // performing deep copy, this could probably be improved
    m_image = buffer()->data().copy();
    if (auto s = surface()) {
        s->resetTrackedDamage();
    }
}

WindowPixmap *QPainterWindowPixmap::createChild(const QPointer<KWayland::Server::SubSurfaceInterface> &subSurface)
{
    return new QPainterWindowPixmap(subSurface, this);
}

void QPainterWindowPixmap::updateBuffer()
{
    const auto oldBuffer = buffer();
    WindowPixmap::updateBuffer();
    const auto &b = buffer();
    if (b.isNull()) {
        m_image = QImage();
        return;
    }
    if (b == oldBuffer) {
        return;
    }
    // perform deep copy
    m_image = b->data().copy();
    if (auto s = surface()) {
        s->resetTrackedDamage();
    }
}

bool QPainterWindowPixmap::isValid() const
{
    if (!m_image.isNull()) {
        return true;
    }
    return WindowPixmap::isValid();
}

QPainterEffectFrame::QPainterEffectFrame(EffectFrameImpl *frame, SceneQPainter *scene)
    : Scene::EffectFrame(frame)
    , m_scene(scene)
{
}

QPainterEffectFrame::~QPainterEffectFrame()
{
}

void QPainterEffectFrame::render(QRegion region, double opacity, double frameOpacity)
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
    if (hasDecorationShadow()) {
        // TODO: implement for QPainter
        return false;
    }
    return true;
}

//****************************************
// QPainterDecorationRenderer
//****************************************
SceneQPainterDecorationRenderer::SceneQPainterDecorationRenderer(Decoration::DecoratedClientImpl *client)
    : Renderer(client)
{
    connect(this, &Renderer::renderScheduled, client->client(), static_cast<void (AbstractClient::*)(const QRect&)>(&AbstractClient::addRepaint));
}

SceneQPainterDecorationRenderer::~SceneQPainterDecorationRenderer() = default;

QImage SceneQPainterDecorationRenderer::image(SceneQPainterDecorationRenderer::DecorationPart part) const
{
    Q_ASSERT(part != DecorationPart::Count);
    return m_images[int(part)];
}

void SceneQPainterDecorationRenderer::render()
{
    const QRegion scheduled = getScheduled();
    if (scheduled.isEmpty()) {
        return;
    }
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

    const QRect geometry = scheduled.boundingRect();
    auto renderPart = [this](const QRect &rect, const QRect &partRect, int index) {
        if (rect.isEmpty()) {
            return;
        }
        QPainter painter(&m_images[index]);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setWindow(QRect(partRect.topLeft(), partRect.size() * m_images[index].devicePixelRatio()));
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
        auto dpr = client()->client()->screenScale();
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

void SceneQPainterDecorationRenderer::reparent(Deleted *deleted)
{
    render();
    Renderer::reparent(deleted);
}


QPainterFactory::QPainterFactory(QObject *parent)
    : SceneFactory(parent)
{
}

QPainterFactory::~QPainterFactory() = default;

Scene *QPainterFactory::create(QObject *parent) const
{
    auto s = SceneQPainter::createScene(parent);
    if (s && s->initFailed()) {
        delete s;
        s = nullptr;
    }
    return s;
}

} // KWin
