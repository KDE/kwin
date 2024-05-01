/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "wayland/blurv1.h"
#include "utils/resource.h"
#include "wayland/display.h"
#include "wayland/region_p.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/surface_p.h"

#include "wayland/qwayland-server-kde-blur-v1.h"

#include <drm_fourcc.h>

namespace KWin
{

class BlurManagerV1InterfacePrivate : public QtWaylandServer::kde_blur_manager_v1
{
public:
    BlurManagerV1InterfacePrivate(BlurManagerV1Interface *q, Display *display);

    BlurManagerV1Interface *q;

protected:
    void kde_blur_manager_v1_destroy_global() override;
    void kde_blur_manager_v1_destroy(Resource *resource) override;
    void kde_blur_manager_v1_get_blur(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void kde_blur_manager_v1_get_blur_mask(Resource *resource, uint32_t id) override;
};

class BlurMaskV1Interface : public QtWaylandServer::kde_blur_mask_v1
{
public:
    BlurMaskV1Interface(wl_client *client, int id, int version);

    static BlurMaskV1Interface *get(wl_resource *resource);

    GraphicsBufferRef buffer;
    int scale = 120;
    QRect center;

protected:
    void kde_blur_mask_v1_destroy_resource(Resource *resource) override;
    void kde_blur_mask_v1_destroy(Resource *resource) override;
    void kde_blur_mask_v1_set_mask(Resource *resource, struct ::wl_resource *mask) override;
    void kde_blur_mask_v1_set_scale(Resource *resource, uint32_t scale) override;
    void kde_blur_mask_v1_set_center(Resource *resource, uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
};

struct BlurV1Commit
{
    std::optional<std::optional<BlurMaskV1>> mask;
    std::optional<std::optional<QRegion>> shape;
};

class BlurV1InterfacePrivate : public SurfaceExtension<BlurV1Commit>, public QtWaylandServer::kde_blur_surface_v1
{
public:
    BlurV1InterfacePrivate(BlurV1Interface *q, SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version);

    void apply(BlurV1Commit *commit) override;

    void fuckingStateHack();

    BlurV1Interface *q;
    QPointer<SurfaceInterface> surface;

