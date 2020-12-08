/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_GESTURES_H
#define KWIN_GESTURES_H

#include <kwin_export.h>

#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QMap>
#include <QVector>

namespace KWin
{

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
        Right
    };

    explicit SwipeGesture(QObject *parent = nullptr);
    ~SwipeGesture() override;

    bool minimumFingerCountIsRelevant() const {
        return m_minimumFingerCountRelevant;
    }
    void setMinimumFingerCount(uint count) {
        m_minimumFingerCount = count;
        m_minimumFingerCountRelevant = true;
    }
    uint minimumFingerCount() const {
        return m_minimumFingerCount;
    }

    bool maximumFingerCountIsRelevant() const {
        return m_maximumFingerCountRelevant;
    }
    void setMaximumFingerCount(uint count) {
        m_maximumFingerCount = count;
        m_maximumFingerCountRelevant = true;
    }
    uint maximumFingerCount() const {
        return m_maximumFingerCount;
    }

    Direction direction() const {
        return m_direction;
    }
    void setDirection(Direction direction) {
        m_direction = direction;
    }

    void setMinimumX(int x) {
        m_minimumX = x;
        m_minimumXRelevant = true;
    }
    int minimumX() const {
        return m_minimumX;
    }
    bool minimumXIsRelevant() const {
        return m_minimumXRelevant;
    }
    void setMinimumY(int y) {
        m_minimumY = y;
        m_minimumYRelevant = true;
    }
    int minimumY() const {
        return m_minimumY;
    }
    bool minimumYIsRelevant() const {
        return m_minimumYRelevant;
    }

    void setMaximumX(int x) {
        m_maximumX = x;
        m_maximumXRelevant = true;
    }
    int maximumX() const {
        return m_maximumX;
    }
    bool maximumXIsRelevant() const {
        return m_maximumXRelevant;
    }
    void setMaximumY(int y) {
        m_maximumY = y;
        m_maximumYRelevant = true;
    }
    int maximumY() const {
        return m_maximumY;
    }
    bool maximumYIsRelevant() const {
        return m_maximumYRelevant;
    }
    void setStartGeometry(const QRect &geometry);

    QSizeF minimumDelta() const {
        return m_minimumDelta;
    }
    void setMinimumDelta(const QSizeF &delta) {
        m_minimumDelta = delta;
        m_minimumDeltaRelevant = true;
    }
    bool isMinimumDeltaRelevant() const {
        return m_minimumDeltaRelevant;
    }

    qreal minimumDeltaReachedProgress(const QSizeF &delta) const;
    bool minimumDeltaReached(const QSizeF &delta) const;

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
    QSizeF m_minimumDelta;
};

class KWIN_EXPORT GestureRecognizer : public QObject
{
    Q_OBJECT
public:
    GestureRecognizer(QObject *parent = nullptr);
    ~GestureRecognizer() override;

    void registerGesture(Gesture *gesture);
    void unregisterGesture(Gesture *gesture);

    int startSwipeGesture(uint fingerCount) {
        return startSwipeGesture(fingerCount, QPointF(), StartPositionBehavior::Irrelevant);
    }
    int startSwipeGesture(const QPointF &startPos) {
        return startSwipeGesture(1, startPos, StartPositionBehavior::Relevant);
    }
    void updateSwipeGesture(const QSizeF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

private:
    void cancelActiveSwipeGestures();
    enum class StartPositionBehavior {
        Relevant,
        Irrelevant
    };
    int startSwipeGesture(uint fingerCount, const QPointF &startPos, StartPositionBehavior startPosBehavior);
    QVector<Gesture*> m_gestures;
    QVector<Gesture*> m_activeSwipeGestures;
    QMap<Gesture*, QMetaObject::Connection> m_destroyConnections;
    QVector<QSizeF> m_swipeUpdates;
};

}

Q_DECLARE_METATYPE(KWin::SwipeGesture::Direction)

#endif
