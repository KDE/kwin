/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"

namespace KWin
{

class ScreenEdgeGestureRecognizer;

class KWIN_EXPORT ScreenEdgeGesture : public QObject
{
    Q_OBJECT
public:
    explicit ScreenEdgeGesture(const std::shared_ptr<ScreenEdgeGestureRecognizer> &recognizer, SwipeDirection direction, const QRectF &geometry);
    ~ScreenEdgeGesture() override;

    SwipeDirection direction() const;
    void setDirection(SwipeDirection direction);

    QRectF geometry() const;
    void setGeometry(const QRectF &geometry);

Q_SIGNALS:
    void triggered();
    void started();
    void cancelled();
    void progress(const QPointF &delta, qreal progress);

private:
    const std::weak_ptr<ScreenEdgeGestureRecognizer> m_recognizer;
    SwipeDirection m_direction;
    QRectF m_geometry;
};

class KWIN_EXPORT ScreenEdgeGestureRecognizer
{
public:
    static constexpr double s_activationDistance = 44;

    void addGesture(ScreenEdgeGesture *gesture);
    void removeGesture(ScreenEdgeGesture *gesture);

    bool touchDown(uint32_t id, const QPointF &pos);
    bool touchMotion(uint32_t id, const QPointF &pos);
    bool touchUp(uint32_t id);
    void touchCancel();

private:
    std::vector<ScreenEdgeGesture *> m_gestures;
    std::vector<uint32_t> m_touchPoints;
    std::optional<uint32_t> m_touchId;
    QPointF m_startPosition;
    double m_currentDistance = 0;
    ScreenEdgeGesture *m_currentGesture = nullptr;
};

}
