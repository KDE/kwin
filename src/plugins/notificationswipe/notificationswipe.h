/*
    SPDX-FileCopyrightText: 2026 Artem Grinev <agrinev98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include <memory>

namespace KWin
{

class TouchpadSwipeFilter;

/**
 * Lets the user dismiss notifications by swiping them towards the screen edge they sit at.
 */
class NotificationSwipe : public Plugin
{
    Q_OBJECT

public:
    NotificationSwipe();
    ~NotificationSwipe() override;

private:
    std::unique_ptr<TouchpadSwipeFilter> m_touchpadFilter;
};

} // namespace KWin
