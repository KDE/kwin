/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "showopaque.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ShowOpaque,
                              "metadata.json.stripped",
                              return true;)

} // namespace KWin

#include "main.moc"
