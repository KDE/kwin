/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touchpoints.h"

#include "core/output.h"
#include "effect/effecthandler.h"
#include "scene/outlinedborderitem.h"
#include "scene/workspacescene.h"

namespace KWin
{

TouchPointsEffect::TouchPointsEffect()
    : Effect()
{
}

TouchPointsEffect::~TouchPointsEffect() = default;

static const Qt::GlobalColor s_colors[] = {
    Qt::blue,
    Qt::red,
    Qt::green,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::gray,
    Qt::darkBlue,
    Qt::darkRed,
    Qt::darkGreen};

Qt::GlobalColor TouchPointsEffect::colorForId(quint32 id)
{
    auto it = m_colors.constFind(id);
    if (it != m_colors.constEnd()) {
        return it.value();
    }
    static int s_colorIndex = -1;
    s_colorIndex = (s_colorIndex + 1) % 10;
    m_colors.insert(id, s_colors[s_colorIndex]);
    return s_colors[s_colorIndex];
}

void TouchPointsEffect::addPoint(quint32 id, const QPointF &pos, bool press)
{
    TouchPoint point;
    point.pos = pos;
    point.press = press;
    point.color = colorForId(id);
    for (int i = 0; i < m_ringCount; ++i) {
        if (m_freeRings.empty()) {
            point.rings.push_back(std::make_unique<OutlinedBorderItem>(RectF(), BorderOutline(), effects->scene()->overlayItem()));
        } else {
            point.rings.push_back(std::move(m_freeRings.back()));
            m_freeRings.pop_back();
        }
    }
    updateRings(point);
    m_points.push_back(std::move(point));
    repaint();
}

void TouchPointsEffect::updateRings(TouchPoint &point)
{
    LogicalOutput *output = effects->screenAt(point.pos.toPoint());
    // The line width is in device pixels
    const qreal lineWidth = m_lineWidth / (output ? output->scale() : 1.0);
    for (int i = 0; i < m_ringCount; ++i) {
        OutlinedBorderItem *ring = point.rings[i].get();
        const float alpha = computeAlpha(point.time, i);
        const float radius = computeRadius(point.time, point.press, i);
        if (radius <= 0 || alpha <= 0) {
            ring->setVisible(false);
            continue;
        }
        // The line is centered on the circle of this radius. A circle smaller than the line is
        // drawn as a dot, with an inner radius just above zero.
        const qreal outerRadius = radius + lineWidth / 2;
        const qreal innerRadius = std::max<qreal>(radius - lineWidth / 2, 0.01);
        ring->setOutline(BorderOutline(outerRadius - innerRadius, point.color, BorderRadius(innerRadius)));
        ring->setInnerRect(RectF(point.pos.x() - innerRadius, point.pos.y() - innerRadius, 2 * innerRadius, 2 * innerRadius));
        ring->setOpacity(alpha);
        ring->setVisible(true);
    }
}

bool TouchPointsEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    addPoint(id, pos, true);
    m_latestPositions.insert(id, pos);
    return false;
}

bool TouchPointsEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    addPoint(id, pos, true);
    m_latestPositions.insert(id, pos);
    return false;
}

bool TouchPointsEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    auto it = m_latestPositions.constFind(id);
    if (it != m_latestPositions.constEnd()) {
        addPoint(id, it.value(), false);
        m_latestPositions.erase(it);
    }
    return false;
}

void TouchPointsEffect::prePaintScreen(ScreenPrePaintData &data)
{
    const int time = m_clock.tickMs(data.view).count();

    std::erase_if(m_points, [this, time](TouchPoint &point) {
        point.time += time;
        if (point.time <= m_ringLife) {
            return false;
        }
        // Keep the rings for the next points, which come with every touch motion
        for (auto &ring : point.rings) {
            ring->setVisible(false);
            m_freeRings.push_back(std::move(ring));
        }
        return true;
    });
    for (TouchPoint &point : m_points) {
        updateRings(point);
    }

    if (m_points.empty()) {
        m_clock.reset();
    }

    effects->prePaintScreen(data);
}

void TouchPointsEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

void TouchPointsEffect::repaint()
{
    if (m_points.empty()) {
        return;
    }
    // The rings of a press start with a radius of 0 and are hidden, so the area is repainted to
    // get the next frame. A single bounding rect keeps the dirty Region to one rectangle, avoiding
    // the superlinear Region::unite() cost while many touch points are moving.
    const int radius = m_ringMaxSize + m_lineWidth;
    qreal minX = m_points.front().pos.x();
    qreal maxX = minX;
    qreal minY = m_points.front().pos.y();
    qreal maxY = minY;
    for (const TouchPoint &point : m_points) {
        minX = qMin(minX, point.pos.x());
        maxX = qMax(maxX, point.pos.x());
        minY = qMin(minY, point.pos.y());
        maxY = qMax(maxY, point.pos.y());
    }
    effects->addRepaint(RectF(QPointF(minX, minY), QPointF(maxX, maxY))
                            .roundedOut()
                            .grownBy(QMargins(radius, radius, radius, radius)));
}

float TouchPointsEffect::computeRadius(int time, bool press, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    if (press) {
        return ((time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
    }
    return ((m_ringLife - time - ringDistance * ring) / m_ringLife) * m_ringMaxSize;
}

float TouchPointsEffect::computeAlpha(int time, int ring)
{
    float ringDistance = m_ringLife / (m_ringCount * 3);
    return (m_ringLife - (float)time - ringDistance * (ring)) / m_ringLife;
}

bool TouchPointsEffect::isActive() const
{
    return !m_points.empty();
}

} // namespace

#include "moc_touchpoints.cpp"
