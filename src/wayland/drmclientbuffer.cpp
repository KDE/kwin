/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "drmclientbuffer.h"
#include "display.h"
#include "utils/common.h"

#include "qwayland-server-drm.h"

namespace KWaylandServer
{

static constexpr int s_version = 2;

class DrmClientBufferIntegrationPrivate : public QtWaylandServer::wl_drm
{
public:
    explicit DrmClientBufferIntegrationPrivate(Display *display);

    QString nodeName;

protected:
    void drm_bind_resource(Resource *resource) override;
    void drm_authenticate(Resource *resource, uint32_t id) override;
    void drm_create_buffer(Resource *resource,
                           uint32_t id,
                           uint32_t name,
                           int32_t width,
                           int32_t height,
                           uint32_t stride,
                           uint32_t format) override;
    void drm_create_planar_buffer(Resource *resource,
                                  uint32_t id,
                                  uint32_t name,
                                  int32_t width,
                                  int32_t height,
                                  uint32_t format,
                                  int32_t offset0,
                                  int32_t stride0,
                                  int32_t offset1,
                                  int32_t stride1,
                                  int32_t offset2,
                                  int32_t stride2) override;
    void drm_create_prime_buffer(Resource *resource,
                                 uint32_t id,
                                 int32_t name,
                                 int32_t width,
                                 int32_t height,
                                 uint32_t format,
                                 int32_t offset0,
                                 int32_t stride0,
                                 int32_t offset1,
                                 int32_t stride1,
                                 int32_t offset2,
                                 int32_t stride2) override;
};

DrmClientBufferIntegrationPrivate::DrmClientBufferIntegrationPrivate(Display *display)
    : QtWaylandServer::wl_drm(*display, s_version)
{
}

void DrmClientBufferIntegrationPrivate::drm_bind_resource(Resource *resource)
{
    send_device(resource->handle, nodeName);
    send_capabilities(resource->handle, capability_prime);
}

void DrmClientBufferIntegrationPrivate::drm_authenticate(Resource *resource, uint32_t id)
{
    send_authenticated(resource->handle);
}

void DrmClientBufferIntegrationPrivate::drm_create_buffer(Resource *resource,
                                                          uint32_t id,
                                                          uint32_t name,
                                                          int32_t width,
                                                          int32_t height,
                                                          uint32_t stride,
                                                          uint32_t format)
{
    wl_resource_post_error(resource->handle, 0, "wl_drm.create_buffer is not implemented");
}

void DrmClientBufferIntegrationPrivate::drm_create_planar_buffer(Resource *resource,
                                                                 uint32_t id,
                                                                 uint32_t name,
                                                                 int32_t width,
                                                                 int32_t height,
                                                                 uint32_t format,
                                                                 int32_t offset0,
                                                                 int32_t stride0,
                                                                 int32_t offset1,
                                                                 int32_t stride1,
                                                                 int32_t offset2,
                                                                 int32_t stride2)
{
    wl_resource_post_error(resource->handle, 0, "wl_drm.create_planar_buffer is not implemented");
}

void DrmClientBufferIntegrationPrivate::drm_create_prime_buffer(Resource *resource,
                                                                uint32_t id,
                                                                int32_t name,
                                                                int32_t width,
                                                                int32_t height,
                                                                uint32_t format,
                                                                int32_t offset0,
                                                                int32_t stride0,
                                                                int32_t offset1,
                                                                int32_t stride1,
                                                                int32_t offset2,
                                                                int32_t stride2)
{
    close(name);
    wl_resource_post_error(resource->handle, 0, "wl_drm.create_prime_buffer is not implemented");
}

DrmClientBufferIntegration::DrmClientBufferIntegration(Display *display)
    : QObject(display)
    , d(std::make_unique<DrmClientBufferIntegrationPrivate>(display))
{
}

DrmClientBufferIntegration::~DrmClientBufferIntegration()
{
}

void DrmClientBufferIntegration::setDevice(const QString &node)
{
    d->nodeName = node;
}

} // namespace KWaylandServer
