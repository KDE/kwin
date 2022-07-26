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

qreal SwipeGesture::deltaToProgress(const QSizeF &delta) const
{
    if (!m_minimumDeltaRelevant || m_minimumDelta.isNull()) {
        return 1.0;
    }

    if (m_direction & (GestureDirection::Up | GestureDirection::Down)) {
        return std::min(std::abs(delta.height()) / std::abs(m_minimumDelta.height()), 1.0);
    } else if (m_direction & (GestureDirection::Left | GestureDirection::Right)) {
        return std::min(std::abs(delta.width()) / std::abs(m_minimumDelta.width()), 1.0);
    }

    return 1.0;
}

bool SwipeGesture::minimumDeltaReached(const QSizeF &delta) const
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
    for (SwipeGesture *gesture : qAsConst(m_swipeGestures)) {
        if (!gesture->isFingerCountAcceptable(fingerCount)) {
            continue;
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
        if (gesture->direction() & (GestureDirection::Up | GestureDirection::Down)) {
            if (m_currentSwipeAxis == Axis::Horizontal) {
                continue;
            }
        } else if (gesture->direction() & (GestureDirection::Left | GestureDirection::Right)) {
            if (m_currentSwipeAxis == Axis::Vertical) {
                continue;
            }
        }

        m_activeSwipeGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

void GestureRecognizer::updateSwipeGesture(const QSizeF &delta)
{
    m_currentDelta += delta;

    GestureDirection direction; // Overall direction
    Axis swipeAxis;

    // Pick an axis for gestures so horizontal ones don't change to vertical ones without lifting fingers
    if (m_currentSwipeAxis == Axis::None) {
        if (std::abs(m_currentDelta.width()) >= std::abs(m_currentDelta.height())) {
            swipeAxis = Axis::Horizontal;
            direction = m_currentDelta.width() < 0 ? GestureDirection::Left : GestureDirection::Right;
        } else {
            swipeAxis = Axis::Vertical;
            direction = m_currentDelta.height() < 0 ? GestureDirection::Up : GestureDirection::Down;
        }
        if (std::abs(m_currentDelta.width()) >= 5 || std::abs(m_currentDelta.height()) >= 5) {
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
        direction = m_currentDelta.height() < 0 ? GestureDirection::Up : GestureDirection::Down;
        break;
    case Axis::Horizontal:
        direction = m_currentDelta.width() < 0 ? GestureDirection::Left : GestureDirection::Right;
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

            if (!(g->direction() & direction)) {
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
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        Q_EMIT g->cancelled();
    }
    for (auto g : qAsConst(m_activePinchGestures)) {
        Q_EMIT g->cancelled();
    }
    m_activeSwipeGestures.clear();
    m_activePinchGestures.clear();
    m_currentScale = 0;
    m_currentDelta = QSizeF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::cancelSwipeGesture()
{
    cancelActiveGestures();
    m_currentFingerCount = 0;
    m_currentDelta = QSizeF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::endSwipeGesture()
{
    const QSizeF delta = m_currentDelta;
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        if (static_cast<SwipeGesture *>(g)->minimumDeltaReached(delta)) {
            Q_EMIT g->triggered();
        } else {
            Q_EMIT g->cancelled();
        }
    }
    m_activeSwipeGestures.clear();
    m_currentFingerCount = 0;
    m_currentDelta = QSizeF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

int GestureRecognizer::startPinchGesture(uint fingerCount)
{
    m_currentFingerCount = fingerCount;
    int count = 0;
    if (!m_activeSwipeGestures.isEmpty() || !m_activePinchGestures.isEmpty()) {
        return 0;
    }
    for (PinchGesture *gesture : qAsConst(m_pinchGestures)) {
        if (!gesture->isFingerCountAcceptable(fingerCount)) {
            continue;
        }

        // direction doesn't matter yet
        m_activePinchGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

void GestureRecognizer::updatePinchGesture(qreal scale, qreal angleDelta, const QSizeF &posDelta)
{
    Q_UNUSED(angleDelta);
    Q_UNUSED(posDelta);
    m_currentScale = scale;

    // Determine the direction of the swipe
    GestureDirection direction;
    if (scale < 1) {
        direction = GestureDirection::Contracting;
    } else {
        direction = GestureDirection::Expanding;
    }

    // Eliminate wrong gestures (takes two iterations)
    for (int i = 0; i < 2; i++) {
        if (m_activePinchGestures.isEmpty()) {
            startPinchGesture(m_currentFingerCount);
        }

        for (auto it = m_activePinchGestures.begin(); it != m_activePinchGestures.end();) {
            auto g = static_cast<PinchGesture *>(*it);

            if (!(g->direction() & direction)) {
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
    for (auto g : qAsConst(m_activePinchGestures)) {
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

QSizeF SwipeGesture::minimumDelta() const
{
    return m_minimumDelta;
}

void SwipeGesture::setMinimumDelta(const QSizeF &delta)
{
    m_minimumDelta = delta;
    m_minimumDeltaRelevant = true;
}

bool SwipeGesture::isMinimumDeltaRelevant() const
{
    return m_minimumDeltaRelevant;
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

void Gesture::addFingerCount(uint numFingers)
{
    if (m_validFingerCounts == DEFAULT_VALID_FINGER_COUNTS) {
        m_validFingerCounts.clear();
    }
    m_validFingerCounts.insert(numFingers);
}

bool Gesture::isFingerCountAcceptable(uint fingers) const
{
    return m_validFingerCounts.contains(fingers);
}

QSet<uint> Gesture::acceptableFingerCounts() const
{
    return m_validFingerCounts;
}

GestureDirections Gesture::direction() const
{
    return m_direction;
}

void Gesture::setDirection(GestureDirections direction)
{
    m_direction = direction;
}
}
