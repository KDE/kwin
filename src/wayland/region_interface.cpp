/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "region_interface.h"
#include "compositor_interface.h"
#include "utils.h"

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{

class RegionInterfacePrivate : public QtWaylandServer::wl_region
{
public:
    RegionInterfacePrivate(RegionInterface *q, wl_resource *resource);

    RegionInterface *q;
    QRegion qtRegion;

protected:
    void region_destroy_resource(Resource *resource) override;
    void region_destroy(Resource *resource) override;
    void region_add(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void region_subtract(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) override;
};

RegionInterfacePrivate::RegionInterfacePrivate(RegionInterface *q, wl_resource *resource)
    : QtWaylandServer::wl_region(resource)
    , q(q)
{
}

void RegionInterfacePrivate::region_destroy_resource(Resource *)
{
    delete q;
}

void RegionInterfacePrivate::region_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RegionInterfacePrivate::region_add(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    qtRegion += QRegion(x, y, width, height);
    emit q->regionChanged(qtRegion);
}

void RegionInterfacePrivate::region_subtract(Resource *, int32_t x, int32_t y, int32_t width, int32_t height)
{
    qtRegion -= QRegion(x, y, width, height);
    emit q->regionChanged(qtRegion);
}

RegionInterface::RegionInterface(CompositorInterface *compositor, wl_resource *resource)
    : QObject(compositor)
    , d(new RegionInterfacePrivate(this, resource))
{
}

RegionInterface::~RegionInterface()
{
}

QRegion RegionInterface::region() const
{
    return d->qtRegion;
}

RegionInterface *RegionInterface::get(wl_resource *native)
{
    if (auto regionPrivate = resource_cast<RegionInterfacePrivate *>(native)) {
        return regionPrivate->q;
    }
    return nullptr;
}

} // namespace KWaylandServer
