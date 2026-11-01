/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/effect.h"
#include "effect/timeline.h"

#include <memory>
#include <vector>

namespace KWin
{

class OutlinedBorderItem;

class TouchPointsEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(qreal lineWidth READ lineWidth)
    Q_PROPERTY(int ringLife READ ringLife)
    Q_PROPERTY(int ringSize READ ringSize)
    Q_PROPERTY(int ringCount READ ringCount)

public:
    TouchPointsEffect();
    ~TouchPointsEffect() override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;

    // for properties
    qreal lineWidth() const
    {
        return m_lineWidth;
    }
    int ringLife() const
    {
        return m_ringLife;
    }
    int ringSize() const
    {
        return m_ringMaxSize;
    }
    int ringCount() const
    {
        return m_ringCount;
    }

private:
    struct TouchPoint;

    void addPoint(quint32 id, const QPointF &pos, bool press);
    void repaint();
    void updateRings(TouchPoint &point);
    float computeAlpha(int time, int ring);
    float computeRadius(int time, bool press, int ring);

    Qt::GlobalColor colorForId(quint32 id);

    int m_ringCount = 2;
    float m_lineWidth = 1.0;
    int m_ringLife = 300;
    float m_ringMaxSize = 20.0;

    struct TouchPoint
    {
        QPointF pos;
        int time = 0;
        bool press;
        QColor color;
        std::vector<std::unique_ptr<OutlinedBorderItem>> rings;
    };
    std::vector<TouchPoint> m_points;
    // Hidden ring items of expired points, used again by the next points
    std::vector<std::unique_ptr<OutlinedBorderItem>> m_freeRings;
    QHash<quint32, QPointF> m_latestPositions;
    QHash<quint32, Qt::GlobalColor> m_colors;
    AnimationClock m_clock;
};

} // namespace
