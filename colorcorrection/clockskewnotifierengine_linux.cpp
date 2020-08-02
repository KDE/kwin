/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
