/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "blur_interface.h"
#include "display.h"
#include "region_interface_p.h"
#include "surface_interface_p.h"

#include "qwayland-server-blur.h"

namespace KWaylandServer
{
static const quint32 s_version = 1;

class BlurManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_blur_manager
{
public:
    BlurManagerInterfacePrivate(BlurManagerInterface *q, Display *d);

    BlurManagerInterface *q;

protected:
    void org_kde_kwin_blur_manager_destroy_global() override;
    void org_kde_kwin_blur_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
    void org_kde_kwin_blur_manager_unset(Resource *resource, wl_resource *surface) override;
};

BlurManagerInterfacePrivate::BlurManagerInterfacePrivate(BlurManagerInterface *_q, Display *d)
    : QtWaylandServer::org_kde_kwin_blur_manager(*d, s_version)
    , q(_q)
{
}

void BlurManagerInterfacePrivate::org_kde_kwin_blur_manager_destroy_global()
{
    delete q;
}

void BlurManagerInterfacePrivate::org_kde_kwin_blur_manager_unset(Resource *resource, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setBlur(QPointer<BlurInterface>());
}

void BlurManagerInterfacePrivate::org_kde_kwin_blur_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    wl_resource *blur_resource = wl_resource_create(resource->client(), &org_kde_kwin_blur_interface, resource->version(), id);
    if (!blur_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto blur = new BlurInterface(blur_resource);
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setBlur(blur);
}

BlurManagerInterface::BlurManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new BlurManagerInterfacePrivate(this, display))
{
}

BlurManagerInterface::~BlurManagerInterface()
{
}

void BlurManagerInterface::remove()
{
    d->globalRemove();
}

class BlurInterfacePrivate : public QtWaylandServer::org_kde_kwin_blur
{
public:
    BlurInterfacePrivate(BlurInterface *q, wl_resource *resource);
    QRegion pendingRegion;
    QRegion currentRegion;

    BlurInterface *q;

protected:
    void org_kde_kwin_blur_destroy_resource(Resource *resource) override;
    void org_kde_kwin_blur_commit(Resource *resource) override;
    void org_kde_kwin_blur_set_region(Resource *resource, wl_resource *region) override;
    void org_kde_kwin_blur_release(Resource *resource) override;
};

void BlurInterfacePrivate::org_kde_kwin_blur_commit(Resource *resource)
{
    currentRegion = pendingRegion;
}

void BlurInterfacePrivate::org_kde_kwin_blur_set_region(Resource *resource, wl_resource *region)
{
    RegionInterface *r = RegionInterface::get(region);
    if (r) {
        pendingRegion = r->region();
    } else {
        pendingRegion = QRegion();
    }
}

void BlurInterfacePrivate::org_kde_kwin_blur_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void BlurInterfacePrivate::org_kde_kwin_blur_destroy_resource(Resource *resource)
{
    delete q;
}

BlurInterfacePrivate::BlurInterfacePrivate(BlurInterface *_q, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_blur(resource)
    , q(_q)
{
}

BlurInterface::BlurInterface(wl_resource *resource)
    : QObject()
    , d(new BlurInterfacePrivate(this, resource))
{
}

BlurInterface::~BlurInterface() = default;

QRegion BlurInterface::region()
{
    return d->currentRegion;
}

}
