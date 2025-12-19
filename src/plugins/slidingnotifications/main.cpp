/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slidingnotifications.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SlidingNotificationsEffect,
                              "metadata.json.stripped",
                              return SlidingNotificationsEffect::supported();)

}

#include "main.moc"
