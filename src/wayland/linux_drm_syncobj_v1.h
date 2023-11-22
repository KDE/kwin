/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "qwayland-server-linux-drm-syncobj-v1.h"

#include <QObject>
#include <QPointer>

namespace KWin
{

class Display;
class SurfaceInterface;
class RenderBackend;
class SyncTimeline;

class LinuxDrmSyncObjV1Interface : public QObject, private QtWaylandServer::wp_linux_drm_syncobj_manager_v1
{
public:
    explicit LinuxDrmSyncObjV1Interface(Display *display, QObject *parent = nullptr);

    void setRenderBackend(RenderBackend *backend);
    void remove();

private:
    void wp_linux_drm_syncobj_manager_v1_get_surface(Resource *resource, uint32_t id, wl_resource *surface) override;
    void wp_linux_drm_syncobj_manager_v1_import_timeline(Resource *resource, uint32_t id, int32_t fd) override;
    void wp_linux_drm_syncobj_manager_v1_destroy(Resource *resource) override;
    void wp_linux_drm_syncobj_manager_v1_destroy_global() override;

    QPointer<RenderBackend> m_renderBackend;
};

class LinuxDrmSyncObjTimelineV1 : private QtWaylandServer::wp_linux_drm_syncobj_timeline_v1
{
public:
    explicit LinuxDrmSyncObjTimelineV1(wl_client *client, uint32_t id, std::unique_ptr<SyncTimeline> &&timeline);
    ~LinuxDrmSyncObjTimelineV1() override;

    /**
     * may be nullptr!
     */
    std::shared_ptr<SyncTimeline> timeline() const;

    static LinuxDrmSyncObjTimelineV1 *get(wl_resource *resource);

private:
    const std::shared_ptr<SyncTimeline> m_timeline;
};

class LinuxDrmSyncObjSurfaceV1 : private QtWaylandServer::wp_linux_drm_syncobj_surface_v1
{
public:
    explicit LinuxDrmSyncObjSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id);
    ~LinuxDrmSyncObjSurfaceV1() override;

private:
    const QPointer<SurfaceInterface> m_surface;

    void wp_linux_drm_syncobj_surface_v1_set_acquire_point(Resource *resource, wl_resource *timeline, uint32_t point_hi, uint32_t point_lo) override;
    void wp_linux_drm_syncobj_surface_v1_set_release_point(Resource *resource, wl_resource *timeline, uint32_t point_hi, uint32_t point_lo) override;
    void wp_linux_drm_syncobj_surface_v1_destroy_resource(Resource *resource) override;
    void wp_linux_drm_syncobj_surface_v1_destroy(Resource *resource) override;
};

}
