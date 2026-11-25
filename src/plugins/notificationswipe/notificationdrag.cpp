/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "notificationdrag.h"

#include "core/output.h"
#include "input.h"
#include "pointer_input.h"
#include "window.h"
#include "workspace.h"

namespace KWin
{

// Fractions of the notification width
static constexpr qreal s_overscrollRatio = 0.05;
static constexpr qreal s_dismissRatio = 0.5;

// Notification widths per second
static constexpr qreal s_flickVelocity = 0.25;
// Movement older than this when the fingers are lifted doesn't count as a flick
static constexpr std::chrono::milliseconds s_flickTimeout(50);

std::unique_ptr<NotificationDrag> NotificationDrag::startAt(const QPointF &position)
{
    Window *notification = workspace()->findWindow([&position](const Window *window) {
        return (window->isNotification() || window->isCriticalNotification()) && window->frameGeometry().contains(position);
    });
    if (!notification) {
        return nullptr;
    }
    return std::unique_ptr<NotificationDrag>(new NotificationDrag(notification));
}

NotificationDrag::NotificationDrag(Window *notification)
    : m_window(notification)
    , m_initialPosition(notification->frameGeometry().topLeft())
{
    const LogicalOutput *output = notification->output();
    const bool leftSide = output && notification->frameGeometry().center().x() < output->geometryF().center().x();
    m_edge = leftSide ? Qt::LeftEdge : Qt::RightEdge;

    connect(notification, &Window::closed, this, [this] {
        m_window = nullptr;
    });

    // Keep the scroll events going to the notification even if it slides away from under the cursor
    input()->pointer()->lockFocus();
}

NotificationDrag::~NotificationDrag()
{
    if (input() && input()->pointer()) {
        input()->pointer()->unlockFocus();
    }
}

qreal NotificationDrag::direction() const
{
    return m_edge == Qt::LeftEdge ? -1.0 : +1.0;
}

void NotificationDrag::update(qreal delta, std::chrono::microseconds time)
{
    if (!m_window) {
        return;
    }

    const qreal width = m_window->frameGeometry().width();
    const qreal directedDelta = delta * direction();
    const qreal overscroll = width * s_overscrollRatio;
    m_offset = std::max(m_offset + directedDelta, -overscroll);
    if (m_window->isCriticalNotification()) {
        m_offset = std::min(m_offset, overscroll);
    }

    // Calculate the velocity of the drag within the last update interval.
    const auto elapsed = std::chrono::duration<qreal>(time - m_lastUpdateTime);
    if (m_lastUpdateTime.count() && elapsed.count() > 0) {
        m_velocity = directedDelta / width / elapsed.count();
    }
    m_lastUpdateTime = time;

    m_window->move(m_initialPosition + QPointF(m_offset * direction(), 0));
    m_window->setOpacity(std::clamp(1.0 - m_offset / width, 0.0, 1.0));
}

bool NotificationDrag::isFlick(std::chrono::microseconds time) const
{
    return m_velocity >= s_flickVelocity && time - m_lastUpdateTime <= s_flickTimeout;
}

void NotificationDrag::finish(std::chrono::microseconds time)
{
    if (!m_window) {
        return;
    }

    if (m_window->isCriticalNotification() || (m_offset <= m_window->frameGeometry().width() * s_dismissRatio && !isFlick(time))) {
        cancel();
        return;
    }

    Window *window = m_window;
    m_window = nullptr;
    window->closeWindow();
}

void NotificationDrag::cancel()
{
    if (!m_window) {
        return;
    }

    m_window->move(m_initialPosition);
    m_window->setOpacity(1.0);
    m_window = nullptr;
}

} // namespace KWin
