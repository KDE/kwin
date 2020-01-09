/*
 * Copyright (C) 2019 Vlad Zahorodnii <vladzzag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "clockskewnotifierengine_linux.h"

#include <QSocketNotifier>

#include <fcntl.h>
#include <sys/timerfd.h>
#include <unistd.h>

#ifndef TFD_TIMER_CANCEL_ON_SET // only available in newer glib
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#endif

namespace KWin
{

LinuxClockSkewNotifierEngine *LinuxClockSkewNotifierEngine::create(QObject *parent)
{
    const int fd = timerfd_create(CLOCK_REALTIME, O_CLOEXEC | O_NONBLOCK);
    if (fd == -1) {
        qWarning("Couldn't create clock skew notifier engine: %s", strerror(errno));
        return nullptr;
    }

    const itimerspec spec = {};
    const int ret = timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec, nullptr);
    if (ret == -1) {
        qWarning("Couldn't create clock skew notifier engine: %s", strerror(errno));
        close(fd);
        return nullptr;
    }

    return new LinuxClockSkewNotifierEngine(fd, parent);
}

LinuxClockSkewNotifierEngine::LinuxClockSkewNotifierEngine(int fd, QObject *parent)
    : ClockSkewNotifierEngine(parent)
    , m_fd(fd)
{
    const QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &LinuxClockSkewNotifierEngine::handleTimerCancelled);
}

LinuxClockSkewNotifierEngine::~LinuxClockSkewNotifierEngine()
{
    close(m_fd);
}

void LinuxClockSkewNotifierEngine::handleTimerCancelled()
{
    uint64_t expirationCount;
    read(m_fd, &expirationCount, sizeof(expirationCount));

    emit clockSkewed();
}

} // namespace KWin
