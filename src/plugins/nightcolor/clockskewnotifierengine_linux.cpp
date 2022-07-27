/*
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clockskewnotifierengine_linux.h"

#include <QSocketNotifier>

#include <cerrno>
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
    FileDescriptor fd{timerfd_create(CLOCK_REALTIME, O_CLOEXEC | O_NONBLOCK)};
    if (!fd.isValid()) {
        qWarning("Couldn't create clock skew notifier engine: %s", strerror(errno));
        return nullptr;
    }

    const itimerspec spec = {};
    const int ret = timerfd_settime(fd.get(), TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec, nullptr);
    if (ret == -1) {
        qWarning("Couldn't create clock skew notifier engine: %s", strerror(errno));
        return nullptr;
    }
    return new LinuxClockSkewNotifierEngine(std::move(fd), parent);
}

LinuxClockSkewNotifierEngine::LinuxClockSkewNotifierEngine(FileDescriptor &&fd, QObject *parent)
    : ClockSkewNotifierEngine(parent)
    , m_fd(std::move(fd))
{
    const QSocketNotifier *notifier = new QSocketNotifier(m_fd.get(), QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, &LinuxClockSkewNotifierEngine::handleTimerCancelled);
}

void LinuxClockSkewNotifierEngine::handleTimerCancelled()
{
    uint64_t expirationCount;
    read(m_fd.get(), &expirationCount, sizeof(expirationCount));

    Q_EMIT clockSkewed();
}

} // namespace KWin
