/*
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshothelper.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ScreenshotHelper,
                              "metadata.json.stripped",
                              return ScreenshotHelper::supported();)

} // namespace KWin

#include "main.moc"
