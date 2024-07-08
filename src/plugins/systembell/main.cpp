/*
    SPDX-FileCopyrightText: 2024 Nicolas Fella <nicolas.fella@kdab.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systembell.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SystemBellEffect,
                              "metadata.json.stripped",
                              return SystemBellEffect::supported();)

} // namespace KWin

#include "main.moc"
