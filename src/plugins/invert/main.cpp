/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "invert.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(InvertEffect,
                              "metadata.json.stripped",
                              return InvertEffect::supported();)

} // namespace KWin

#include "main.moc"
