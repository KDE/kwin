/*
    SPDX-FileCopyrightText: 2022 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopgrideffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(DesktopGridEffect,
                              "metadata.json.stripped",
                              return DesktopGridEffect::supported();)

} // namespace KWin

#include "main.moc"
