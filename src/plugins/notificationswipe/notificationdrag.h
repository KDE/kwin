/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <chrono>
#include <memory>

namespace KWin
{

class Window;

/**
 * Drags a notification towards the screen edge it sits at, dismissing it once dragged far enough.
 */
class NotificationDrag : public QObject
{
    Q_OBJECT

public:
    static std::unique_ptr<NotificationDrag> startAt(const QPointF &position);
    ~NotificationDrag() override;

    void update(qreal delta, std::chrono::microseconds time);
    void finish(std::chrono::microseconds time);
    void cancel();

private:
    explicit NotificationDrag(Window *notification);

    qreal direction() const;
    bool isFlick(std::chrono::microseconds time) const;

    QPointer<Window> m_window;
    const QPointF m_initialPosition;
    Qt::Edge m_edge = Qt::RightEdge;

    // Towards m_edge, in notification widths per second
    qreal m_offset = 0.0;
    qreal m_velocity = 0.0;
    std::chrono::microseconds m_lastUpdateTime = std::chrono::microseconds::zero();
};

} // namespace KWin
