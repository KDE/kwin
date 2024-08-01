/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showcompositing.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ShowCompositingEffect,
                              "metadata.json.stripped",
                              return ShowCompositingEffect::supported();)

} // namespace KWin

#include "main.moc"
