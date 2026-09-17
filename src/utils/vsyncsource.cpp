/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/vsyncsource.h"

using namespace std::chrono_literals;

namespace KWin
{

VsyncSource::VsyncSource()
{
    connect(&m_softwareClock, &PreciseTimer::timeout, this, &VsyncSource::handleSyntheticVsync);
}

int VsyncSource::refreshRate() const
{
    return m_refreshRate;
}

void VsyncSource::setRefreshRate(int refreshRate)
{
    m_refreshRate = refreshRate;
}

void VsyncSource::handleSyntheticVsync()
{
    Q_EMIT vblankOccurred(m_vblankTimestamp);
}

std::chrono::steady_clock::time_point alignTimestamp(std::chrono::steady_clock::time_point timestamp, std::chrono::nanoseconds alignment)
{
    return timestamp + ((alignment - (timestamp.time_since_epoch() % alignment)) % alignment);
}

void VsyncSource::arm()
{
    if (m_softwareClock.isActive()) {
        return;
    }

    const auto currentTime = std::chrono::steady_clock::now();
    const std::chrono::nanoseconds vblankInterval(1'000'000'000'000ull / m_refreshRate);

    m_vblankTimestamp = alignTimestamp(std::max(currentTime, m_vblankTimestamp + 1ns), vblankInterval);

    m_softwareClock.start(m_vblankTimestamp);
}

void VsyncSource::disarm()
{
    m_softwareClock.stop();
}

} // namespace KWin

#include "moc_vsyncsource.cpp"
