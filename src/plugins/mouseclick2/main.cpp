/*
    SPDX-FileCopyrightText: 2026 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mouseclickeffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(MouseClickEffect2,
                              "metadata.json.stripped",
                              return MouseClickEffect2::supported();)

} // namespace KWin

#include "main.moc"
