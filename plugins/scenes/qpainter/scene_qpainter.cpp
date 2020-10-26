/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "scene_qpainter.h"
// KWin
#include "abstract_client.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "main.h"
#include "screens.h"
#include "toplevel.h"
#include "platform.h"
#include "wayland_server.h"

#include <kwineffectquickview.h>

#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/subcompositor_interface.h>
#include <KWaylandServer/surface_interface.h>
#include "decorations/decoratedclient.h"
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
SceneQPainter *SceneQPainter::createScene(QObject *parent)
{
    QScopedPointer<QPainterBackend> backend(kwinApp()->platform()->createQPainterBackend());
    if (backend.isNull()) {
        return nullptr;
    }
    if (backend->isFailed()) {
        return nullptr;
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

void SceneQPainter::paintGenericScreen(int mask, const ScreenPaintData &data)
{
    m_painter->save();
    m_painter->translate(data.xTranslation(), data.yTranslation());
    m_painter->scale(data.xScale(), data.yScale());
    Scene::paintGenericScreen(mask, data);
    m_painter->restore();
}

qint64 SceneQPainter::paint(const QRegion &_damage, const QList<Toplevel *> &toplevels)
{
    QElapsedTimer renderTimer;
    renderTimer.start();

    createStackingOrder(toplevels);
    QRegion damage = _damage;

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
            paintCursor(updateRegion);

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

        paintCursor(updateRegion);
        m_backend->showOverlay();

        m_painter->end();
        m_backend->present(mask, updateRegion);
    }

    // do cleanup
    clearStackingOrder();

    return renderTimer.nsecsElapsed();
}

void SceneQPainter::paintBackground(const QRegion &region)
{
    m_painter->setBrush(Qt::black);
    for (const QRect &rect : region) {
        m_painter->drawRect(rect);
    }
}

void SceneQPainter::paintCursor(const QRegion &rendered)
{
    if (!kwinApp()->platform()->usesSoftwareCursor()) {
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

void SceneQPainter::paintEffectQuickView(EffectQuickView *w)
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
}

void SceneQPainter::Window::performPaint(int mask, const QRegion &_region, const WindowPaintData &data)
{
    QRegion region = _region;
    if (!(mask & (PAINT_WINDOW_TRANSFORMED | PAINT_SCREEN_TRANSFORMED)))
        region &= toplevel->visibleRect();

    if (region.isEmpty())
        return;
    QPainterWindowPixmap *pixmap = windowPixmap<QPainterWindowPixmap>();
    if (!pixmap || !pixmap->isValid()) {
        return;
    }
    toplevel->resetDamage();

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
        tempPainter.translate(toplevel->frameGeometry().topLeft() - toplevel->visibleRect().topLeft());
        painter = &tempPainter;
    }
    renderShadow(painter);
    renderWindowDecorations(painter);
    renderWindowPixmap(painter, pixmap);

    if (!opaque) {
        tempPainter.restore();
        tempPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QColor translucent(Qt::transparent);
        translucent.setAlphaF(data.opacity());
        tempPainter.fillRect(QRect(QPoint(0, 0), toplevel->visibleRect().size()), translucent);
        tempPainter.end();
        painter = scenePainter;
        painter->drawImage(toplevel->visibleRect().topLeft() - toplevel->frameGeometry().topLeft(), tempImage);
    }

    painter->restore();
}

void SceneQPainter::Window::renderWindowPixmap(QPainter *painter, QPainterWindowPixmap *windowPixmap)
{
    const QRegion shape = windowPixmap->shape();
    for (const QRectF rect : shape) {
        const QPointF windowTopLeft = windowPixmap->mapToWindow(rect.topLeft());
        const QPointF windowBottomRight = windowPixmap->mapToWindow(rect.bottomRight());

        const QPointF bufferTopLeft = windowPixmap->mapToBuffer(rect.topLeft());
        const QPointF bufferBottomRight = windowPixmap->mapToBuffer(rect.bottomRight());

        painter->drawImage(QRectF(windowTopLeft, windowBottomRight),
                           windowPixmap->image(),
                           QRectF(bufferTopLeft, bufferBottomRight));
    }

    const QVector<WindowPixmap *> children = windowPixmap->children();
    for (WindowPixmap *child : children) {
        QPainterWindowPixmap *scenePixmap = static_cast<QPainterWindowPixmap *>(child);
        if (scenePixmap->isValid()) {
            renderWindowPixmap(painter, scenePixmap);
        }
    }
}

void SceneQPainter::Window::renderShadow(QPainter* painter)
{
    if (!toplevel->shadow()) {
        return;
    }
    SceneQPainterShadow *shadow = static_cast<SceneQPainterShadow *>(toplevel->shadow());

    const QImage &shadowTexture = shadow->shadowTexture();
    const WindowQuadList &shadowQuads = shadow->shadowQuads();

    for (const auto &q : shadowQuads) {
        auto topLeft = q[0];
        auto bottomRight = q[2];
        QRectF target(topLeft.x(), topLeft.y(),
                      bottomRight.x() - topLeft.x(),
                      bottomRight.y() - topLeft.y());
        QRectF source(topLeft.textureX(), topLeft.textureY(),
                      bottomRight.textureX() - topLeft.textureX(),
                      bottomRight.textureY() - topLeft.textureY());
        painter->drawImage(target, shadowTexture, source);
    }
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

QPainterWindowPixmap::QPainterWindowPixmap(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface, WindowPixmap *parent)
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
    if (!surface()) {
        // That's an internal client.
        m_image = internalImage();
        return;
    }
    // performing deep copy, this could probably be improved
    m_image = buffer()->data().copy();
    if (auto s = surface()) {
        s->resetTrackedDamage();
    }
}

WindowPixmap *QPainterWindowPixmap::createChild(const QPointer<KWaylandServer::SubSurfaceInterface> &subSurface)
{
    return new QPainterWindowPixmap(subSurface, this);
}

void QPainterWindowPixmap::update()
{
    const auto oldBuffer = buffer();
    WindowPixmap::update();
    const auto &b = buffer();
    if (!surface()) {
        // That's an internal client.
        m_image = internalImage();
        return;
    }
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

void SceneQPainterShadow::buildQuads()
{
    // Do not draw shadows if window width or window height is less than
    // 5 px. 5 is an arbitrary choice.
    if (topLevel()->width() < 5 || topLevel()->height() < 5) {
        m_shadowQuads.clear();
        setShadowRegion(QRegion());
        return;
    }

    const QSizeF top(elementSize(ShadowElementTop));
    const QSizeF topRight(elementSize(ShadowElementTopRight));
    const QSizeF right(elementSize(ShadowElementRight));
    const QSizeF bottomRight(elementSize(ShadowElementBottomRight));
    const QSizeF bottom(elementSize(ShadowElementBottom));
    const QSizeF bottomLeft(elementSize(ShadowElementBottomLeft));
    const QSizeF left(elementSize(ShadowElementLeft));
    const QSizeF topLeft(elementSize(ShadowElementTopLeft));

    const QRectF outerRect(QPointF(-leftOffset(), -topOffset()),
                           QPointF(topLevel()->width() + rightOffset(),
                                   topLevel()->height() + bottomOffset()));

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()})
                    + std::max(top.width(), bottom.width())
                    + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()})
                     + std::max(left.height(), right.height())
                     + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    QRectF topLeftRect(outerRect.topLeft(), topLeft);
    QRectF topRightRect(outerRect.topRight() - QPointF(topRight.width(), 0), topRight);
    QRectF bottomRightRect(
        outerRect.bottomRight() - QPointF(bottomRight.width(), bottomRight.height()),
        bottomRight);
    QRectF bottomLeftRect(outerRect.bottomLeft() - QPointF(0, bottomLeft.height()), bottomLeft);

    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    bool drawTop = true;
    if (topLeftRect.right() >= topRightRect.left()) {
        const float halfOverlap = qAbs(topLeftRect.right() - topRightRect.left()) / 2;
        topLeftRect.setRight(topLeftRect.right() - std::floor(halfOverlap));
        topRightRect.setLeft(topRightRect.left() + std::ceil(halfOverlap));
        drawTop = false;
    }

    bool drawRight = true;
    if (topRightRect.bottom() >= bottomRightRect.top()) {
        const float halfOverlap = qAbs(topRightRect.bottom() - bottomRightRect.top()) / 2;
        topRightRect.setBottom(topRightRect.bottom() - std::floor(halfOverlap));
        bottomRightRect.setTop(bottomRightRect.top() + std::ceil(halfOverlap));
        drawRight = false;
    }

    bool drawBottom = true;
    if (bottomLeftRect.right() >= bottomRightRect.left()) {
        const float halfOverlap = qAbs(bottomLeftRect.right() - bottomRightRect.left()) / 2;
        bottomLeftRect.setRight(bottomLeftRect.right() - std::floor(halfOverlap));
        bottomRightRect.setLeft(bottomRightRect.left() + std::ceil(halfOverlap));
        drawBottom = false;
    }

    bool drawLeft = true;
    if (topLeftRect.bottom() >= bottomLeftRect.top()) {
        const float halfOverlap = qAbs(topLeftRect.bottom() - bottomLeftRect.top()) / 2;
        topLeftRect.setBottom(topLeftRect.bottom() - std::floor(halfOverlap));
        bottomLeftRect.setTop(bottomLeftRect.top() + std::ceil(halfOverlap));
        drawLeft = false;
    }

    qreal tx1 = 0.0,
          tx2 = 0.0,
          ty1 = 0.0,
          ty2 = 0.0;

    m_shadowQuads.clear();

    tx1 = 0.0;
    ty1 = 0.0;
    tx2 = topLeftRect.width();
    ty2 = topLeftRect.height();
    WindowQuad topLeftQuad(WindowQuadShadow);
    topLeftQuad[0] = WindowVertex(topLeftRect.left(),  topLeftRect.top(),    tx1, ty1);
    topLeftQuad[1] = WindowVertex(topLeftRect.right(), topLeftRect.top(),    tx2, ty1);
    topLeftQuad[2] = WindowVertex(topLeftRect.right(), topLeftRect.bottom(), tx2, ty2);
    topLeftQuad[3] = WindowVertex(topLeftRect.left(),  topLeftRect.bottom(), tx1, ty2);
    m_shadowQuads.append(topLeftQuad);

    tx1 = width - topRightRect.width();
    ty1 = 0.0;
    tx2 = width;
    ty2 = topRightRect.height();
    WindowQuad topRightQuad(WindowQuadShadow);
    topRightQuad[0] = WindowVertex(topRightRect.left(),  topRightRect.top(),    tx1, ty1);
    topRightQuad[1] = WindowVertex(topRightRect.right(), topRightRect.top(),    tx2, ty1);
    topRightQuad[2] = WindowVertex(topRightRect.right(), topRightRect.bottom(), tx2, ty2);
    topRightQuad[3] = WindowVertex(topRightRect.left(),  topRightRect.bottom(), tx1, ty2);
    m_shadowQuads.append(topRightQuad);

    tx1 = width - bottomRightRect.width();
    tx2 = width;
    ty1 = height - bottomRightRect.height();
    ty2 = height;
    WindowQuad bottomRightQuad(WindowQuadShadow);
    bottomRightQuad[0] = WindowVertex(bottomRightRect.left(),  bottomRightRect.top(),    tx1, ty1);
    bottomRightQuad[1] = WindowVertex(bottomRightRect.right(), bottomRightRect.top(),    tx2, ty1);
    bottomRightQuad[2] = WindowVertex(bottomRightRect.right(), bottomRightRect.bottom(), tx2, ty2);
    bottomRightQuad[3] = WindowVertex(bottomRightRect.left(),  bottomRightRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomRightQuad);

    tx1 = 0.0;
    tx2 = bottomLeftRect.width();
    ty1 = height - bottomLeftRect.height();
    ty2 = height;
    WindowQuad bottomLeftQuad(WindowQuadShadow);
    bottomLeftQuad[0] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.top(),    tx1, ty1);
    bottomLeftQuad[1] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.top(),    tx2, ty1);
    bottomLeftQuad[2] = WindowVertex(bottomLeftRect.right(), bottomLeftRect.bottom(), tx2, ty2);
    bottomLeftQuad[3] = WindowVertex(bottomLeftRect.left(),  bottomLeftRect.bottom(), tx1, ty2);
    m_shadowQuads.append(bottomLeftQuad);

    if (drawTop) {
        QRectF topRect(
            topLeftRect.topRight(),
            topRightRect.bottomLeft());
        tx1 = topLeft.width();
        ty1 = 0.0;
        tx2 = width - topRight.width();
        ty2 = topRect.height();
        WindowQuad topQuad(WindowQuadShadow);
        topQuad[0] = WindowVertex(topRect.left(),  topRect.top(),    tx1, ty1);
        topQuad[1] = WindowVertex(topRect.right(), topRect.top(),    tx2, ty1);
        topQuad[2] = WindowVertex(topRect.right(), topRect.bottom(), tx2, ty2);
        topQuad[3] = WindowVertex(topRect.left(),  topRect.bottom(), tx1, ty2);
        m_shadowQuads.append(topQuad);
    }

    if (drawRight) {
        QRectF rightRect(
            topRightRect.bottomLeft(),
            bottomRightRect.topRight());
        tx1 = width - rightRect.width();
        ty1 = topRight.height();
        tx2 = width;
        ty2 = height - bottomRight.height();
        WindowQuad rightQuad(WindowQuadShadow);
        rightQuad[0] = WindowVertex(rightRect.left(),  rightRect.top(),    tx1, ty1);
        rightQuad[1] = WindowVertex(rightRect.right(), rightRect.top(),    tx2, ty1);
        rightQuad[2] = WindowVertex(rightRect.right(), rightRect.bottom(), tx2, ty2);
        rightQuad[3] = WindowVertex(rightRect.left(),  rightRect.bottom(), tx1, ty2);
        m_shadowQuads.append(rightQuad);
    }

    if (drawBottom) {
        QRectF bottomRect(
            bottomLeftRect.topRight(),
            bottomRightRect.bottomLeft());
        tx1 = bottomLeft.width();
        ty1 = height - bottomRect.height();
        tx2 = width - bottomRight.width();
        ty2 = height;
        WindowQuad bottomQuad(WindowQuadShadow);
        bottomQuad[0] = WindowVertex(bottomRect.left(),  bottomRect.top(),    tx1, ty1);
        bottomQuad[1] = WindowVertex(bottomRect.right(), bottomRect.top(),    tx2, ty1);
        bottomQuad[2] = WindowVertex(bottomRect.right(), bottomRect.bottom(), tx2, ty2);
        bottomQuad[3] = WindowVertex(bottomRect.left(),  bottomRect.bottom(), tx1, ty2);
        m_shadowQuads.append(bottomQuad);
    }

    if (drawLeft) {
        QRectF leftRect(
            topLeftRect.bottomLeft(),
            bottomLeftRect.topRight());
        tx1 = 0.0;
        ty1 = topLeft.height();
        tx2 = leftRect.width();
        ty2 = height - bottomRight.height();
        WindowQuad leftQuad(WindowQuadShadow);
        leftQuad[0] = WindowVertex(leftRect.left(),  leftRect.top(),    tx1, ty1);
        leftQuad[1] = WindowVertex(leftRect.right(), leftRect.top(),    tx2, ty1);
        leftQuad[2] = WindowVertex(leftRect.right(), leftRect.bottom(), tx2, ty2);
        leftQuad[3] = WindowVertex(leftRect.left(),  leftRect.bottom(), tx1, ty2);
        m_shadowQuads.append(leftQuad);
    }
}

