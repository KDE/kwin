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
/*
 * Everytime the scale of the gesture changes by this much, the callback changes by 1.
 * This is the amount of change for 1 unit of change, like switch by 1 desktop.
 * */
static const qreal DEFAULT_UNIT_SCALE_DELTA = .2; // 20%

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
    explicit SwipeGesture(uint32_t fingerCount);
    ~SwipeGesture() override;

    uint32_t fingerCount() const;

    SwipeDirection direction() const;
    void setDirection(SwipeDirection direction);

    QPointF minimumDelta() const;
    void setMinimumDelta(const QPointF &delta);
    bool isMinimumDeltaRelevant() const;

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
    bool m_minimumXRelevant = false;
    int m_minimumX = 0;
    bool m_minimumYRelevant = false;
    int m_minimumY = 0;
    bool m_maximumXRelevant = false;
    int m_maximumX = 0;
    bool m_maximumYRelevant = false;
    int m_maximumY = 0;
    bool m_minimumDeltaRelevant = false;
    QPointF m_minimumDelta;
};

class PinchGesture : public Gesture
{
    Q_OBJECT
public:
    explicit PinchGesture(uint32_t fingerCount);
    ~PinchGesture() override;

    uint32_t fingerCount() const;

    PinchDirection direction() const;
    void setDirection(PinchDirection direction);

    qreal minimumScaleDelta() const;

    /**
     * scaleDelta is the % scale difference needed to trigger
     * 0.25 will trigger when scale reaches 0.75 or 1.25
     */
    void setMinimumScaleDelta(const qreal &scaleDelta);
    bool isMinimumScaleDeltaRelevant() const;

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
    bool m_minimumScaleDeltaRelevant = false;
    qreal m_minimumScaleDelta = DEFAULT_UNIT_SCALE_DELTA;
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
