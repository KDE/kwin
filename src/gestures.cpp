/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gestures.h"

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

    switch (m_direction) {
    case GestureDirection::Up:
    case GestureDirection::Down:
    case GestureDirection::VerticalAxis:
        return std::min(std::abs(delta.height()) / std::abs(m_minimumDelta.height()), 1.0);
    case GestureDirection::Left:
    case GestureDirection::Right:
    case GestureDirection::HorizontalAxis:
        return std::min(std::abs(delta.width()) / std::abs(m_minimumDelta.width()), 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QSizeF &delta) const
{
    return getSemanticProgress(delta) >= 0.5;
}

void PinchGesture::setMinimumScaleDelta(const qreal &scaleDelta)
{
    m_minimumScaleDeltaRelevant = true;
    m_minimumScaleDelta = scaleDelta;
}

qreal PinchGesture::minimumScaleDelta() const
{
    return m_minimumScaleDelta;
}

PinchGesture::PinchGesture(QObject *parent)
    : Gesture(parent)
{
}

PinchGesture::~PinchGesture() = default;

bool PinchGesture::minimumScaleDeltaReached(const qreal &scaleDelta) const
{
    return getSemanticProgress(scaleDelta) >= 0.5;
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
        switch (gesture->direction()) {
        case GestureDirection::Up:
        case GestureDirection::Down:
        case GestureDirection::VerticalAxis:
            if (m_currentSwipeAxis == Axis::Horizontal) {
                continue;
            }
            break;
        case GestureDirection::Left:
        case GestureDirection::Right:
        case GestureDirection::HorizontalAxis:
            if (m_currentSwipeAxis == Axis::Vertical) {
                continue;
            }
            break;
        case GestureDirection::DirectionlessSwipe:
            break;
        default:
            continue;
        }

        m_activeSwipeGestures << gesture;
        count++;
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
        startSwipeGesture(m_currentFingerCount);

        for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();) {
            auto g = static_cast<SwipeGesture *>(*it);

            if (mutuallyExclusive(direction, g->direction())) {
                // If a gesture was started from a touchscreen border never cancel it
                if (!g->minimumXIsRelevant() || !g->maximumXIsRelevant() || !g->minimumYIsRelevant() || !g->maximumYIsRelevant()) {
                    if (g->isStarted) {
                        g->isStarted = false;
                        Q_EMIT g->cancelled();
                    }
                    it = m_activeSwipeGestures.erase(it);
                    continue;
                }
            }

            it++;
        }
    }

    // Send progress update
    for (SwipeGesture *g : std::as_const(m_activeSwipeGestures)) {
        if (!g->isStarted) {
            g->isStarted = true;
            Q_EMIT g->started();
        }
        Q_EMIT g->progress(g->deltaToProgress(m_currentDelta));
        Q_EMIT g->semanticProgress(g->getSemanticProgress(m_currentDelta), g->direction());
        Q_EMIT g->pixelDelta(m_currentDelta, g->direction());
        Q_EMIT g->deltaProgress(m_currentDelta);
        Q_EMIT g->semanticDelta(g->getSemanticDelta(m_currentDelta), g->direction());
        if (g->direction() != GestureDirection::DirectionlessSwipe) {
            Q_EMIT g->semanticProgressAxis(g->getSemanticAxisProgress(m_currentDelta), g->direction());
        }
    }
}

bool GestureRecognizer::mutuallyExclusive(GestureDirection currentDir, GestureDirection gestureDir)
{
    if (currentDir == gestureDir) {
        return false;
    }
    if (gestureDir == GestureDirection::DirectionlessSwipe) {
        return false;
    }

    switch (currentDir) {
    case GestureDirection::Up:
    case GestureDirection::Down:
        if (gestureDir == GestureDirection::VerticalAxis) {
            return false;
        }
        break;
    case GestureDirection::Left:
    case GestureDirection::Right:
        if (gestureDir == GestureDirection::HorizontalAxis) {
            return false;
        }
        break;
    default:
        return true;
    }

    return true;
}

void GestureRecognizer::cancelActiveGestures()
{
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        Q_EMIT g->cancelled();
        g->isStarted = false;
    }
    for (auto g : qAsConst(m_activePinchGestures)) {
        Q_EMIT g->cancelled();
        g->isStarted = false;
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
}

