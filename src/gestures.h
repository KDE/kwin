/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include <QMap>
#include <QObject>
#include <QPointF>
#include <QVector>

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
    explicit Gesture(QObject *parent);

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
    enum class Direction {
        Down,
        Left,
        Up,
        Right,
    };

    explicit SwipeGesture(QObject *parent = nullptr);
    ~SwipeGesture() override;

    bool minimumFingerCountIsRelevant() const;
    void setMinimumFingerCount(uint count);
    uint minimumFingerCount() const;

    bool maximumFingerCountIsRelevant() const;
    void setMaximumFingerCount(uint count);
    uint maximumFingerCount() const;

    Direction direction() const;
    void setDirection(Direction direction);

    void setMinimumX(int x);
    int minimumX() const;
    bool minimumXIsRelevant() const;
    void setMinimumY(int y);
    int minimumY() const;
    bool minimumYIsRelevant() const;

    void setMaximumX(int x);
    int maximumX() const;
    bool maximumXIsRelevant() const;
    void setMaximumY(int y);
    int maximumY() const;
    bool maximumYIsRelevant() const;
    void setStartGeometry(const QRect &geometry);

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
    bool m_minimumFingerCountRelevant = false;
    uint m_minimumFingerCount = 0;
    bool m_maximumFingerCountRelevant = false;
    uint m_maximumFingerCount = 0;
    Direction m_direction = Direction::Down;
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
    enum class Direction {
        Expanding,
        Contracting,
    };

    explicit PinchGesture(QObject *parent = nullptr);
    ~PinchGesture() override;

    bool minimumFingerCountIsRelevant() const;
    void setMinimumFingerCount(uint count);
    uint minimumFingerCount() const;

    bool maximumFingerCountIsRelevant() const;
    void setMaximumFingerCount(uint count);
    uint maximumFingerCount() const;

    Direction direction() const;
    void setDirection(Direction direction);

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
    bool m_minimumFingerCountRelevant = false;
    uint m_minimumFingerCount = 0;
    bool m_maximumFingerCountRelevant = false;
    uint m_maximumFingerCount = 0;
    Direction m_direction = Direction::Expanding;
    bool m_minimumScaleDeltaRelevant = false;
    qreal m_minimumScaleDelta = DEFAULT_UNIT_SCALE_DELTA;
};

class KWIN_EXPORT GestureRecognizer : public QObject
{
    Q_OBJECT
public:
    GestureRecognizer(QObject *parent = nullptr);
    ~GestureRecognizer() override;

    void registerSwipeGesture(SwipeGesture *gesture);
    void unregisterSwipeGesture(SwipeGesture *gesture);
    void registerPinchGesture(PinchGesture *gesture);
    void unregisterPinchGesture(PinchGesture *gesture);

    int startSwipeGesture(uint fingerCount);
    int startSwipeGesture(const QPointF &startPos);

    void updateSwipeGesture(const QPointF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

    int startPinchGesture(uint fingerCount);
    void updatePinchGesture(qreal scale, qreal angleDelta, const QPointF &posDelta);
    void cancelPinchGesture();
    void endPinchGesture();

private:
    void cancelActiveGestures();
    enum class StartPositionBehavior {
        Relevant,
        Irrelevant,
    };
    enum class Axis {
        Horizontal,
        Vertical,
        None,
    };
    int startSwipeGesture(uint fingerCount, const QPointF &startPos, StartPositionBehavior startPosBehavior);
    QVector<SwipeGesture *> m_swipeGestures;
    QVector<PinchGesture *> m_pinchGestures;
    QVector<SwipeGesture *> m_activeSwipeGestures;
    QVector<PinchGesture *> m_activePinchGestures;
    QMap<Gesture *, QMetaObject::Connection> m_destroyConnections;

    QPointF m_currentDelta = QPointF(0, 0);
    qreal m_currentScale = 1; // For Pinch Gesture recognition
    uint m_currentFingerCount = 0;
    Axis m_currentSwipeAxis = Axis::None;
};

}

Q_DECLARE_METATYPE(KWin::SwipeGesture::Direction)
Q_DECLARE_METATYPE(KWin::PinchGesture::Direction)
