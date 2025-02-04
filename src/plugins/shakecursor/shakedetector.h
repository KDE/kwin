/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>

#include <chrono>
#include <deque>

namespace KWin
{

struct PointerMotionEvent;

/**
 * The ShakeDetector type provides a way to detect pointer shake gestures.
 *
 * Shake gestures are detected by comparing the length of the trail of the cursor within past N milliseconds
 * with the length of the diagonal of the bounding rectangle of the trail. If the trail is longer
 * than the diagonal by certain preconfigured factor, it's assumed that the user shook the pointer.
 */
class ShakeDetector
{
public:
    ShakeDetector();

    void reset();
    bool update(PointerMotionEvent *event);

    quint64 interval() const;
    void setInterval(quint64 interval);

    qreal sensitivity() const;
    void setSensitivity(qreal sensitivity);

private:
    struct HistoryItem
    {
        QPointF position;
        std::chrono::microseconds timestamp;
    };

    std::deque<HistoryItem> m_history;
    std::chrono::milliseconds m_interval = std::chrono::milliseconds(1000);
    qreal m_sensitivity = 4;
};

} // namespace KWin
