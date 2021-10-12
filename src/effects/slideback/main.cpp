/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slideback.h"

namespace KWin
{

KWIN_EFFECT_FACTORY(SlideBackEffectFactory,
                    SlideBackEffect,
                    "metadata.json")

} // namespace KWin

#include "main.moc"
