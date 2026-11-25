/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "notificationswipe.h"
#include "touchpadswipefilter.h"

namespace KWin
{

NotificationSwipe::NotificationSwipe()
    : m_touchpadFilter(std::make_unique<TouchpadSwipeFilter>())
{
}

NotificationSwipe::~NotificationSwipe() = default;

} // namespace KWin
