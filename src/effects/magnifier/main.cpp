/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "magnifier.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(MagnifierEffect,
                              "metadata.json.stripped",
                              return MagnifierEffect::supported();)

} // namespace KWin

#include "main.moc"
