/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GESTURES_H
#define KWIN_GESTURES_H

#include "kwinglobals.h"
#include <kwin_export.h>

#include <QMap>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QSizeF>
#include <QVector2D>
#include <QVector>

namespace KWin
{
static const QSet<uint> DEFAULT_VALID_FINGER_COUNTS = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

/**
 * This is the amount of change for 1 unit of change, like switch by 1 desktop.
 */
static const qreal DEFAULT_UNIT_DELTA = 200; // Pixels
static const qreal DEFAULT_UNIT_SCALE_DELTA = .2; // 20%

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

    GestureType direction() const;
    void setDirection(GestureType direction);

protected:
    explicit Gesture(QObject *parent);
    GestureType m_direction;

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
     * Progress towards the minimum threshold to trigger. [0, 1]
     */
    void triggerProgress(qreal);
    /**
     * The progress is reported in [0.0,1.0+]
     * Progress is always positive
     * It can be more than 1, indicating an action should happen more than once.
     */
    void semanticProgress(qreal, GestureType);

private:
    QSet<uint> m_validFingerCounts = DEFAULT_VALID_FINGER_COUNTS;
};

class SwipeGesture : public Gesture
{
    Q_OBJECT
public:
    explicit SwipeGesture(QObject *parent = nullptr);
    ~SwipeGesture() override;

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

    /**
     * In pixels
     */
    QSizeF triggerDelta() const;
    void setTriggerDelta(const QSizeF &delta);
    bool isTriggerDeltaRelevant() const;

    /**
     * Returns the progress [0, 1] of the gesture
     * being triggered. Picks the largest possible value
     * considering each direction available to the
     * gesture.
     */
    qreal getTriggerProgress(const QSizeF &delta) const;
    bool triggerDeltaReached(const QSizeF &delta) const;

    /**
     * Take the given pixel delta and
     * map it to a simple [0, 1+] semantic scale.
     *  0 = no progress
     *  1 = complete something once
     * The value can be greater than 1, indicating
     * that the action should be done more times.
     */
    qreal getSemanticProgress(const QSizeF &delta) const;

Q_SIGNALS:
    /**
     * Summative pixel delta from where the gesture
     * started to where it is now.
     */
    void pixelDelta(const QSizeF &delta, GestureType);

private:
    bool m_minimumXRelevant = false;
    int m_minimumX = 0;
    bool m_minimumYRelevant = false;
    int m_minimumY = 0;
    bool m_maximumXRelevant = false;
    int m_maximumX = 0;
    bool m_maximumYRelevant = false;
    int m_maximumY = 0;
    bool m_triggerDeltaRelevant = false;
    QSizeF m_triggerDelta;
    qreal m_unitDelta = DEFAULT_UNIT_DELTA;
};

class PinchGesture : public Gesture
{
    Q_OBJECT
public:
    explicit PinchGesture(QObject *parent = nullptr);
    ~PinchGesture() override;

    qreal triggerScaleDelta() const;

    /**
     * scaleDelta is the % scale difference needed to trigger
     * 0.25 will trigger when scale reaches 0.75 or 1.25
     */
    void setTriggerScaleDelta(const qreal &scaleDelta);
    bool isTriggerScaleDeltaRelevant() const;

    /**
     * [0, 1]
     */
    qreal getTriggerProgress(const qreal &scaleDelta) const;
    bool triggerScaleDeltaReached(const qreal &scaleDelta) const;

    /**
     * Take the given pixel delta and
     * map it to a simple [0, 1+] semantic scale.
     *  0 = no progress
     *  1 = complete something once
     * The value can be greater than 1, indicating
     * that the action should be done more times.
     */
    qreal getSemanticProgress(const qreal scale) const;

private:
    bool m_triggerScaleDeltaRelevant = false;
    qreal m_triggerScaleDelta = .2;
    qreal m_unitScaleDelta = DEFAULT_UNIT_SCALE_DELTA;
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
