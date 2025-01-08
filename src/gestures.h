/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"
#include <kwin_export.h>

#include <QList>
#include <QMap>
#include <QObject>
#include <QPointF>

namespace KWin
{

class Gesture : public QObject
{
    Q_OBJECT
public:
    ~Gesture() override;

protected:
    explicit Gesture();

Q_SIGNALS:
    /**
     * Matching of a gesture started and this Gesture might match.
     * On further evaluation either the signal @ref triggered or
     * @ref cancelled will get emitted.
     */
    void started();
    /**
     * Gesture matching ended and this Gesture matched.
     */
    void triggered();
    /**
     * This Gesture no longer matches.
     */
    void cancelled();
};

class SwipeGesture : public Gesture
{
    Q_OBJECT
public:
    static constexpr double s_minimumDelta = 200;

    explicit SwipeGesture(uint32_t fingerCount);
    ~SwipeGesture() override;

    uint32_t fingerCount() const;

    SwipeDirection direction() const;
    void setDirection(SwipeDirection direction);

    qreal deltaToProgress(const QPointF &delta) const;
    bool minimumDeltaReached(const QPointF &delta) const;

Q_SIGNALS:
    /**
     * The progress of the gesture if a minimumDelta is set.
     * The progress is reported in [0.0,1.0]
     */
    void progress(qreal);

    /**
     * The progress in actual pixel distance traveled by the fingers
     */
    void deltaProgress(const QPointF &delta);

private:
    const uint32_t m_fingerCount;
    SwipeDirection m_direction = SwipeDirection::Down;
};

class PinchGesture : public Gesture
{
    Q_OBJECT
public:
    /**
     * Every time the scale of the gesture changes by this much, the callback changes by 1.
     * This is the amount of change for 1 unit of change, like switch by 1 desktop.
     */
    static constexpr double s_minimumScaleDelta = 0.2;

    explicit PinchGesture(uint32_t fingerCount);
    ~PinchGesture() override;

    uint32_t fingerCount() const;

    PinchDirection direction() const;
    void setDirection(PinchDirection direction);

    qreal scaleDeltaToProgress(const qreal &scaleDelta) const;
    bool minimumScaleDeltaReached(const qreal &scaleDelta) const;

Q_SIGNALS:
    /**
     * The progress of the gesture if a minimumDelta is set.
     * The progress is reported in [0.0,1.0]
     */
    void progress(qreal);

private:
    const uint32_t m_fingerCount;
    PinchDirection m_direction = PinchDirection::Expanding;
};

class KWIN_EXPORT GestureRecognizer
{
public:
    void registerSwipeGesture(SwipeGesture *gesture);
    void unregisterSwipeGesture(SwipeGesture *gesture);
    void registerPinchGesture(PinchGesture *gesture);
    void unregisterPinchGesture(PinchGesture *gesture);

    int startSwipeGesture(uint fingerCount);
    void updateSwipeGesture(const QPointF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

    int startPinchGesture(uint fingerCount);
    void updatePinchGesture(qreal scale, qreal angleDelta, const QPointF &posDelta);
    void cancelPinchGesture();
    void endPinchGesture();

private:
    void cancelActiveGestures();
    enum class Axis {
        Horizontal,
        Vertical,
        None,
    };
    int startSwipeGesture(uint fingerCount, const QPointF &startPos);
    QList<SwipeGesture *> m_swipeGestures;
    QList<PinchGesture *> m_pinchGestures;
    QList<SwipeGesture *> m_activeSwipeGestures;
    QList<PinchGesture *> m_activePinchGestures;

    QPointF m_currentDelta = QPointF(0, 0);
    qreal m_currentScale = 1; // For Pinch Gesture recognition
    uint m_currentFingerCount = 0;
    Axis m_currentSwipeAxis = Axis::None;
};
}
