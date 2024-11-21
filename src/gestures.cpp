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
    if (!m_minimumDeltaRelevant || m_minimumDelta.isNull()) {
        return 1.0;
    }

    switch (m_direction) {
    case SwipeDirection::Up:
    case SwipeDirection::Down:
        return std::min(std::abs(delta.y()) / std::abs(m_minimumDelta.y()), 1.0);
    case SwipeDirection::Left:
    case SwipeDirection::Right:
        return std::min(std::abs(delta.x()) / std::abs(m_minimumDelta.x()), 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QPointF &delta) const
{
    return deltaToProgress(delta) >= 1.0;
}

PinchGesture::PinchGesture(uint32_t fingerCount)
    : m_fingerCount(fingerCount)
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

void GestureRecognizer::registerSwipeGesture(KWin::SwipeGesture *gesture)
{
    Q_ASSERT(!m_swipeGestures.contains(gesture));
    m_swipeGestures << gesture;
}

void GestureRecognizer::unregisterSwipeGesture(KWin::SwipeGesture *gesture)
{
    m_swipeGestures.removeAll(gesture);
    if (m_activeSwipeGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
}

void GestureRecognizer::registerPinchGesture(KWin::PinchGesture *gesture)
{
    Q_ASSERT(!m_pinchGestures.contains(gesture));
    m_pinchGestures << gesture;
}

void GestureRecognizer::unregisterPinchGesture(KWin::PinchGesture *gesture)
{
    m_pinchGestures.removeAll(gesture);
    if (m_activePinchGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
}

int GestureRecognizer::startSwipeGesture(uint fingerCount, const QPointF &startPos)
{
    m_currentFingerCount = fingerCount;
    if (!m_activeSwipeGestures.isEmpty() || !m_activePinchGestures.isEmpty()) {
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
        if (gesture->fingerCount() != fingerCount) {
            continue;
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
    PinchDirection direction;
    if (scale < 1) {
        direction = PinchDirection::Contracting;
    } else {
        direction = PinchDirection::Expanding;
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

uint32_t PinchGesture::fingerCount() const
{
    return m_fingerCount;
}

PinchDirection PinchGesture::direction() const
{
    return m_direction;
}

void PinchGesture::setDirection(PinchDirection direction)
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
    return startSwipeGesture(fingerCount, QPointF());
}

}

#include "moc_gestures.cpp"
