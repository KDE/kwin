/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowvieweffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SidePeekEffect,
                              "metadata.json.stripped",
                              return SidePeekEffect::supported();)

} // namespace KWin

#include "main.moc"
