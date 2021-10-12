/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "slide.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SlideEffectFactory,
                              SlideEffect,
                              "metadata.json",
                              return SlideEffect::supported();)

} // namespace KWin

#include "main.moc"
