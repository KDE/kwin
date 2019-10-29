/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Filip Wieladek <wattos@gmail.com>
Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_TOUCHPOINTS_H
#define KWIN_TOUCHPOINTS_H

#include <kwineffects.h>

namespace KWin
{

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
    void prePaintScreen(ScreenPrePaintData& data, int time) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    // for properties
    qreal lineWidth() const {
        return m_lineWidth;
    }
    int ringLife() const {
        return m_ringLife;
    }
    int ringSize() const {
        return m_ringMaxSize;
    }
    int ringCount() const {
        return m_ringCount;
    }

private:
    inline void drawCircle(const QColor& color, float cx, float cy, float r);
    inline void paintScreenSetup(int mask, QRegion region, ScreenPaintData& data);
    inline void paintScreenFinish(int mask, QRegion region, ScreenPaintData& data);

    void repaint();

    float computeAlpha(int time, int ring);
    float computeRadius(int time, bool press, int ring);
    void drawCircleGl(const QColor& color, float cx, float cy, float r);
    void drawCircleXr(const QColor& color, float cx, float cy, float r);
    void drawCircleQPainter(const QColor& color, float cx, float cy, float r);
    void paintScreenSetupGl(int mask, QRegion region, ScreenPaintData& data);
    void paintScreenFinishGl(int mask, QRegion region, ScreenPaintData& data);

    Qt::GlobalColor colorForId(quint32 id);

    int m_ringCount = 2;
    float m_lineWidth = 1.0;
    int m_ringLife = 300;
    float m_ringMaxSize = 20.0;

    struct TouchPoint {
        QPointF pos;
        int time = 0;
        bool press;
        QColor color;
    };
    QVector<TouchPoint> m_points;
    QHash<quint32, QPointF> m_latestPositions;
    QHash<quint32, Qt::GlobalColor> m_colors;

};

} // namespace

#endif
