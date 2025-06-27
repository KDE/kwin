/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "effect/quickeffect.h"

#include "windowpicker.h"

namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED(WindowPicker,
                              "metadata.json.stripped",
                              return WindowPicker::supported();)

} // namespace KWin

#include "main.moc"
