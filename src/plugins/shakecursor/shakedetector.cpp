/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "shakedetector.h"

#include <cmath>

ShakeDetector::ShakeDetector()
{
}

quint64 ShakeDetector::interval() const
{
    return m_interval;
}

void ShakeDetector::setInterval(quint64 interval)
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

std::optional<qreal> ShakeDetector::update(QMouseEvent *event)
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

    m_history.append(HistoryItem{
        .position = event->localPos(),
        .timestamp = event->timestamp(),
    });

    qreal left = m_history[0].position.x();
    qreal top = m_history[0].position.y();
    qreal right = m_history[0].position.x();
    qreal bottom = m_history[0].position.y();
    qreal distance = 0;

    for (int i = 1; i < m_history.size(); ++i) {
        // Compute the length of the mouse path.
        const qreal deltaX = m_history.at(i).position.x() - m_history.at(i - 1).position.x();
        const qreal deltaY = m_history.at(i).position.y() - m_history.at(i - 1).position.y();
        distance += std::sqrt(deltaX * deltaX + deltaY * deltaY);

        // Compute the bounds of the mouse path.
        left = std::min(left, m_history.at(i).position.x());
        top = std::min(top, m_history.at(i).position.y());
        right = std::max(right, m_history.at(i).position.x());
        bottom = std::max(bottom, m_history.at(i).position.y());
    }

    const qreal boundsWidth = right - left;
    const qreal boundsHeight = bottom - top;
    const qreal diagonal = std::sqrt(boundsWidth * boundsWidth + boundsHeight * boundsHeight);
    if (diagonal == 0) {
        return std::nullopt;
    }

    const qreal shakeFactor = distance / diagonal;
    if (shakeFactor > m_sensitivity) {
        return shakeFactor - m_sensitivity;
    }

    return std::nullopt;
}
