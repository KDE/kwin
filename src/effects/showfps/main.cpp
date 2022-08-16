/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showfpseffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ShowFpsEffect,
                              "metadata.json.stripped",
                              return ShowFpsEffect::supported();)

} // namespace KWin

#include "main.moc"
