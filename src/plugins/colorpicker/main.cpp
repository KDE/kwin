/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorpicker.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ColorPickerEffect,
                              "metadata.json.stripped",
                              return ColorPickerEffect::supported();)

} // namespace KWin

#include "main.moc"
