/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "touchpadswipefilter.h"

#include "input_event.h"

namespace KWin
{

TouchpadSwipeFilter::TouchpadSwipeFilter()
    : InputEventFilter(InputFilterOrder::NotificationSwipe)
{
    input()->installInputEventFilter(this);
}

TouchpadSwipeFilter::~TouchpadSwipeFilter()
{
    if (m_drag) {
        m_drag->cancel();
    }
    if (input()) {
        input()->uninstallInputEventFilter(this);
    }
}

bool TouchpadSwipeFilter::pointerAxis(PointerAxisEvent *event)
{
    if (event->source != PointerAxisSource::Finger || event->orientation != Qt::Horizontal) {
        return false;
    }

    if (event->delta == 0) {
        // Scroll stop: the fingers were lifted
        if (!m_drag) {
            return false;
        }
        m_drag->finish(event->timestamp);
        m_drag.reset();
        return true;
    }

    if (!m_drag) {
        m_drag = NotificationDrag::startAt(event->position);
        if (!m_drag) {
            return false;
        }
    }
    // Not a scroll so inversion doesn't matter
    m_drag->update(event->inverted ? -event->delta : event->delta, event->timestamp);
    return true;
}

} // namespace KWin
