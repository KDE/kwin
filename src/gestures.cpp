/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gestures.h"

#include <QDebug>
#include <QRect>
#include <cmath>
#include <functional>

namespace KWin
{

Gesture::Gesture(QObject *parent)
    : QObject(parent)
{
}

Gesture::~Gesture() = default;

SwipeGesture::SwipeGesture(QObject *parent)
    : Gesture(parent)
{
}

SwipeGesture::~SwipeGesture() = default;

void SwipeGesture::setStartGeometry(const QRect &geometry)
{
    setMinimumX(geometry.x());
    setMinimumY(geometry.y());
    setMaximumX(geometry.x() + geometry.width());
    setMaximumY(geometry.y() + geometry.height());

    Q_ASSERT(m_maximumX >= m_minimumX);
    Q_ASSERT(m_maximumY >= m_minimumY);
}

qreal SwipeGesture::deltaToProgress(const QPointF &delta) const
{
    if (!m_minimumDeltaRelevant || m_minimumDelta.isNull()) {
        return 1.0;
    }

    switch (m_direction) {
    case Direction::Up:
    case Direction::Down:
        return std::min(std::abs(delta.y()) / std::abs(m_minimumDelta.y()), 1.0);
    case Direction::Left:
    case Direction::Right:
        return std::min(std::abs(delta.x()) / std::abs(m_minimumDelta.x()), 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QPointF &delta) const
{
    return deltaToProgress(delta) >= 1.0;
}

PinchGesture::PinchGesture(QObject *parent)
    : Gesture(parent)
{
}

PinchGesture::~PinchGesture() = default;

qreal PinchGesture::scaleDeltaToProgress(const qreal &scaleDelta) const
{
    return std::clamp(std::abs(scaleDelta - 1) / minimumScaleDelta(), 0.0, 1.0);
}

bool PinchGesture::minimumScaleDeltaReached(const qreal &scaleDelta) const
{
    return scaleDeltaToProgress(scaleDelta) >= 1.0;
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

void GestureRecognizer::registerPinchGesture(KWin::PinchGesture *gesture)
{
    Q_ASSERT(!m_pinchGestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterPinchGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_pinchGestures << gesture;
}

void GestureRecognizer::unregisterPinchGesture(KWin::PinchGesture *gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_pinchGestures.removeAll(gesture);
    if (m_activePinchGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
}

int GestureRecognizer::startSwipeGesture(uint fingerCount, const QPointF &startPos, StartPositionBehavior startPosBehavior)
{
    m_currentFingerCount = fingerCount;
    if (!m_activeSwipeGestures.isEmpty() || !m_activePinchGestures.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (SwipeGesture *gesture : std::as_const(m_swipeGestures)) {
        if (gesture->minimumFingerCountIsRelevant()) {
            if (gesture->minimumFingerCount() > fingerCount) {
                continue;
            }
        }
        if (gesture->maximumFingerCountIsRelevant()) {
            if (gesture->maximumFingerCount() < fingerCount) {
                continue;
            }
        }
        if (startPosBehavior == StartPositionBehavior::Relevant) {
            if (gesture->minimumXIsRelevant()) {
                if (gesture->minimumX() > startPos.x()) {
                    continue;
                }
            }
            if (gesture->maximumXIsRelevant()) {
                if (gesture->maximumX() < startPos.x()) {
                    continue;
                }
            }
            if (gesture->minimumYIsRelevant()) {
                if (gesture->minimumY() > startPos.y()) {
                    continue;
                }
            }
            if (gesture->maximumYIsRelevant()) {
                if (gesture->maximumY() < startPos.y()) {
                    continue;
                }
            }
        }

        // Only add gestures who's direction aligns with current swipe axis
        switch (gesture->direction()) {
        case SwipeGesture::Direction::Up:
        case SwipeGesture::Direction::Down:
            if (m_currentSwipeAxis == Axis::Horizontal) {
                continue;
            }
            break;
        case SwipeGesture::Direction::Left:
        case SwipeGesture::Direction::Right:
            if (m_currentSwipeAxis == Axis::Vertical) {
                continue;
            }
            break;
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

    SwipeGesture::Direction direction; // Overall direction
    Axis swipeAxis;

    // Pick an axis for gestures so horizontal ones don't change to vertical ones without lifting fingers
    if (m_currentSwipeAxis == Axis::None) {
        if (std::abs(m_currentDelta.x()) >= std::abs(m_currentDelta.y())) {
            swipeAxis = Axis::Horizontal;
            direction = m_currentDelta.x() < 0 ? SwipeGesture::Direction::Left : SwipeGesture::Direction::Right;
        } else {
            swipeAxis = Axis::Vertical;
            direction = m_currentDelta.y() < 0 ? SwipeGesture::Direction::Up : SwipeGesture::Direction::Down;
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
        direction = m_currentDelta.y() < 0 ? SwipeGesture::Direction::Up : SwipeGesture::Direction::Down;
        break;
    case Axis::Horizontal:
        direction = m_currentDelta.x() < 0 ? SwipeGesture::Direction::Left : SwipeGesture::Direction::Right;
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
                // If a gesture was started from a touchscreen border never cancel it
                if (!g->minimumXIsRelevant() || !g->maximumXIsRelevant() || !g->minimumYIsRelevant() || !g->maximumYIsRelevant()) {
                    Q_EMIT g->cancelled();
                    it = m_activeSwipeGestures.erase(it);
                    continue;
                }
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
    for (auto g : std::as_const(m_activePinchGestures)) {
        Q_EMIT g->cancelled();
    }
    m_activeSwipeGestures.clear();
    m_activePinchGestures.clear();
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
    if (!m_activeSwipeGestures.isEmpty() || !m_activePinchGestures.isEmpty()) {
        return 0;
    }
    for (PinchGesture *gesture : std::as_const(m_pinchGestures)) {
        if (gesture->minimumFingerCountIsRelevant()) {
            if (gesture->minimumFingerCount() > fingerCount) {
                continue;
            }
        }
        if (gesture->maximumFingerCountIsRelevant()) {
            if (gesture->maximumFingerCount() < fingerCount) {
                continue;
            }
        }

        // direction doesn't matter yet
        m_activePinchGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

void GestureRecognizer::updatePinchGesture(qreal scale, qreal angleDelta, const QPointF &posDelta)
{
    m_currentScale = scale;

    // Determine the direction of the swipe
    PinchGesture::Direction direction;
    if (scale < 1) {
        direction = PinchGesture::Direction::Contracting;
    } else {
        direction = PinchGesture::Direction::Expanding;
    }

    // Eliminate wrong gestures (takes two iterations)
    for (int i = 0; i < 2; i++) {
        if (m_activePinchGestures.isEmpty()) {
            startPinchGesture(m_currentFingerCount);
        }

        for (auto it = m_activePinchGestures.begin(); it != m_activePinchGestures.end();) {
            auto g = static_cast<PinchGesture *>(*it);

            if (g->direction() != direction) {
                Q_EMIT g->cancelled();
                it = m_activePinchGestures.erase(it);
                continue;
            }
            it++;
        }
    }

    for (PinchGesture *g : std::as_const(m_activePinchGestures)) {
        Q_EMIT g->progress(g->scaleDeltaToProgress(scale));
    }
}

void GestureRecognizer::cancelPinchGesture()
{
    cancelActiveGestures();
    m_currentScale = 1;
    m_currentFingerCount = 0;
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::endPinchGesture() // because fingers up
{
    for (auto g : std::as_const(m_activePinchGestures)) {
        if (g->minimumScaleDeltaReached(m_currentScale)) {
            Q_EMIT g->triggered();
        } else {
            Q_EMIT g->cancelled();
        }
    }
    m_activeSwipeGestures.clear();
    m_activePinchGestures.clear();
    m_currentScale = 1;
    m_currentFingerCount = 0;
    m_currentSwipeAxis = Axis::None;
}

bool SwipeGesture::maximumFingerCountIsRelevant() const
{
    return m_maximumFingerCountRelevant;
}

uint SwipeGesture::minimumFingerCount() const
{
    return m_minimumFingerCount;
}

void SwipeGesture::setMinimumFingerCount(uint count)
{
    m_minimumFingerCount = count;
    m_minimumFingerCountRelevant = true;
}

bool SwipeGesture::minimumFingerCountIsRelevant() const
{
    return m_minimumFingerCountRelevant;
}

void SwipeGesture::setMaximumFingerCount(uint count)
{
    m_maximumFingerCount = count;
    m_maximumFingerCountRelevant = true;
}

uint SwipeGesture::maximumFingerCount() const
{
    return m_maximumFingerCount;
}

SwipeGesture::Direction SwipeGesture::direction() const
{
    return m_direction;
}

void SwipeGesture::setDirection(Direction direction)
{
    m_direction = direction;
}

void SwipeGesture::setMinimumX(int x)
{
    m_minimumX = x;
    m_minimumXRelevant = true;
}

int SwipeGesture::minimumX() const
{
    return m_minimumX;
}

bool SwipeGesture::minimumXIsRelevant() const
{
    return m_minimumXRelevant;
}

void SwipeGesture::setMinimumY(int y)
{
    m_minimumY = y;
    m_minimumYRelevant = true;
}

int SwipeGesture::minimumY() const
{
    return m_minimumY;
}

bool SwipeGesture::minimumYIsRelevant() const
{
    return m_minimumYRelevant;
}

void SwipeGesture::setMaximumX(int x)
{
    m_maximumX = x;
    m_maximumXRelevant = true;
}

int SwipeGesture::maximumX() const
{
    return m_maximumX;
}

bool SwipeGesture::maximumXIsRelevant() const
{
    return m_maximumXRelevant;
}

void SwipeGesture::setMaximumY(int y)
{
    m_maximumY = y;
    m_maximumYRelevant = true;
}

int SwipeGesture::maximumY() const
{
    return m_maximumY;
}

bool SwipeGesture::maximumYIsRelevant() const
{
    return m_maximumYRelevant;
}

QPointF SwipeGesture::minimumDelta() const
{
    return m_minimumDelta;
}

void SwipeGesture::setMinimumDelta(const QPointF &delta)
{
    m_minimumDelta = delta;
    m_minimumDeltaRelevant = true;
}

bool SwipeGesture::isMinimumDeltaRelevant() const
{
    return m_minimumDeltaRelevant;
}

bool PinchGesture::minimumFingerCountIsRelevant() const
{
    return m_minimumFingerCountRelevant;
}

void PinchGesture::setMinimumFingerCount(uint count)
{
    m_minimumFingerCount = count;
    m_minimumFingerCountRelevant = true;
}

uint PinchGesture::minimumFingerCount() const
{
    return m_minimumFingerCount;
}

bool PinchGesture::maximumFingerCountIsRelevant() const
{
    return m_maximumFingerCountRelevant;
}

void PinchGesture::setMaximumFingerCount(uint count)
{
    m_maximumFingerCount = count;
    m_maximumFingerCountRelevant = true;
}

uint PinchGesture::maximumFingerCount() const
{
    return m_maximumFingerCount;
}

PinchGesture::Direction PinchGesture::direction() const
{
    return m_direction;
}

void PinchGesture::setDirection(Direction direction)
{
    m_direction = direction;
}

qreal PinchGesture::minimumScaleDelta() const
{
    return m_minimumScaleDelta;
}

void PinchGesture::setMinimumScaleDelta(const qreal &scaleDelta)
{
    m_minimumScaleDelta = scaleDelta;
    m_minimumScaleDeltaRelevant = true;
}

bool PinchGesture::isMinimumScaleDeltaRelevant() const
{
    return m_minimumScaleDeltaRelevant;
}

int GestureRecognizer::startSwipeGesture(uint fingerCount)
{
    return startSwipeGesture(fingerCount, QPointF(), StartPositionBehavior::Irrelevant);
}

int GestureRecognizer::startSwipeGesture(const QPointF &startPos)
{
    return startSwipeGesture(1, startPos, StartPositionBehavior::Relevant);
}

}
