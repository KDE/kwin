/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "realtime.h"

#include "config-kwin.h"

#include <sched.h>

namespace KWin
{

void gainRealTime()
{
#if HAVE_SCHED_RESET_ON_FORK
    const int minPriority = sched_get_priority_min(SCHED_RR);
    sched_param sp;
    sp.sched_priority = minPriority;
    sched_setscheduler(0, SCHED_RR | SCHED_RESET_ON_FORK, &sp);
#endif
}

} // namespace KWin
