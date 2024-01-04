/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/shadowitem.h"
#include "compositor.h"
#include "scene/workspacescene.h"
#include "shadow.h"
#include "window.h"

namespace KWin
{

ShadowTextureProvider::ShadowTextureProvider(Shadow *shadow)
    : m_shadow(shadow)
{
}

ShadowTextureProvider::~ShadowTextureProvider()
{
}

ShadowItem::ShadowItem(Shadow *shadow, Window *window, Scene *scene, Item *parent)
    : Item(scene, parent)
    , m_window(window)
    , m_shadow(shadow)
    , m_textureProvider(Compositor::self()->scene()->createShadowTextureProvider(shadow))
{
    connect(shadow, &Shadow::offsetChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::rectChanged, this, &ShadowItem::updateGeometry);
    connect(shadow, &Shadow::textureChanged, this, &ShadowItem::handleTextureChanged);

    updateGeometry();
    handleTextureChanged();
}

ShadowItem::~ShadowItem()
{
}

Shadow *ShadowItem::shadow() const
{
    return m_shadow;
}

ShadowTextureProvider *ShadowItem::textureProvider() const
{
    return m_textureProvider.get();
}

void ShadowItem::updateGeometry()
{
    const QRectF rect = m_shadow->rect() + m_shadow->offset();

    setPosition(rect.topLeft());
    setSize(rect.size());
    discardQuads();
}

QRectF ShadowItem::toTextureGeometry(const QRectF &logicalRect) const
{
    const qreal dpr = m_shadow->devicePixelRatio();
    const qreal topLeftX = std::round(logicalRect.topLeft().x() * dpr);
    const qreal topLeftY = std::round(logicalRect.topLeft().y() * dpr);
    const qreal bottomRightX = std::round(logicalRect.bottomRight().x() * dpr);
    const qreal bottomRightY = std::round(logicalRect.bottomRight().y() * dpr);
    return QRectF(QPointF(topLeftX, topLeftY), QPointF(bottomRightX, bottomRightY));
}

void ShadowItem::handleTextureChanged()
{
    scheduleRepaint(rect());
    discardQuads();
    m_textureDirty = true;
}

static inline void distributeHorizontally(QRectF &leftRect, QRectF &rightRect)
{
    if (leftRect.right() > rightRect.left()) {
        const qreal boundedRight = std::min(leftRect.right(), rightRect.right());
        const qreal boundedLeft = std::max(leftRect.left(), rightRect.left());
        const qreal halfOverlap = (boundedRight - boundedLeft) / 2.0;
        leftRect.setRight(boundedRight - halfOverlap);
        rightRect.setLeft(boundedLeft + halfOverlap);
    }
}

static inline void distributeVertically(QRectF &topRect, QRectF &bottomRect)
{
    if (topRect.bottom() > bottomRect.top()) {
        const qreal boundedBottom = std::min(topRect.bottom(), bottomRect.bottom());
        const qreal boundedTop = std::max(topRect.top(), bottomRect.top());
        const qreal halfOverlap = (boundedBottom - boundedTop) / 2.0;
        topRect.setBottom(boundedBottom - halfOverlap);
        bottomRect.setTop(boundedTop + halfOverlap);
    }
}

WindowQuadList ShadowItem::buildQuads() const
{
    // Do not draw shadows if window width or window height is less than 5 px. 5 is an arbitrary choice.
    if (!m_window->wantsShadowToBeRendered() || m_window->width() < 5 || m_window->height() < 5) {
        return WindowQuadList();
    }

    const QSizeF top(m_shadow->elementSize(Shadow::ShadowElementTop));
    const QSizeF topRight(m_shadow->elementSize(Shadow::ShadowElementTopRight));
    const QSizeF right(m_shadow->elementSize(Shadow::ShadowElementRight));
    const QSizeF bottomRight(m_shadow->elementSize(Shadow::ShadowElementBottomRight));
    const QSizeF bottom(m_shadow->elementSize(Shadow::ShadowElementBottom));
    const QSizeF bottomLeft(m_shadow->elementSize(Shadow::ShadowElementBottomLeft));
    const QSizeF left(m_shadow->elementSize(Shadow::ShadowElementLeft));
    const QSizeF topLeft(m_shadow->elementSize(Shadow::ShadowElementTopLeft));

    const QMarginsF shadowMargins(
        std::max({topLeft.width(), left.width(), bottomLeft.width()}),
        std::max({topLeft.height(), top.height(), topRight.height()}),
        std::max({topRight.width(), right.width(), bottomRight.width()}),
        std::max({bottomRight.height(), bottom.height(), bottomLeft.height()}));

    const QRectF outerRect = rect();

    const qreal width = shadowMargins.left() + std::max(top.width(), bottom.width()) + shadowMargins.right();
    const qreal height = shadowMargins.top() + std::max(left.height(), right.height()) + shadowMargins.bottom();

    QRectF topLeftRect;
    if (!topLeft.isEmpty()) {
        topLeftRect = QRectF(outerRect.topLeft(), topLeft);
    } else {
        topLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                             outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF topRightRect;
    if (!topRight.isEmpty()) {
        topRightRect = QRectF(outerRect.right() - topRight.width(), outerRect.top(),
                              topRight.width(), topRight.height());
    } else {
        topRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                              outerRect.top() + shadowMargins.top(), 0, 0);
    }

    QRectF bottomRightRect;
    if (!bottomRight.isEmpty()) {
        bottomRightRect = QRectF(outerRect.right() - bottomRight.width(),
                                 outerRect.bottom() - bottomRight.height(),
                                 bottomRight.width(), bottomRight.height());
    } else {
        bottomRightRect = QRectF(outerRect.right() - shadowMargins.right(),
                                 outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }

    QRectF bottomLeftRect;
    if (!bottomLeft.isEmpty()) {
        bottomLeftRect = QRectF(outerRect.left(), outerRect.bottom() - bottomLeft.height(),
                                bottomLeft.width(), bottomLeft.height());
    } else {
        bottomLeftRect = QRectF(outerRect.left() + shadowMargins.left(),
                                outerRect.bottom() - shadowMargins.bottom(), 0, 0);
    }
    // Re-distribute the corner tiles so no one of them is overlapping with others.
    // By doing this, we assume that shadow's corner tiles are symmetric
    // and it is OK to not draw top/right/bottom/left tile between corners.
    // For example, let's say top-left and top-right tiles are overlapping.
    // In that case, the right side of the top-left tile will be shifted to left,
    // the left side of the top-right tile will shifted to right, and the top
    // tile won't be rendered.
    distributeHorizontally(topLeftRect, topRightRect);
    distributeHorizontally(bottomLeftRect, bottomRightRect);
    distributeVertically(topLeftRect, bottomLeftRect);
    distributeVertically(topRightRect, bottomRightRect);

    WindowQuadList quads;
    quads.reserve(8);

    if (topLeftRect.isValid()) {
        const QPointF t1 = QPointF(0, 0);
        const QPointF t2 = QPointF(topLeftRect.width(), topLeftRect.height());
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        WindowQuad topLeftQuad = WindowQuad::fromRect(topLeftRect, textureRect);
        quads.append(topLeftQuad);
    }

    if (topRightRect.isValid()) {
        const QPointF t1 = QPointF(width - topRightRect.width(), 0);
        const QPointF t2 = QPointF(width, topRightRect.height());
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        WindowQuad topRightQuad = WindowQuad::fromRect(topRightRect, textureRect);
        quads.append(topRightQuad);
    }

    if (bottomRightRect.isValid()) {
        const QPointF t1 = QPointF(width - bottomRightRect.width(), height - bottomRightRect.height());
        const QPointF t2 = QPointF(width, height);
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        WindowQuad bottomRightQuad = WindowQuad::fromRect(bottomRightRect, textureRect);
        quads.append(bottomRightQuad);
    }

    if (bottomLeftRect.isValid()) {
        const QPointF t1 = QPointF(0, height - bottomLeftRect.height());
        const QPointF t2 = QPointF(bottomLeftRect.width(), height);
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        WindowQuad bottomLeftQuad = WindowQuad::fromRect(bottomLeftRect, textureRect);
        quads.append(bottomLeftQuad);
    }

    QRectF topRect(QPointF(topLeftRect.right(), outerRect.top()),
                   QPointF(topRightRect.left(), outerRect.top() + top.height()));

    QRectF rightRect(QPointF(outerRect.right() - right.width(), topRightRect.bottom()),
                     QPointF(outerRect.right(), bottomRightRect.top()));

    QRectF bottomRect(QPointF(bottomLeftRect.right(), outerRect.bottom() - bottom.height()),
                      QPointF(bottomRightRect.left(), outerRect.bottom()));

    QRectF leftRect(QPointF(outerRect.left(), topLeftRect.bottom()),
                    QPointF(outerRect.left() + left.width(), bottomLeftRect.top()));

    // Re-distribute left/right and top/bottom shadow tiles so they don't
    // overlap when the window is too small. Please notice that we don't
    // fix overlaps between left/top(left/bottom, right/top, and so on)
    // corner tiles because corresponding counter parts won't be valid when
    // the window is too small, which means they won't be rendered.
    distributeHorizontally(leftRect, rightRect);
    distributeVertically(topRect, bottomRect);

    if (topRect.isValid()) {
        const QPointF t1 = QPointF(shadowMargins.left(), 0);
        const QPointF t2 = QPointF(t1.x() + top.width(), topRect.height());
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        const WindowQuad topQuad = WindowQuad::fromRect(topRect, textureRect);
        quads.append(topQuad);
    }

    if (rightRect.isValid()) {
        const QPointF t1 = QPointF(width - rightRect.width(), shadowMargins.top());
        const QPointF t2 = QPointF(width, t1.y() - right.height());
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        const WindowQuad rightQuad = WindowQuad::fromRect(rightRect, textureRect);
        quads.append(rightQuad);
    }

    if (bottomRect.isValid()) {
        const QPointF t1 = QPointF(shadowMargins.left(), height - bottom.height());
        const QPointF t2 = QPointF(t1.x() + bottom.width(), height);
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        const WindowQuad bottomQuad = WindowQuad::fromRect(bottomRect, textureRect);
        quads.append(bottomQuad);
    }

    if (leftRect.isValid()) {
        const QPointF t1 = QPointF(0, shadowMargins.top());
        const QPointF t2 = QPointF(leftRect.width(), t1.y() - left.height());
        const QRectF textureRect = toTextureGeometry(QRectF(t1, t2));
        const WindowQuad leftQuad = WindowQuad::fromRect(leftRect, textureRect);
        quads.append(leftQuad);
    }

    return quads;
}

void ShadowItem::preprocess()
{
    if (m_textureDirty) {
        m_textureDirty = false;
        m_textureProvider->update();
    }
}

} // namespace KWin

#include "moc_shadowitem.cpp"
