/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "committiming.h"
#include "wayland-commit-timing-v1-client-protocol.h"

using namespace std::chrono_literals;

namespace KWin::WaylandClient
{

CommitTimerV1::CommitTimerV1(wp_commit_timer_v1 *handle)
    : m_handle(handle)
{
}

CommitTimerV1::~CommitTimerV1()
{
    wp_commit_timer_v1_destroy(m_handle);
}

void CommitTimerV1::setTimestamp(std::chrono::steady_clock::time_point timestamp)
{
    const std::chrono::nanoseconds time = timestamp.time_since_epoch();
    const uint64_t seconds = time / 1s;
    const uint64_t nanos = (time - std::chrono::seconds(seconds)).count();
    wp_commit_timer_v1_set_timestamp(m_handle, seconds >> 32, seconds & 0xFFFFFFFF, nanos);
}

const QByteArrayView CommitTimingManagerV1::s_interfaceName = wp_commit_timing_manager_v1_interface.name;

CommitTimingManagerV1::CommitTimingManagerV1(::wl_registry *registry, uint32_t name, int version)
    : m_handle(reinterpret_cast<wp_commit_timing_manager_v1 *>(wl_registry_bind(registry, name, &wp_commit_timing_manager_v1_interface, version)))
{
}

CommitTimingManagerV1::~CommitTimingManagerV1()
{
    wp_commit_timing_manager_v1_destroy(m_handle);
}

std::unique_ptr<CommitTimerV1> CommitTimingManagerV1::createTimer(::wl_surface *surface)
{
    return std::make_unique<CommitTimerV1>(wp_commit_timing_manager_v1_get_timer(m_handle, surface));
}

}
