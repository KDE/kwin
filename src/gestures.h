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
    explicit PinchGesture(uint32_t fingerCount);
    ~PinchGesture() override;

    uint32_t fingerCount() const;
    /**
     * The angle threshold for a pinch gesture to be recognized as rotate
     */
    static constexpr double s_minimumRotateAngle = 5;

    bool minimumScaleDeltaReached(const qreal &scaleDelta) const;
    bool minimumAngleDeltaReached(const qreal &angleDelta) const;

Q_SIGNALS:
    /**
     * The progress of the gesture if a minimumDelta is set.
     * The progress is reported in [0.0,1.0]
     */
    void progress(qreal);

private:
    const uint32_t m_fingerCount;
};

class StraightPinchGesture : public Gesture
{
    Q_OBJECT
public:
    /**
     * Every time the scale of the gesture changes by this much, the callback changes by 1.
     * This is the amount of change for 1 unit of change, like switch by 1 desktop.
     */
    static constexpr double s_minimumScaleDelta = 0.2;

    explicit StraightPinchGesture(uint32_t fingerCount);
    ~StraightPinchGesture() override;

    uint32_t fingerCount() const;

    StraightPinchDirection direction() const;
    void setDirection(StraightPinchDirection direction);

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
    StraightPinchDirection m_direction = StraightPinchDirection::Expanding;
};

class RotatePinchGesture : public Gesture
{
    Q_OBJECT
public:
    /**
     * Every time the angle of the gesture changes by this much, the callback changes by 1.
     * This is the amount of change for 1 unit of change, like switch by 1 desktop.
     */
    static constexpr double s_minimumAngleDelta = 2;

    explicit RotatePinchGesture(uint32_t fingerCount);
    ~RotatePinchGesture() override;

    uint32_t fingerCount() const;

    RotatePinchDirection direction() const;
    void setDirection(RotatePinchDirection direction);

    qreal angleDeltaToProgress(const qreal &angleDelta) const;
    bool minimumAngleDeltaReached(const qreal &angleDelta) const;

Q_SIGNALS:
    /**
     * The progress of the gesture if a minimumDelta is set.
     * The progress is reported in [0.0,1.0]
     */
    void progress(qreal);

private:
    const uint32_t m_fingerCount;
    RotatePinchDirection m_direction = RotatePinchDirection::Clockwise;
};

class KWIN_EXPORT GestureRecognizer : public QObject
{
    Q_OBJECT
public:
    GestureRecognizer(QObject *parent = nullptr);
    ~GestureRecognizer() override;

    void registerSwipeGesture(SwipeGesture *gesture);
    void unregisterSwipeGesture(SwipeGesture *gesture);
    void registerStraightPinchGesture(StraightPinchGesture *gesture);
    void unregisterStraightPinchGesture(StraightPinchGesture *gesture);
    void registerRotatePinchGesture(RotatePinchGesture *gesture);
    void unregisterRotatePinchGesture(RotatePinchGesture *gesture);

    int startSwipeGesture(uint fingerCount);
    void updateSwipeGesture(const QPointF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

    int startPinchGesture(uint fingerCount);
    void updatePinchGesture(qreal scale, qreal angleDelta, const QPointF &posDelta);
    void cancelPinchGesture();
    void endPinchGesture();

    int startStraightPinchGesture(uint fingerCount);
    void updateStarightPinchGesture(qreal scaleData);
    void cancelStraightPinchGesture();
    void endStraightPinchGesture();

    int startRotatePinchGesture(uint fingerCount);
    void updateRotatePinchGesture(qreal angleDelta);
    void cancelRotatePinchGesture();
    void endRotatePinchGesture();

private:
    void cancelActiveGestures();
    enum class Axis {
        Horizontal,
        Vertical,
        None,
    };
    int startSwipeGesture(uint fingerCount, const QPointF &startPos);
    QList<SwipeGesture *> m_swipeGestures;
    QList<StraightPinchGesture *> m_straightPinchGestures;
    QList<RotatePinchGesture *> m_rotatePinchGestures;
    QList<SwipeGesture *> m_activeSwipeGestures;
    QList<StraightPinchGesture *> m_activeStraightPinchGestures;
    QList<RotatePinchGesture *> m_activeRotatePinchGestures;
    QMap<Gesture *, QMetaObject::Connection> m_destroyConnections;

    QPointF m_currentDelta = QPointF(0, 0);
    qreal m_currentScale = 1; // For Straight Pinch Gesture recognition
    qreal m_currentAngle = 0; // For Rotate Pinch Gesture recognition
    uint m_currentFingerCount = 0;
    Axis m_currentSwipeAxis = Axis::None;
};

}