    std::optional<BlurMaskV1> mask;
    std::optional<QRegion> shape;

protected:
    void kde_blur_surface_v1_destroy_resource(Resource *resource) override;
    void kde_blur_surface_v1_destroy(Resource *resource) override;
    void kde_blur_surface_v1_set_mask(Resource *resource, struct ::wl_resource *mask) override;
    void kde_blur_surface_v1_unset_mask(Resource *resource) override;
    void kde_blur_surface_v1_set_shape(Resource *resource, struct ::wl_resource *shape) override;
    void kde_blur_surface_v1_unset_shape(Resource *resource) override;
};

BlurManagerV1InterfacePrivate::BlurManagerV1InterfacePrivate(BlurManagerV1Interface *q, Display *display)
    : QtWaylandServer::kde_blur_manager_v1(*display, 1)
    , q(q)
{
}

void BlurManagerV1InterfacePrivate::kde_blur_manager_v1_destroy_global()
{
    delete q;
}

void BlurManagerV1InterfacePrivate::kde_blur_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void BlurManagerV1InterfacePrivate::kde_blur_manager_v1_get_blur(Resource *resource, uint32_t id, struct ::wl_resource *surfaceResource)
{
    SurfaceInterface *surface = SurfaceInterface::get(surfaceResource);
    new BlurV1Interface(surface, resource->client(), id, resource->version());
}

void BlurManagerV1InterfacePrivate::kde_blur_manager_v1_get_blur_mask(Resource *resource, uint32_t id)
{
    new BlurMaskV1Interface(resource->client(), id, resource->version());
}

BlurManagerV1Interface::BlurManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new BlurManagerV1InterfacePrivate(this, display))
{
}

BlurManagerV1Interface::~BlurManagerV1Interface()
{
}

void BlurManagerV1Interface::remove()
{
    d->globalRemove();
}

BlurMaskV1Interface::BlurMaskV1Interface(wl_client *client, int id, int version)
    : QtWaylandServer::kde_blur_mask_v1(client, id, version)
{
}

BlurMaskV1Interface *BlurMaskV1Interface::get(wl_resource *resource)
{
    return resource_cast<BlurMaskV1Interface *>(resource);
}

void BlurMaskV1Interface::kde_blur_mask_v1_destroy_resource(Resource *resource)
{
    delete this;
}

void BlurMaskV1Interface::kde_blur_mask_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void BlurMaskV1Interface::kde_blur_mask_v1_set_mask(Resource *resource, ::wl_resource *mask)
{
    GraphicsBuffer *buffer = Display::bufferForResource(mask);

    const ShmAttributes *shmAttributes = buffer->shmAttributes();
    if (!shmAttributes) {
        wl_resource_post_error(resource->handle, error_invalid_mask, "the mask buffer is not a wl_shm_buffer");
        return;
    }
    if (shmAttributes->format != DRM_FORMAT_R8) {
        wl_resource_post_error(resource->handle, error_invalid_mask, "the mask buffer must have WL_SHM_FORMAT_R8 format");
        return;
    }

    this->buffer = buffer;
}

void BlurMaskV1Interface::kde_blur_mask_v1_set_scale(Resource *resource, uint32_t scale)
{
    this->scale = scale;
}

void BlurMaskV1Interface::kde_blur_mask_v1_set_center(Resource *resource, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    center = QRect(x, y, width, height);
}

BlurV1InterfacePrivate::BlurV1InterfacePrivate(BlurV1Interface *q, SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version)
    : SurfaceExtension(surface)
    , QtWaylandServer::kde_blur_surface_v1(client, id, version)
    , q(q)
    , surface(surface)
{
}

void BlurV1InterfacePrivate::apply(BlurV1Commit *commit)
{
    if (commit->mask.has_value()) {
        mask = commit->mask.value();
    }
    if (commit->shape.has_value()) {
        shape = commit->shape.value();
    }
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_destroy_resource(Resource *resource)
{
    delete q;
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void BlurV1InterfacePrivate::fuckingStateHack()
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending->blurV1 = q;
    surfacePrivate->pending->blurV1IsSet = true;
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_set_mask(Resource *resource, struct ::wl_resource *mask)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "wl_surface is destroyed");
        return;
    }

    BlurMaskV1Interface *m = BlurMaskV1Interface::get(mask);
    pending.mask.emplace(BlurMaskV1{
        .mask = m->buffer,
        .scale = m->scale,
        .center = m->center,
    });
    fuckingStateHack();
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_unset_mask(Resource *resource)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "wl_surface is destroyed");
        return;
    }

    pending.mask.emplace(std::nullopt);
    fuckingStateHack();
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_set_shape(Resource *resource, struct ::wl_resource *shape)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "wl_surface is destroyed");
        return;
    }

    pending.shape.emplace(RegionInterface::get(shape)->region());
    fuckingStateHack();
}

void BlurV1InterfacePrivate::kde_blur_surface_v1_unset_shape(Resource *resource)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "wl_surface is destroyed");
        return;
    }

    pending.shape.emplace(std::nullopt);
    fuckingStateHack();
}

BlurV1Interface::BlurV1Interface(SurfaceInterface *surface, wl_client *client, uint32_t id, uint32_t version)
    : d(std::make_unique<BlurV1InterfacePrivate>(this, surface, client, id, version))
{
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(surface);
    surfacePrivate->pending->blurV1 = this;
    surfacePrivate->pending->blurV1IsSet = true;
}

BlurV1Interface::~BlurV1Interface()
{
    if (d->surface) {
        SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(d->surface);
        surfacePrivate->pending->blurV1 = nullptr;
        surfacePrivate->pending->blurV1IsSet = true;
    }
}

std::optional<BlurMaskV1> BlurV1Interface::mask() const
{
    return d->mask;
}

std::optional<QRegion> BlurV1Interface::shape() const
{
    return d->shape;
}

} // namespace KWin
