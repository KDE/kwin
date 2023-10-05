/*
    SPDX-FileCopyrightText: 2023 Fushan Wen <qydwhotmail@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colorblindnesscorrection.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ColorBlindnessCorrectionEffect, "metadata.json.stripped", return ColorBlindnessCorrectionEffect::supported();)

} // namespace KWin

#include "main.moc"
