/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sessionfade.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(SessionFadeEffect,
                              "metadata.json.stripped",
                              return SessionFadeEffect::supported();)

} // namespace KWin

#include "main.moc"
