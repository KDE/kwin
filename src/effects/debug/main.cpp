/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "debugeffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(DebugEffect,
                              "metadata.json.stripped",
                              return DebugEffect::supported();)

} // namespace KWin

#include "main.moc"
