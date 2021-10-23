/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screentransform.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ScreenTransformEffect,
                              "metadata.json.stripped",
                              return ScreenTransformEffect::supported();)

} // namespace KWin

#include "main.moc"
