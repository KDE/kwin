/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "surfaceinvalidation_v1.h"
#include "display.h"
#include "qwayland-server-surface-invalidation-v1.h"

static const int s_version = 1;

namespace KWin
{

class SurfaceInvalidtionV1Interface : public QObject, public QtWaylandServer::wp_surface_invalidation_v1
{
    Q_OBJECT
public:
    SurfaceInvalidtionV1Interface(wl_resource *handle);

protected:
    void wp_surface_invalidation_v1_destroy(Resource *resource) override;
    void wp_surface_invalidation_v1_destroy_resource(Resource *resource) override;
};

class SurfaceInvalidtionManagerV1InterfacePrivate : public QObject, public QtWaylandServer::wp_surface_invalidation_manager_v1
{
    Q_OBJECT
public:
    SurfaceInvalidtionManagerV1InterfacePrivate() = default;
    ~SurfaceInvalidtionManagerV1InterfacePrivate() = default;
    QList<SurfaceInvalidtionV1Interface *> m_surfaceInvalidationList;
    int m_serial = 0;

protected:
    void wp_surface_invalidation_manager_v1_destroy(Resource *resource) override;
    void wp_surface_invalidation_manager_v1_get_surface_invalidation(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
};

void SurfaceInvalidtionManagerV1InterfacePrivate::wp_surface_invalidation_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SurfaceInvalidtionManagerV1InterfacePrivate::wp_surface_invalidation_manager_v1_get_surface_invalidation(Resource *resource, uint32_t id, struct ::wl_resource *surface_resource)
{
    wl_resource *surfaceInvalidationResource = wl_resource_create(resource->client(), &wp_surface_invalidation_v1_interface, resource->version(), id);

    auto surfaceInvalidation = new SurfaceInvalidtionV1Interface(surfaceInvalidationResource);
    connect(surfaceInvalidation, &QObject::destroyed, this, [this, surfaceInvalidation]() {
        m_surfaceInvalidationList.removeOne(surfaceInvalidation);
    });
    m_surfaceInvalidationList.append(surfaceInvalidation);
}

SurfaceInvalidtionV1Interface::SurfaceInvalidtionV1Interface(wl_resource *resource)
    : QtWaylandServer::wp_surface_invalidation_v1(resource)
{
}

void SurfaceInvalidtionV1Interface::wp_surface_invalidation_v1_destroy_resource(Resource *resource)
{
    // remove from list
    delete this;
}

void SurfaceInvalidtionV1Interface::wp_surface_invalidation_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

SurfaceInvalidationManagerV1Interface::SurfaceInvalidationManagerV1Interface(Display *display, QObject *parent)
    : d(new SurfaceInvalidtionManagerV1InterfacePrivate)
{
    d->init(*display, s_version);
}

SurfaceInvalidationManagerV1Interface::~SurfaceInvalidationManagerV1Interface()
{
}

void SurfaceInvalidationManagerV1Interface::invalidateSurfaces()
{
    d->m_serial++;
    for (auto surfaceInvalidation : d->m_surfaceInvalidationList) {
        surfaceInvalidation->send_invalidated(d->m_serial);
    }
}

} // namespace KWin

#include "surfaceinvalidation_v1.moc"
