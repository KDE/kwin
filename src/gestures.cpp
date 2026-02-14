/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gestures.h"
#include "effect/globals.h"

#include <QDebug>

#include <cmath>
#include <functional>

namespace KWin
{

Gesture::Gesture()
{
}

Gesture::~Gesture() = default;

SwipeGesture::SwipeGesture(uint32_t fingerCount)
    : m_fingerCount(fingerCount)
{
}

SwipeGesture::~SwipeGesture() = default;

qreal SwipeGesture::deltaToProgress(const QPointF &delta) const
{
    switch (m_direction) {
    case SwipeDirection::Up:
    case SwipeDirection::Down:
        return std::min(std::abs(delta.y()) / s_minimumDelta, 1.0);
    case SwipeDirection::Left:
    case SwipeDirection::Right:
        return std::min(std::abs(delta.x()) / s_minimumDelta, 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QPointF &delta) const
{
    return deltaToProgress(delta) >= 1.0;
}

StraightPinchGesture::StraightPinchGesture(uint32_t fingerCount)
    : m_fingerCount(fingerCount)
{
}

PinchGesture::~PinchGesture() = default;

StraightPinchGesture::~StraightPinchGesture() = default;

qreal StraightPinchGesture::scaleDeltaToProgress(const qreal &scaleDelta) const
{
    return std::abs(scaleDelta - 1) / s_minimumScaleDelta;
}

bool StraightPinchGesture::minimumScaleDeltaReached(const qreal &scaleDelta) const
{
    return scaleDeltaToProgress(scaleDelta) >= 1.0;
}

RotatePinchGesture::~RotatePinchGesture() = default;

bool RotatePinchGesture::minimumAngleDeltaReached(const qreal &angleDelta) const
{
    return angleDelta >= s_minimumAngleDelta;
}

GestureRecognizer::GestureRecognizer(QObject *parent)
    : QObject(parent)
{
}

GestureRecognizer::~GestureRecognizer() = default;

void GestureRecognizer::registerSwipeGesture(KWin::SwipeGesture *gesture)
{
    Q_ASSERT(!m_swipeGestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterSwipeGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_swipeGestures << gesture;
}

void GestureRecognizer::unregisterSwipeGesture(KWin::SwipeGesture *gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_swipeGestures.removeAll(gesture);
    if (m_activeSwipeGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
}

void GestureRecognizer::registerStraightPinchGesture(KWin::StraightPinchGesture *gesture)
{
    Q_ASSERT(!m_straightPinchGestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterStraightPinchGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_straightPinchGestures << gesture;
}

void GestureRecognizer::unregisterStraightPinchGesture(KWin::StraightPinchGesture *gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_straightPinchGestures.removeAll(gesture);
    if (m_activeStraightPinchGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
}

int GestureRecognizer::startSwipeGesture(uint fingerCount, const QPointF &startPos)
{
    m_currentFingerCount = fingerCount;
    if (!m_activeSwipeGestures.isEmpty() || !m_activeStraightPinchGestures.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (SwipeGesture *gesture : std::as_const(m_swipeGestures)) {
        if (gesture->fingerCount() != fingerCount) {
            continue;
        }

        // Only add gestures who's direction aligns with current swipe axis
        switch (gesture->direction()) {
        case SwipeDirection::Up:
        case SwipeDirection::Down:
            if (m_currentSwipeAxis == Axis::Horizontal) {
                continue;
            }
            break;
        case SwipeDirection::Left:
        case SwipeDirection::Right:
            if (m_currentSwipeAxis == Axis::Vertical) {
                continue;
            }
            break;
        case SwipeDirection::Invalid:
            Q_UNREACHABLE();
        }

        m_activeSwipeGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

void GestureRecognizer::updateSwipeGesture(const QPointF &delta)
{
    m_currentDelta += delta;

    SwipeDirection direction; // Overall direction
    Axis swipeAxis;

    // Pick an axis for gestures so horizontal ones don't change to vertical ones without lifting fingers
    if (m_currentSwipeAxis == Axis::None) {
        if (std::abs(m_currentDelta.x()) >= std::abs(m_currentDelta.y())) {
            swipeAxis = Axis::Horizontal;
            direction = m_currentDelta.x() < 0 ? SwipeDirection::Left : SwipeDirection::Right;
        } else {
            swipeAxis = Axis::Vertical;
            direction = m_currentDelta.y() < 0 ? SwipeDirection::Up : SwipeDirection::Down;
        }
        if (std::abs(m_currentDelta.x()) >= 5 || std::abs(m_currentDelta.y()) >= 5) {
            // only lock in a direction if the delta is big enough
            // to prevent accidentally choosing the wrong direction
            m_currentSwipeAxis = swipeAxis;
        }
    } else {
        swipeAxis = m_currentSwipeAxis;
    }

    // Find the current swipe direction
    switch (swipeAxis) {
    case Axis::Vertical:
        direction = m_currentDelta.y() < 0 ? SwipeDirection::Up : SwipeDirection::Down;
        break;
    case Axis::Horizontal:
        direction = m_currentDelta.x() < 0 ? SwipeDirection::Left : SwipeDirection::Right;
        break;
    default:
        Q_UNREACHABLE();
    }

    // Eliminate wrong gestures (takes two iterations)
    for (int i = 0; i < 2; i++) {

        if (m_activeSwipeGestures.isEmpty()) {
            startSwipeGesture(m_currentFingerCount);
        }

        for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();) {
            auto g = static_cast<SwipeGesture *>(*it);

            if (g->direction() != direction) {
                Q_EMIT g->cancelled();
                it = m_activeSwipeGestures.erase(it);
                continue;
            }

            it++;
        }
    }

    // Send progress update
    for (SwipeGesture *g : std::as_const(m_activeSwipeGestures)) {
        Q_EMIT g->progress(g->deltaToProgress(m_currentDelta));
        Q_EMIT g->deltaProgress(m_currentDelta);
    }
}

void GestureRecognizer::cancelActiveGestures()
{
    for (auto g : std::as_const(m_activeSwipeGestures)) {
        Q_EMIT g->cancelled();
    }
    for (auto g : std::as_const(m_activeStraightPinchGestures)) {
        Q_EMIT g->cancelled();
    }
    for (auto g : std::as_const(m_activeRotatePinchGestures)) {
        Q_EMIT g->cancelled();
    }
    m_activeSwipeGestures.clear();
    m_activeStraightPinchGestures.clear();
    m_activeRotatePinchGestures.clear();
    m_currentScale = 0;
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::cancelSwipeGesture()
{
    cancelActiveGestures();
    m_currentFingerCount = 0;
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::endSwipeGesture()
{
    const QPointF delta = m_currentDelta;
    for (auto g : std::as_const(m_activeSwipeGestures)) {
        if (static_cast<SwipeGesture *>(g)->minimumDeltaReached(delta)) {
            Q_EMIT g->triggered();
        } else {
            Q_EMIT g->cancelled();
        }
    }
    m_activeSwipeGestures.clear();
    m_currentFingerCount = 0;
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

int GestureRecognizer::startPinchGesture(uint fingerCount)
{
    m_currentFingerCount = fingerCount;
    int count = 0;
    if (!m_activeSwipeGestures.isEmpty() || !m_activeStraightPinchGestures.isEmpty() || !m_activeRotatePinchGestures.isEmpty()) {
        return 0;
    }
    for (StraightPinchGesture *gesture : std::as_const(m_straightPinchGestures)) {
        if (gesture->fingerCount() != fingerCount) {
            continue;
        }

        // direction doesn't matter yet
        m_activeStraightPinchGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    for (RotatePinchGesture *gesture : std::as_const(m_rotatePinchGestures)) {
        if (gesture->fingerCount() != fingerCount) {
            continue;
        }

        // direction doesn't matter yet
        m_activeRotatePinchGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

void GestureRecognizer::updatePinchGesture(qreal scale, qreal angleDelta, const QPointF &posDelta)
{
    m_currentScale = scale;
    m_currentAngle = angleDelta;

    if (angleDelta < PinchGesture::s_minimumRotateAngle) {
        // TO DO use total angle since start of pinch instead of delta since last event
        // Straight pinch gesture

        // Determine the direction of the swipe
        StraightPinchDirection direction;
        if (scale < 1) {
            direction = StraightPinchDirection::Contracting;
        } else {
            direction = StraightPinchDirection::Expanding;
        }

        // Eliminate wrong gestures (takes two iterations)
        for (int i = 0; i < 2; i++) {
            if (m_activeStraightPinchGestures.isEmpty()) {
                startPinchGesture(m_currentFingerCount);
            }

            for (auto it = m_activeStraightPinchGestures.begin(); it != m_activeStraightPinchGestures.end();) {
                auto g = static_cast<StraightPinchGesture *>(*it);

                if (g->direction() != direction) {
                    Q_EMIT g->cancelled();
                    it = m_activeStraightPinchGestures.erase(it);
                    continue;
                }
                it++;
            }
        }

        // TO DO eliminate rotate pinch gestures?

        for (StraightPinchGesture *g : std::as_const(m_activeStraightPinchGestures)) {
            Q_EMIT g->progress(g->scaleDeltaToProgress(scale));
        }
    } else {
        // Rotate pinch gesture

        // Determine the direction of the swipe
        RotatePinchDirection direction;
        if (angleDelta < 0) {
            direction = RotatePinchDirection::Counterclockwise;
        } else {
            direction = RotatePinchDirection::Clockwise;
        }

        // Eliminate wrong gestures (takes two iterations each)
        for (int i = 0; i < 2; i++) {
            if (m_activeRotatePinchGestures.isEmpty()) {
                startPinchGesture(m_currentFingerCount);
            }

            for (auto it = m_activeRotatePinchGestures.begin(); it != m_activeRotatePinchGestures.end();) {
                auto g = static_cast<RotatePinchGesture *>(*it);

                if (g->direction() != direction) {
                    Q_EMIT g->cancelled();
                    it = m_activeRotatePinchGestures.erase(it);
                    continue;
                }
                it++;
            }
        }
        for (int i = 0; i < 2; i++) {
            for (auto it = m_activeStraightPinchGestures.begin(); it != m_activeStraightPinchGestures.end();) {
                auto g = static_cast<StraightPinchGesture *>(*it);

                Q_EMIT g->cancelled();
                it = m_activeStraightPinchGestures.erase(it);
                continue;
            }
        }

        for (RotatePinchGesture *g : std::as_const(m_activeRotatePinchGestures)) {
            Q_EMIT g->progress(angleDelta);
        }
    }
}

void GestureRecognizer::cancelPinchGesture()
{
    cancelActiveGestures();
    m_currentScale = 1;
    m_currentAngle = 0;
    m_currentFingerCount = 0;
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::endPinchGesture() // because fingers up
{
    for (auto g : std::as_const(m_activeStraightPinchGestures)) {
        if (g->minimumScaleDeltaReached(m_currentScale)) {
            Q_EMIT g->triggered();
        } else {
            Q_EMIT g->cancelled();
        }
    }
    for (auto g : std::as_const(m_activeRotatePinchGestures)) {
        if (g->minimumAngleDeltaReached(m_currentAngle)) {
            Q_EMIT g->triggered();
        } else {
            Q_EMIT g->cancelled();
        }
    }
    m_activeSwipeGestures.clear();
    m_activeStraightPinchGestures.clear();
    m_activeRotatePinchGestures.clear();
    m_currentScale = 1;
    m_currentAngle = 0;
    m_currentFingerCount = 0;
    m_currentSwipeAxis = Axis::None;
}

uint32_t SwipeGesture::fingerCount() const
{
    return m_fingerCount;
}

SwipeDirection SwipeGesture::direction() const
{
    return m_direction;
}

void SwipeGesture::setDirection(SwipeDirection direction)
{
    m_direction = direction;
}

uint32_t StraightPinchGesture::fingerCount() const
{
    return m_fingerCount;
}

StraightPinchDirection StraightPinchGesture::direction() const
{
    return m_direction;
}

void StraightPinchGesture::setDirection(StraightPinchDirection direction)
{
    m_direction = direction;
}

uint32_t RotatePinchGesture::fingerCount() const
{
    return m_fingerCount;
}

RotatePinchDirection RotatePinchGesture::direction() const
{
    return m_direction;
}

void RotatePinchGesture::setDirection(RotatePinchDirection direction)
{
    m_direction = direction;
}

int GestureRecognizer::startSwipeGesture(uint fingerCount)
{
    return startSwipeGesture(fingerCount, QPointF());
}

}

#include "moc_gestures.cpp"
