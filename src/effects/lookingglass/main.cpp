/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "lookingglass.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(LookingGlassEffect,
                              "metadata.json.stripped",
                              return LookingGlassEffect::supported();)

} // namespace KWin

#include "main.moc"
