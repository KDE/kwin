/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "magiclamp.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(MagicLampEffect,
                              "metadata.json.stripped",
                              return MagicLampEffect::supported();)

} // namespace KWin

#include "main.moc"
