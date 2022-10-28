/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "contrast_interface.h"
#include "display.h"
#include "region_interface_p.h"
#include "surface_interface_p.h"

#include <qwayland-server-contrast.h>

namespace KWaylandServer
{
static const quint32 s_version = 2;

class ContrastManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_contrast_manager
{
public:
    ContrastManagerInterfacePrivate(ContrastManagerInterface *q, Display *display);

    ContrastManagerInterface *q;

protected:
    void org_kde_kwin_contrast_manager_destroy_global() override;
    void org_kde_kwin_contrast_manager_create(Resource *resource, uint32_t id, wl_resource *surface) override;
    void org_kde_kwin_contrast_manager_unset(Resource *resource, wl_resource *surface) override;
};

ContrastManagerInterfacePrivate::ContrastManagerInterfacePrivate(ContrastManagerInterface *q, Display *display)
    : QtWaylandServer::org_kde_kwin_contrast_manager(*display, s_version)
    , q(q)
{
}

void ContrastManagerInterfacePrivate::org_kde_kwin_contrast_manager_destroy_global()
{
    delete q;
}

void ContrastManagerInterfacePrivate::org_kde_kwin_contrast_manager_create(Resource *resource, uint32_t id, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }

    wl_resource *contrast_resource = wl_resource_create(resource->client(), &org_kde_kwin_contrast_interface, resource->version(), id);
    if (!contrast_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    auto contrast = new ContrastInterface(contrast_resource);
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setContrast(contrast);
}

void ContrastManagerInterfacePrivate::org_kde_kwin_contrast_manager_unset(Resource *resource, wl_resource *surface)
{
    SurfaceInterface *s = SurfaceInterface::get(surface);
    if (!s) {
        wl_resource_post_error(resource->handle, 0, "Invalid  surface");
        return;
    }
    SurfaceInterfacePrivate *surfacePrivate = SurfaceInterfacePrivate::get(s);
    surfacePrivate->setContrast(QPointer<ContrastInterface>());
}

ContrastManagerInterface::ContrastManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new ContrastManagerInterfacePrivate(this, display))
{
}

ContrastManagerInterface::~ContrastManagerInterface()
{
}

void ContrastManagerInterface::remove()
{
    d->globalRemove();
}

class ContrastInterfacePrivate : public QtWaylandServer::org_kde_kwin_contrast
{
public:
    ContrastInterfacePrivate(ContrastInterface *_q, wl_resource *resource);

    QRegion pendingRegion;
    QRegion currentRegion;
    qreal pendingContrast;
    qreal currentContrast;
    qreal pendingIntensity;
    qreal currentIntensity;
    qreal pendingSaturation;
    qreal currentSaturation;
    QColor currentFrost;
    QColor pendingFrost;
    ContrastInterface *q;

protected:
    void org_kde_kwin_contrast_commit(Resource *resource) override;
    void org_kde_kwin_contrast_set_region(Resource *resource, wl_resource *region) override;
    void org_kde_kwin_contrast_set_contrast(Resource *resource, wl_fixed_t contrast) override;
    void org_kde_kwin_contrast_set_intensity(Resource *resource, wl_fixed_t intensity) override;
    void org_kde_kwin_contrast_set_saturation(Resource *resource, wl_fixed_t saturation) override;
    void org_kde_kwin_contrast_set_frost(Resource *resource, int r, int g, int b, int a) override;
    void org_kde_kwin_contrast_unset_frost(Resource *resource) override;
    void org_kde_kwin_contrast_release(Resource *resource) override;
    void org_kde_kwin_contrast_destroy_resource(Resource *resource) override;
};

void ContrastInterfacePrivate::org_kde_kwin_contrast_commit(Resource *resource)
{
    currentRegion = pendingRegion;
    currentContrast = pendingContrast;
    currentIntensity = pendingIntensity;
    currentSaturation = pendingSaturation;
    currentFrost = pendingFrost;
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_set_region(Resource *resource, wl_resource *region)
{
    RegionInterface *r = RegionInterface::get(region);
    if (r) {
        pendingRegion = r->region();
    } else {
        pendingRegion = QRegion();
    }
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_set_contrast(Resource *resource, wl_fixed_t contrast)
{
    pendingContrast = wl_fixed_to_double(contrast);
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_set_intensity(Resource *resource, wl_fixed_t intensity)
{
    pendingIntensity = wl_fixed_to_double(intensity);
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_set_saturation(Resource *resource, wl_fixed_t saturation)
{
    pendingSaturation = wl_fixed_to_double(saturation);
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_set_frost(Resource *resource, int r, int g, int b, int a)
{
    pendingFrost = QColor(r, g, b, a);
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_unset_frost(Resource *resource)
{
    pendingFrost = {};
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ContrastInterfacePrivate::org_kde_kwin_contrast_destroy_resource(Resource *resource)
{
    delete q;
}

ContrastInterfacePrivate::ContrastInterfacePrivate(ContrastInterface *_q, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_contrast(resource)
    , q(_q)
{
}

ContrastInterface::ContrastInterface(wl_resource *resource)
    : QObject()
    , d(new ContrastInterfacePrivate(this, resource))
{
}

ContrastInterface::~ContrastInterface() = default;

QRegion ContrastInterface::region() const
{
    return d->currentRegion;
}

qreal ContrastInterface::contrast() const
{
    return d->currentContrast;
}

qreal ContrastInterface::intensity() const
{
    return d->currentIntensity;
}

qreal ContrastInterface::saturation() const
{
    return d->currentSaturation;
}

QColor ContrastInterface::frost() const
{
    return d->currentFrost;
}

}
