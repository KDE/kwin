/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slidingpopups.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SlidingPopupsEffect,
                              "metadata.json.stripped",
                              return SlidingPopupsEffect::supported();)

} // namespace KWin

#include "main.moc"
