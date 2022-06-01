/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "softwarevsyncmonitor.h"

namespace KWin
{

std::unique_ptr<SoftwareVsyncMonitor> SoftwareVsyncMonitor::create()
{
    return std::unique_ptr<SoftwareVsyncMonitor>{new SoftwareVsyncMonitor()};
}

SoftwareVsyncMonitor::SoftwareVsyncMonitor()
{
    connect(&m_softwareClock, &QTimer::timeout, this, &SoftwareVsyncMonitor::handleSyntheticVsync);
    m_softwareClock.setSingleShot(true);
}

int SoftwareVsyncMonitor::refreshRate() const
{
    return m_refreshRate;
}

void SoftwareVsyncMonitor::setRefreshRate(int refreshRate)
{
    m_refreshRate = refreshRate;
}

void SoftwareVsyncMonitor::handleSyntheticVsync()
{
    Q_EMIT vblankOccurred(m_vblankTimestamp);
}

template<typename T>
T alignTimestamp(const T &timestamp, const T &alignment)
{
    return timestamp + ((alignment - (timestamp % alignment)) % alignment);
}

void SoftwareVsyncMonitor::arm()
{
    if (m_softwareClock.isActive()) {
        return;
    }

    const std::chrono::nanoseconds currentTime(std::chrono::steady_clock::now().time_since_epoch());
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / m_refreshRate);

    m_vblankTimestamp = alignTimestamp(currentTime, vblankInterval);

    m_softwareClock.start(std::chrono::duration_cast<std::chrono::milliseconds>(m_vblankTimestamp - currentTime));
}

} // namespace KWin
