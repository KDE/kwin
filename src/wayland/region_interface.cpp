/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "region_interface.h"
#include "resource_p.h"
#include "compositor_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
{

class RegionInterface::Private : public Resource::Private
{
public:
    Private(CompositorInterface *compositor, RegionInterface *q, wl_resource *parentResource);
    ~Private();
    QRegion qtRegion;

private:
    RegionInterface *q_func() {
        return reinterpret_cast<RegionInterface*>(q);
    }
    void add(const QRect &rect);
    void subtract(const QRect &rect);

    static void addCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height);
    static void subtractCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height);

    static const struct wl_region_interface s_interface;
};

#ifndef K_DOXYGEN
const struct wl_region_interface RegionInterface::Private::s_interface = {
    resourceDestroyedCallback,
    addCallback,
    subtractCallback
};
#endif

RegionInterface::Private::Private(CompositorInterface *compositor, RegionInterface *q, wl_resource *parentResource)
    : Resource::Private(q, compositor, parentResource, &wl_region_interface, &s_interface)
{
}

RegionInterface::Private::~Private() = default;

void RegionInterface::Private::add(const QRect &rect)
{
    qtRegion = qtRegion.united(rect);
    Q_Q(RegionInterface);
    emit q->regionChanged(qtRegion);
}

void RegionInterface::Private::subtract(const QRect &rect)
{
    if (qtRegion.isEmpty()) {
        return;
    }
    qtRegion = qtRegion.subtracted(rect);
    Q_Q(RegionInterface);
    emit q->regionChanged(qtRegion);
}

void RegionInterface::Private::addCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(r)->add(QRect(x, y, width, height));
}

void RegionInterface::Private::subtractCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height)
{
    Q_UNUSED(client)
    cast<Private>(r)->subtract(QRect(x, y, width, height));
}

RegionInterface::RegionInterface(CompositorInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, this, parentResource))
{
}

RegionInterface::~RegionInterface() = default;

QRegion RegionInterface::region() const
{
    Q_D();
    return d->qtRegion;
}

RegionInterface *RegionInterface::get(wl_resource *native)
{
    return Private::get<RegionInterface>(native);
}

RegionInterface::Private *RegionInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
