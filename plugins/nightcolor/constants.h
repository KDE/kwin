/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_NIGHTCOLOR_CONSTANTS_H
#define KWIN_NIGHTCOLOR_CONSTANTS_H

namespace KWin
{

static const int MSC_DAY = 86400000;
static const int MIN_TEMPERATURE = 1000;
static const int NEUTRAL_TEMPERATURE = 6500;
static const int DEFAULT_NIGHT_TEMPERATURE = 4500;
static const int FALLBACK_SLOW_UPDATE_TIME = 1800000;   /* 30 minutes */

}

#endif // KWIN_NIGHTCOLOR_CONSTANTS_H
