/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "realtime.h"

#include "config-kwin.h"

#include <pthread.h>
#include <sched.h>

namespace KWin
{

void gainRealTime()
{
#if HAVE_SCHED_RESET_ON_FORK
    const int minPriority = sched_get_priority_min(SCHED_RR);
    sched_param sp;
    sp.sched_priority = minPriority;
    if (pthread_setschedparam(pthread_self(), SCHED_RR | SCHED_RESET_ON_FORK, &sp) != 0) {
        qWarning("Failed to gain real time thread priority (See CAP_SYS_NICE in the capabilities(7) man page). error: %s", strerror(errno));
    }
#endif
}

} // namespace KWin
