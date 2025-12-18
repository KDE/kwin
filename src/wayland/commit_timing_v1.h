/*
    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include <QObject>
#include <QPointer>

#include "wayland/qwayland-server-commit-timing-v1.h"

namespace KWin
{

class Display;
class SurfaceInterface;

class CommitTimingManagerV1 : public QObject, private QtWaylandServer::wp_commit_timing_manager_v1
{
    Q_OBJECT
public:
    explicit CommitTimingManagerV1(Display *display, QObject *parent);

private:
    void wp_commit_timing_manager_v1_destroy(Resource *resource) override;
    void wp_commit_timing_manager_v1_get_timer(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

class CommitTimingSurfaceV1 : private QtWaylandServer::wp_commit_timer_v1
{
public:
    explicit CommitTimingSurfaceV1(wl_client *client, uint32_t id, uint32_t version, SurfaceInterface *surface);
    ~CommitTimingSurfaceV1() override;

private:
    void wp_commit_timer_v1_destroy_resource(Resource *resource) override;
    void wp_commit_timer_v1_set_timestamp(Resource *resource, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) override;
    void wp_commit_timer_v1_destroy(Resource *resource) override;

    const QPointer<SurfaceInterface> m_surface;
};

}
