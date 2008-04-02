/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2008 Louai Al-Khanji <louai.khanji@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// This contains code shared by the shadow effect and shadow effect configurator
// Mark all functions static so the symbols are not exported!

#ifndef KWIN_SHADOW_HELPER_H
#define KWIN_SHADOW_HELPER_H

#include <KColorScheme>
#include <KColorUtils>

static const int MAX_ITERS = 10; // should be enough
static const qreal LUMA_THRESHOLD = 0.05;
static const qreal MINIMUM_CONTRAST = 3.0;

static bool contrastTooLow(const QColor& one, const QColor& two)
    {
    return KColorUtils::contrastRatio(one, two) < MINIMUM_CONTRAST;
    }

static QColor schemeShadowColor()
    {
    QPalette palette;
    QColor shadowColor;
    QPalette::ColorRole shadowRole;
    QColor windowColor;

    windowColor = palette.color(QPalette::Window);

    if (KColorUtils::luma(windowColor) >= LUMA_THRESHOLD)
        shadowRole = QPalette::Shadow;
    else
        shadowRole = QPalette::Light;

    shadowColor = palette.color(shadowRole);

    // Some styles might set a weird shadow or light color. Make sure we
    // dont't end up looping forever or we might lock up the desktop!!
    int iters = 0;
    while (contrastTooLow(shadowColor, windowColor) && iters < MAX_ITERS)
        {
        iters++;
        if (shadowRole == QPalette::Shadow)
            shadowColor = KColorUtils::darken(shadowColor);
        else
            shadowColor = KColorUtils::lighten(shadowColor);
        }

    return shadowColor;
    }

#endif // KWIN_SHADOW_HELPER_H
