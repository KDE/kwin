/*
    SPDX-FileCopyrightText: 2024 Jin Liu <m.liu.jin@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hidecursor.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(HideCursorEffect,
                              "metadata.json.stripped",
                              return HideCursorEffect::supported();)

} // namespace KWin

#include "main.moc"
