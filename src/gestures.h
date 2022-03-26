/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GESTURES_H
#define KWIN_GESTURES_H

#include <kwin_export.h>
#include <kwinglobals.h>

#include <QMap>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QVector>

namespace KWin
{
/*
 * Everytime the scale of the gesture changes by this much, the callback changes by 1.
 * This is the amount of change for 1 unit of change, like switch by 1 desktop.
 * */
static const qreal DEFAULT_UNIT_DELTA = 400;
static const qreal DEFAULT_UNIT_SCALE_DELTA = .2; // 20%

static const QSet<uint> DEFAULT_VALID_FINGER_COUNTS = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

class Gesture : public QObject
{
    Q_OBJECT
public:
    ~Gesture() override;

    /**
     * This gesture framework allows for one gesture to
     * capture a range of fingers.
     *
     * This function adds an acceptable number of fingers
     * to the set of finger counts this gesture can
     * be identified by.
     *
     * By default, any number of fingers are accepted.
     * (see DEFAULT_VALID_FINGER_COUNTS)
     */
    void addFingerCount(uint numFingers);
    bool isFingerCountAcceptable(uint fingers) const;
    QSet<uint> acceptableFingerCounts() const;

    /**
     * Don't emit start until the first update
     * Don't canccel if not started
     */
    bool isStarted;

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
    /**
     * The progress of the gesture if a minimumDelta is set.
     * The progress is reported in [0.0,1.0+]
     * Progress is always positive,
     * It can be more than 1
     */
    void semanticProgress(qreal, GestureDirection);
    /**
     * Progress towards the minimum threshold to trigger
     */
    void progress(qreal);
    /**
     * Like semantic progress except [-1, 1] and
     * it captures both of something
     * example: Up and Down, Contracting and Expanding
     */
    void semanticProgressAxis(qreal, GestureDirection);

private:
    QSet<uint> m_validFingerCounts = DEFAULT_VALID_FINGER_COUNTS;
};

class SwipeGesture : public Gesture
{
    Q_OBJECT
public:

    explicit SwipeGesture(QObject *parent = nullptr);
    ~SwipeGesture() override;

    GestureDirection direction() const;
    void setDirection(GestureDirection direction);

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

    QSizeF minimumDelta() const;
    void setMinimumDelta(const QSizeF &delta);
    bool isMinimumDeltaRelevant() const;

    /**
     * Progress reaching trigger threshold.
     */
    qreal deltaToProgress(const QSizeF &delta) const;
    bool minimumDeltaReached(const QSizeF &delta) const;

    /**
     * Take the given pixel delta and
     * map it to a simple [0, 1+] semantic scale.
     *  0 = no progress
     *  1 = complete something once
     * The value can be greater than 1, indicating
     * that the action should be done more times.
     */
    qreal getSemanticProgress(const QSizeF &delta) const;
    /**
     * Like the last one, except [-1, 1]
     */
    qreal getSemanticAxisProgress(const QSizeF &delta);
    /**
     * A two dimensional semantic delta.
     * [-1, 1] on each axis.
     */
    QSizeF getSemanticDelta(const QSizeF &delta) const;

Q_SIGNALS:

    /**
     * The progress in actual pixel distance traveled by the fingers
     */
    void pixelDelta(const QSizeF &delta, GestureDirection);
    /**
     * Just the pixel delta.
     */
    void deltaProgress(const QSizeF &delta);

    void semanticDelta(const QSizeF &delta, GestureDirection);

private:
    GestureDirection m_direction = GestureDirection::Down;
    bool m_minimumXRelevant = false;
    int m_minimumX = 0;
    bool m_minimumYRelevant = false;
    int m_minimumY = 0;
    bool m_maximumXRelevant = false;
    int m_maximumX = 0;
    bool m_maximumYRelevant = false;
    int m_maximumY = 0;
    bool m_minimumDeltaRelevant = false;
    QSizeF m_minimumDelta;
    qreal m_unitDelta = DEFAULT_UNIT_DELTA;
};

class PinchGesture : public Gesture
{
    Q_OBJECT
public:

    explicit PinchGesture(QObject *parent = nullptr);
    ~PinchGesture() override;

    GestureDirection direction() const;
    void setDirection(GestureDirection direction);

    /**
     * Progress reaching trigger threshold.
     */
    qreal deltaToProgress(const qreal &scale) const;
    /**
     * Have we reached the minimum required delta to trigger this gesture?
     */
    bool minimumScaleDeltaReached(const qreal &scaleDelta) const;
    void setMinimumScaleDelta(const qreal &scaleDelta);

    /**
     * Does the minimum scale delta matter
     */
    bool isMinimumScaleDeltaRelevant() const;
    qreal minimumScaleDelta() const;

    /**
     * Take the given pixel delta and
     * map it to a simple [0, 1+] semantic scale.
     *  0 = no progress
     *  1 = complete something once
     * The value can be greater than 1, indicating
     * that the action should be done more times.
     */
    qreal getSemanticProgress(const qreal scale) const;

    /**
     * Like the last one, except [-1, 1]
     * Positive is expanding.
     */
    qreal getSemanticAxisProgress(const qreal scale);

Q_SIGNALS:

private:
    GestureDirection m_direction = GestureDirection::Expanding;
    bool m_minimumScaleDeltaRelevant = false;
    qreal m_minimumScaleDelta = .2;
    qreal m_unitDelta = DEFAULT_UNIT_SCALE_DELTA;
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

    void updateSwipeGesture(const QSizeF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

    int startPinchGesture(uint fingerCount);
    void updatePinchGesture(qreal scale, qreal angleDelta, const QSizeF &posDelta);
    void cancelPinchGesture();
    void endPinchGesture();

private:
    void cancelActiveGestures();
    bool mutuallyExclusive(GestureDirection d, GestureDirection gestureDir);
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

    QSizeF m_currentDelta = QSizeF(0, 0);
    qreal m_currentScale = 1; // For Pinch Gesture recognition
    uint m_currentFingerCount = 0;
    Axis m_currentSwipeAxis = Axis::None;
};

}

#endif
