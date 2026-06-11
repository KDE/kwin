/*
    SPDX-FileCopyrightText: 2026 Jakub Okoński <jakub@okonski.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/common.h"
#include "utils/precisetimer.h"

#include <sys/timerfd.h>
#include <unistd.h>

namespace KWin
{

PreciseTimer::PreciseTimer()
{
    m_timerFd = FileDescriptor{timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK)};
    if (!m_timerFd.isValid()) {
        qFatal("Failed to create PreciseTimer timerfd: %s", strerror(errno));
    }
    m_notifier = std::make_unique<QSocketNotifier>(m_timerFd.get(), QSocketNotifier::Read);
    QObject::connect(m_notifier.get(), &QSocketNotifier::activated, this, &PreciseTimer::timerFdReadable);
}

void PreciseTimer::timerFdReadable()
{
    uint64_t expirations;
    const ssize_t bytesRead = read(m_timerFd.get(), &expirations, sizeof(expirations));
    if (!m_active) {
        return;
    }
    if (bytesRead == sizeof(expirations)) {
        m_active = false;
        Q_EMIT timeout();
    } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        qCWarning(KWIN_CORE) << "Failed to read PreciseTimer timerfd:" << strerror(errno);
        m_active = false;
    }
}

void PreciseTimer::start(std::chrono::nanoseconds deadline)
{
    drain();

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(deadline);
    const auto nanoseconds = deadline - seconds;
    itimerspec spec = {};
    spec.it_value.tv_sec = seconds.count();
    spec.it_value.tv_nsec = nanoseconds.count();

    if (timerfd_settime(m_timerFd.get(), TFD_TIMER_ABSTIME, &spec, nullptr) != 0) {
        qFatal("Failed to arm PreciseTimer timerfd: %s", strerror(errno));
    }
    m_active = true;
}

void PreciseTimer::stop()
{
    const itimerspec spec = {};
    if (timerfd_settime(m_timerFd.get(), 0, &spec, nullptr) != 0) {
        qCWarning(KWIN_CORE) << "Failed to disarm PreciseTimer timerfd:" << strerror(errno);
    }
    m_active = false;
    drain();
}

bool PreciseTimer::isActive() const
{
    return m_active;
}

void PreciseTimer::drain()
{
    uint64_t expirations;
    const auto b = read(m_timerFd.get(), &expirations, sizeof(expirations));
    Q_UNUSED(b);
}

}

#include "moc_precisetimer.cpp"
