/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

namespace KWin
{

static const int MSC_DAY = 86400000;
static const int MIN_TEMPERATURE = 1000;
static const int DEFAULT_DAY_TEMPERATURE = 6500;
static const int DEFAULT_NIGHT_TEMPERATURE = 4500;
static const int DEFAULT_TRANSITION_DURATION = 1800000; /* 30 minutes */
static const int MIN_TRANSITION_DURATION = 60000;

}