bool SceneQPainterShadow::prepareBackend()
{
    if (hasDecorationShadow()) {
        m_texture = decorationShadowImage();
        return true;
    }

    const QPixmap &topLeft     = shadowPixmap(ShadowElementTopLeft);
    const QPixmap &top         = shadowPixmap(ShadowElementTop);
    const QPixmap &topRight    = shadowPixmap(ShadowElementTopRight);
    const QPixmap &bottomLeft  = shadowPixmap(ShadowElementBottomLeft);
    const QPixmap &bottom      = shadowPixmap(ShadowElementBottom);
    const QPixmap &bottomRight = shadowPixmap(ShadowElementBottomRight);
    const QPixmap &left        = shadowPixmap(ShadowElementLeft);
    const QPixmap &right       = shadowPixmap(ShadowElementRight);

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()})
                    + std::max(top.width(), bottom.width())
                    + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()})
                     + std::max(left.height(), right.height())
                     + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return false;
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&image);
    painter.drawPixmap(0, 0, topLeft.width(), topLeft.height(), topLeft);
    painter.drawPixmap(topLeft.width(), 0, top.width(), top.height(), top);
    painter.drawPixmap(width - topRight.width(), 0, topRight.width(), topRight.height(), topRight);
    painter.drawPixmap(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height(), bottomLeft);
    painter.drawPixmap(bottomLeft.width(), height - bottom.height(), bottom.width(), bottom.height(), bottom);
    painter.drawPixmap(width - bottomRight.width(), height - bottomRight.height(), bottomRight.width(), bottomRight.height(), bottomRight);
    painter.drawPixmap(0, topLeft.height(), left.width(), left.height(), left);
    painter.drawPixmap(width - right.width(), topRight.height(), right.width(), right.height(), right);
    painter.end();

    m_texture = image;

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
