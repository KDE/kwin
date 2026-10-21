/*
    SPDX-FileCopyrightText: 2026 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <QByteArrayView>
#include <chrono>
#include <memory>
#include <stdint.h>

struct wl_surface;
struct wp_commit_timing_manager_v1;
struct wp_commit_timer_v1;
struct wl_registry;

namespace KWin::WaylandClient
{

class KWIN_EXPORT CommitTimerV1
{
public:
    explicit CommitTimerV1(wp_commit_timer_v1 *handle);
    ~CommitTimerV1();

    void setTimestamp(std::chrono::steady_clock::time_point timestamp);

private:
    wp_commit_timer_v1 *const m_handle;
};

class KWIN_EXPORT CommitTimingManagerV1
{
public:
    static const QByteArrayView s_interfaceName;

    explicit CommitTimingManagerV1(::wl_registry *registry, uint32_t name, int version);
    ~CommitTimingManagerV1();

    std::unique_ptr<CommitTimerV1> createTimer(::wl_surface *surface);

private:
    wp_commit_timing_manager_v1 *const m_handle;
};

}
