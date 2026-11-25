/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "input.h"
#include "notificationdrag.h"

namespace KWin
{

/**
 * Drags notifications with a two-finger horizontal touchpad scroll.
 */
class TouchpadSwipeFilter : public InputEventFilter
{
public:
    TouchpadSwipeFilter();
    ~TouchpadSwipeFilter() override;

    bool pointerAxis(PointerAxisEvent *event) override;

private:
    std::unique_ptr<NotificationDrag> m_drag;
};

} // namespace KWin
