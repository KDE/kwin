/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fallapart.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(FallApartEffect,
                              "metadata.json.stripped",
                              return FallApartEffect::supported();)

} // namespace KWin

#include "main.moc"
