/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shakedetector.h"
#include "input_event.h"

#include <cmath>

namespace KWin
{

ShakeDetector::ShakeDetector()
{
}

std::chrono::microseconds ShakeDetector::interval() const
{
    return m_interval;
}

void ShakeDetector::setInterval(std::chrono::microseconds interval)
{
    m_interval = interval;
}

qreal ShakeDetector::sensitivity() const
{
    return m_sensitivity;
}

void ShakeDetector::setSensitivity(qreal sensitivity)
{
    m_sensitivity = sensitivity;
}

static inline int sign(qreal value)
{
    if (value < 0) {
        return -1;
    } else if (value > 0) {
        return 1;
    } else {
        return 0;
    }
}

bool ShakeDetector::update(MouseEvent *event)
{
    // Prune the old entries in the history.
    auto it = m_history.begin();
    for (; it != m_history.end(); ++it) {
        if (event->timestamp() - it->timestamp < m_interval) {
            break;
        }
    }
    if (it != m_history.begin()) {
        m_history.erase(m_history.begin(), it);
    }

    if (!m_history.empty()) {
        HistoryItem &last = m_history.back();

        if (sign(last.delta.x()) == sign(event->delta().x()) && sign(last.delta.y()) == sign(event->delta().x())) {
            last = HistoryItem{
                .position = event->localPos(),
                .delta = event->delta(),
                .timestamp = event->timestamp(),
            };
            return false;
        }
    }

    m_history.emplace_back(HistoryItem{
        .position = event->localPos(),
        .delta = event->delta(),
        .timestamp = event->timestamp(),
    });

    qreal left = m_history[0].position.x();
    qreal top = m_history[0].position.y();
    qreal right = m_history[0].position.x();
    qreal bottom = m_history[0].position.y();
    qreal distance = 0;

    for (const HistoryItem &item : m_history) {
        // Compute the length of the mouse path.
        const qreal deltaX = item.delta.x();
        const qreal deltaY = item.delta.y();
        distance += std::sqrt(deltaX * deltaX + deltaY * deltaY);

        // Compute the bounds of the mouse path.
        left = std::min(left, item.position.x());
        top = std::min(top, item.position.y());
        right = std::max(right, item.position.x());
        bottom = std::max(bottom, item.position.y());
    }

    const qreal boundsWidth = right - left;
    const qreal boundsHeight = bottom - top;
    const qreal diagonal = std::sqrt(boundsWidth * boundsWidth + boundsHeight * boundsHeight);
    if (diagonal < 100) {
        return false;
    }

    const qreal shakeFactor = distance / diagonal;
    if (shakeFactor > m_sensitivity) {
        m_history.clear();
        return true;
    }

    return false;
}

} // namespace KWin
