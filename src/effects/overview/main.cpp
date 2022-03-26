/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "overvieweffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(OverviewEffect,
                              "metadata.json.stripped",
                              return OverviewEffect::supported();)

} // namespace KWin

#include "main.moc"
