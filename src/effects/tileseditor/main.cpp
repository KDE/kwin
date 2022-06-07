/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tileseditoreffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(TilesEditorEffect,
                              "metadata.json.stripped",
                              return TilesEditorEffect::supported();)

} // namespace KWin

#include "main.moc"
