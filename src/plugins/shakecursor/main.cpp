/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/shakecursor/shakecursor.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ShakeCursorEffect,
                              "metadata.json.stripped",
                              return ShakeCursorEffect::supported();)

} // namespace KWin

#include "main.moc"
