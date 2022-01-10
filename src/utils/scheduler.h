/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwin_export.h>

namespace KWin
{

class KWIN_EXPORT Scheduler
{
public:
    static void gainRealTime();
};

} // namespace KWin
