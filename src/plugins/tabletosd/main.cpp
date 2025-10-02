/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabletosdeffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(TabletOsdEffect,
                              "metadata.json.stripped",
                              return TabletOsdEffect::supported();)

} // namespace KWin

#include "main.moc"
