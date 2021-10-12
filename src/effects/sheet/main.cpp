/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sheet.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SheetEffectFactory,
                              SheetEffect,
                              "metadata.json",
                              return SheetEffect::supported();)

} // namespace KWin

#include "main.moc"
