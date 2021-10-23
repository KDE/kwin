/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshot.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ScreenShotEffect,
                              "metadata.json.stripped",
                              return ScreenShotEffect::supported();)

} // namespace KWin

#include "main.moc"
