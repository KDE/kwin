/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenedgegestures.h"

#include <algorithm>
#include <ranges>

namespace KWin
{

ScreenEdgeGesture::ScreenEdgeGesture(const std::shared_ptr<ScreenEdgeGestureRecognizer> &recognizer, SwipeDirection direction, const QRectF &geometry)
    : m_recognizer(recognizer)
    , m_direction(direction)
    , m_geometry(geometry)
{
}

ScreenEdgeGesture::~ScreenEdgeGesture()
{
    if (const auto r = m_recognizer.lock()) {
        r->removeGesture(this);
    }
}

SwipeDirection ScreenEdgeGesture::direction() const
{
    return m_direction;
}

void ScreenEdgeGesture::setDirection(SwipeDirection direction)
{
    m_direction = direction;
}

QRectF ScreenEdgeGesture::geometry() const
{
    return m_geometry;
}

void ScreenEdgeGesture::setGeometry(const QRectF &geometry)
{
    m_geometry = geometry;
}

void ScreenEdgeGestureRecognizer::addGesture(ScreenEdgeGesture *gesture)
{
    m_gestures.push_back(gesture);
}

void ScreenEdgeGestureRecognizer::removeGesture(ScreenEdgeGesture *gesture)
{
    if (gesture == m_currentGesture) {
        Q_EMIT gesture->cancelled();
        m_currentGesture = nullptr;
    }
    std::erase(m_gestures, gesture);
}

bool ScreenEdgeGestureRecognizer::touchDown(uint32_t id, const QPointF &pos)
{
    m_touchPoints.push_back(id);
    if (m_touchId) {
        return true;
    }
    if (m_touchPoints.size() != 1) {
        // no multi touch support for screen edges
        return false;
    }
    const auto match = std::ranges::find_if(m_gestures, [pos](ScreenEdgeGesture *gesture) {
        return gesture->geometry().contains(pos);
    });
    if (match == m_gestures.end()) {
        return false;
    }
    m_touchId = id;
    m_currentGesture = *match;
    m_startPosition = pos;
    m_currentDistance = 0;
    Q_EMIT m_currentGesture->started();
    return true;
}

bool ScreenEdgeGestureRecognizer::touchMotion(uint32_t id, const QPointF &pos)
{
    if (!m_touchId) {
        return false;
    }
    if (m_touchId != id || !m_currentGesture) {
        return true;
    }
    const QPointF delta = pos - m_startPosition;
    switch (m_currentGesture->direction()) {
    case SwipeDirection::Left:
        m_currentDistance = -delta.x();
        break;
    case SwipeDirection::Right:
        m_currentDistance = delta.x();
        break;
    case SwipeDirection::Up:
        m_currentDistance = -delta.y();
        break;
    case SwipeDirection::Down:
        m_currentDistance = delta.y();
        break;
    case SwipeDirection::Invalid:
        break;
    }
    Q_EMIT m_currentGesture->progress(delta, std::clamp(m_currentDistance / s_activationDistance, 0.0, 1.0));
    return true;
}

bool ScreenEdgeGestureRecognizer::touchUp(uint32_t id)
{
    std::erase(m_touchPoints, id);
    if (!m_touchId) {
        return false;
    }
    if (m_touchId != id) {
        return true;
    }
    if (m_currentGesture) {
        if (m_currentDistance >= s_activationDistance) {
            Q_EMIT m_currentGesture->triggered();
        } else {
            Q_EMIT m_currentGesture->cancelled();
        }
        m_currentGesture = nullptr;
    }
    m_touchId.reset();
    return true;
}

void ScreenEdgeGestureRecognizer::touchCancel()
{
    m_touchPoints.clear();
    m_touchId.reset();
    if (m_currentGesture) {
        Q_EMIT m_currentGesture->cancelled();
        m_currentGesture = nullptr;
    }
}

}