void GestureRecognizer::endSwipeGesture()
{
    const QSizeF delta = m_currentDelta;
    for (SwipeGesture *g : qAsConst(m_activeSwipeGestures)) {
        if (g->minimumDeltaReached(delta)) {
            Q_EMIT g->triggered();
            g->isStarted = false;
        } else {
            Q_EMIT g->cancelled();
            g->isStarted = false;
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
        startPinchGesture(m_currentFingerCount);

        for (auto it = m_activePinchGestures.begin(); it != m_activePinchGestures.end();) {
            auto g = static_cast<PinchGesture *>(*it);

            if (g->direction() != direction && g->direction() != GestureDirection::BiDirectionalPinch) {
                if (g->isStarted) {
                    g->isStarted = false;
                    Q_EMIT g->cancelled();
                }
                it = m_activePinchGestures.erase(it);
                continue;
            }
            it++;
        }
    }

    for (PinchGesture *g : std::as_const(m_activePinchGestures)) {
        if (!g->isStarted) {
            g->isStarted = true;
            Q_EMIT g->started();
        }
        Q_EMIT g->progress(g->deltaToProgress(scale));
        Q_EMIT g->semanticProgress(g->getSemanticProgress(scale), g->direction());
        Q_EMIT g->semanticProgressAxis(g->getSemanticAxisProgress(scale), g->direction());
    }
}

void GestureRecognizer::cancelPinchGesture()
{
    cancelActiveGestures();
}

void GestureRecognizer::endPinchGesture() // because fingers up
{
    for (auto g : qAsConst(m_activePinchGestures)) {
        if (g->minimumScaleDeltaReached(m_currentScale)) {
            Q_EMIT g->triggered();
            g->isStarted = false;
        } else {
            Q_EMIT g->cancelled();
            g->isStarted = false;
        }
    }
    m_activeSwipeGestures.clear();
    m_activePinchGestures.clear();
    m_currentScale = 1;
    m_currentFingerCount = 0;
    m_currentSwipeAxis = Axis::None;
}

GestureDirection SwipeGesture::direction() const
{
    return m_direction;
}

void SwipeGesture::setDirection(GestureDirection direction)
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

GestureDirection PinchGesture::direction() const
{
    return m_direction;
}

void PinchGesture::setDirection(GestureDirection direction)
{
    m_direction = direction;
}

qreal PinchGesture::deltaToProgress(const qreal &scale) const
{
    return std::clamp(std::abs(1 - scale) / m_minimumScaleDelta, 0.0, 1.0);
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

QSizeF SwipeGesture::getSemanticDelta(const QSizeF &delta) const
{
    QSizeF d = QSizeF();
    d.setWidth(delta.width() / m_unitDelta);
    d.setHeight(delta.height() / m_unitDelta);
    return d;
}

qreal SwipeGesture::getSemanticProgress(const QSizeF &delta) const
{
    switch (m_direction) {
    case GestureDirection::Up:
    case GestureDirection::Down:
        return std::abs(delta.height()) / m_unitDelta;
    case GestureDirection::Left:
    case GestureDirection::Right:
        return std::abs(delta.width()) / m_unitDelta;
    case GestureDirection::VerticalAxis:
        return std::abs(delta.height()) / m_unitDelta;
    case GestureDirection::HorizontalAxis:
        return std::abs(delta.width()) / m_unitDelta;
    case GestureDirection::DirectionlessSwipe:
        return std::hypot(delta.width(), delta.height()) / m_unitDelta;

    default:
        Q_UNREACHABLE();
    }
}

qreal PinchGesture::getSemanticProgress(const qreal scale) const
{
    return std::max(std::abs(1 - scale) / m_unitDelta, 0.0);
}

qreal SwipeGesture::getSemanticAxisProgress(const QSizeF &delta)
{
    switch (m_direction) {
    case GestureDirection::Up:
    case GestureDirection::Down:
    case GestureDirection::VerticalAxis:
        return delta.height() / m_unitDelta;
    case GestureDirection::Left:
    case GestureDirection::Right:
    case GestureDirection::HorizontalAxis:
        return delta.width() / m_unitDelta;
    default:
        Q_UNREACHABLE();
    }
}

qreal PinchGesture::getSemanticAxisProgress(const qreal scale)
{
    return (scale - 1) / m_unitDelta;
}
}
