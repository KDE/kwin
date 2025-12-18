/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "commit_timing_v1.h"

#include "display.h"
#include "surface.h"
#include "surface_p.h"

namespace KWin
{

CommitTimingManagerV1::CommitTimingManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::wp_commit_timing_manager_v1(*display, 1)
{
}

void CommitTimingManagerV1::wp_commit_timing_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CommitTimingManagerV1::wp_commit_timing_manager_v1_get_timer(Resource *resource, uint32_t id, struct ::wl_resource *wlSurface)
{
    const auto surface = SurfaceInterface::get(wlSurface);
    const auto surfacePrivate = SurfaceInterfacePrivate::get(surface);
    if (surfacePrivate->commitTiming) {
        wl_resource_post_error(resource->handle, error_commit_timer_exists, "Attempted to create a second commit timer for the wl_surface");
        return;
    }
    surfacePrivate->commitTiming = new CommitTimingSurfaceV1(resource->client(), id, resource->version(), surface);
}

CommitTimingSurfaceV1::CommitTimingSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface)
    : QtWaylandServer::wp_commit_timer_v1(client, id, version)
    , m_surface(surface)
{
}

CommitTimingSurfaceV1::~CommitTimingSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->commitTiming = nullptr;
    }
}

void CommitTimingSurfaceV1::wp_commit_timer_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void CommitTimingSurfaceV1::wp_commit_timer_v1_set_timestamp(Resource *resource, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, error_surface_destroyed, "Attempted to set a commit time for a destroyed wl_surface");
        return;
    }
    auto &timing = SurfaceInterfacePrivate::get(m_surface)->pending->requestedTiming;
    if (timing.has_value()) {
        wl_resource_post_error(resource->handle, error_timestamp_exists, "The surface already has a commit-timing timestamp");
        return;
    }
    const uint64_t seconds = (uint64_t(tv_sec_hi) << 32) | uint64_t(tv_sec_lo);
    const auto time = std::chrono::seconds(seconds) + std::chrono::nanoseconds(tv_nsec);
    timing = std::chrono::steady_clock::time_point(time);
}

void CommitTimingSurfaceV1::wp_commit_timer_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

}
