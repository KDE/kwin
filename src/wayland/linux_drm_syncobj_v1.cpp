/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "linux_drm_syncobj_v1.h"
#include "core/drmdevice.h"
#include "core/syncobjtimeline.h"
#include "display.h"
#include "linux_drm_syncobj_v1_p.h"
#include "surface.h"
#include "surface_p.h"
#include "transaction.h"
#include "utils/resource.h"

#include <xf86drm.h>

namespace KWin
{

static constexpr uint32_t s_version = 1;

LinuxDrmSyncObjV1Interface::LinuxDrmSyncObjV1Interface(Display *display, QObject *parent, DrmDevice *drmDevice)
    : QObject(parent)
    , QtWaylandServer::wp_linux_drm_syncobj_manager_v1(*display, s_version)
    , m_drmDevice(drmDevice)
{
}

void LinuxDrmSyncObjV1Interface::wp_linux_drm_syncobj_manager_v1_get_surface(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *surf = SurfaceInterface::get(surface);
    SurfaceInterfacePrivate *priv = SurfaceInterfacePrivate::get(surf);
    if (priv->syncObjV1) {
        wl_resource_post_error(resource->handle, error_surface_exists, "surface already exists");
        return;
    }
    priv->syncObjV1 = new LinuxDrmSyncObjSurfaceV1(surf, resource->client(), id);
}

void LinuxDrmSyncObjV1Interface::wp_linux_drm_syncobj_manager_v1_import_timeline(Resource *resource, uint32_t id, int32_t rawFd)
{
    FileDescriptor fd(rawFd);
    if (isGlobalRemoved()) {
        // to not crash the client, create an inert timeline
        new LinuxDrmSyncObjTimelineV1(resource->client(), id, nullptr);
        return;
    }
    uint32_t handle = 0;
    if (drmSyncobjFDToHandle(m_drmDevice->fileDescriptor(), fd.get(), &handle) != 0) {
        wl_resource_post_error(resource->handle, WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE, "Importing timeline failed");
        return;
    }
    new LinuxDrmSyncObjTimelineV1(resource->client(), id, std::make_unique<SyncTimeline>(m_drmDevice->fileDescriptor(), handle));
}

void LinuxDrmSyncObjV1Interface::wp_linux_drm_syncobj_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void LinuxDrmSyncObjV1Interface::remove()
{
    QtWaylandServer::wp_linux_drm_syncobj_manager_v1::globalRemove();
}

void LinuxDrmSyncObjV1Interface::wp_linux_drm_syncobj_manager_v1_destroy_global()
{
    delete this;
}

LinuxDrmSyncObjTimelineV1::LinuxDrmSyncObjTimelineV1(wl_client *client, uint32_t id, std::unique_ptr<SyncTimeline> &&timeline)
    : QtWaylandServer::wp_linux_drm_syncobj_timeline_v1(client, id, s_version)
    , m_timeline(std::move(timeline))
{
}

LinuxDrmSyncObjTimelineV1::~LinuxDrmSyncObjTimelineV1()
{
}

void LinuxDrmSyncObjTimelineV1::wp_linux_drm_syncobj_timeline_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void LinuxDrmSyncObjTimelineV1::wp_linux_drm_syncobj_timeline_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

std::shared_ptr<SyncTimeline> LinuxDrmSyncObjTimelineV1::timeline() const
{
    return m_timeline;
}

LinuxDrmSyncObjSurfaceV1::LinuxDrmSyncObjSurfaceV1(SurfaceInterface *surface, wl_client *client, uint32_t id)
    : QtWaylandServer::wp_linux_drm_syncobj_surface_v1(client, id, s_version)
    , m_surface(surface)
{
}

LinuxDrmSyncObjSurfaceV1::~LinuxDrmSyncObjSurfaceV1()
{
    if (m_surface) {
        SurfaceInterfacePrivate::get(m_surface)->syncObjV1 = nullptr;
    }
}

void LinuxDrmSyncObjSurfaceV1::wp_linux_drm_syncobj_surface_v1_set_acquire_point(Resource *resource, wl_resource *timeline_resource, uint32_t point_hi, uint32_t point_lo)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface got destroyed already");
        return;
    }
    const auto timeline = resource_cast<LinuxDrmSyncObjTimelineV1 *>(timeline_resource);
    if (!timeline->timeline()) {
        // in the normal case this should never happen, but if it does,
        // there's nothing we can do about it without killing the client
        return;
    }
    const uint64_t point = (uint64_t(point_hi) << 32) | point_lo;
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    priv->pending->acquirePoint.timeline = timeline->timeline();
    priv->pending->acquirePoint.point = point;
}

void LinuxDrmSyncObjSurfaceV1::wp_linux_drm_syncobj_surface_v1_set_release_point(Resource *resource, wl_resource *timeline_resource, uint32_t point_hi, uint32_t point_lo)
{
    if (!m_surface) {
        wl_resource_post_error(resource->handle, WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE, "Surface got destroyed already");
        return;
    }
    const auto timeline = resource_cast<LinuxDrmSyncObjTimelineV1 *>(timeline_resource);
    if (!timeline->timeline()) {
        // in the normal case this should never happen, but if it does,
        // there's nothing we can do about it without killing the client
        return;
    }
    const uint64_t point = (uint64_t(point_hi) << 32) | point_lo;
    SurfaceInterfacePrivate::get(m_surface)->pending->releasePoint = std::make_unique<SyncReleasePoint>(timeline->timeline(), point);
}

void LinuxDrmSyncObjSurfaceV1::wp_linux_drm_syncobj_surface_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void LinuxDrmSyncObjSurfaceV1::wp_linux_drm_syncobj_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

bool LinuxDrmSyncObjSurfaceV1::maybeEmitProtocolErrors()
{
    const auto priv = SurfaceInterfacePrivate::get(m_surface);
    if ((!priv->pending->bufferIsSet || !priv->pending->buffer) && !priv->pending->acquirePoint.timeline && !priv->pending->releasePoint) {
        return false;
    }
    if (!priv->pending->acquirePoint.timeline) {
        wl_resource_post_error(resource()->handle, error_no_acquire_point, "explicit sync is used, but no acquire point is set");
        return true;
    }
    if (!priv->pending->releasePoint) {
        wl_resource_post_error(resource()->handle, error_no_release_point, "explicit sync is used, but no release point is set");
        return true;
    }
    if (priv->pending->acquirePoint.timeline.get() == priv->pending->releasePoint->timeline()
        && priv->pending->acquirePoint.point >= priv->pending->releasePoint->timelinePoint()) {
        wl_resource_post_error(resource()->handle, error_conflicting_points, "acquire and release points are on the same timeline and acquire >= release");
        return true;
    }
    if (!priv->pending->buffer) {
        wl_resource_post_error(resource()->handle, error_no_buffer, "explicit sync is used, but no buffer is attached");
        return true;
    }
    if (!priv->pending->buffer->dmabufAttributes()) {
        wl_resource_post_error(resource()->handle, error_unsupported_buffer, "only linux dmabuf buffers are allowed to use explicit sync");
        return true;
    }
    return false;
}
}
