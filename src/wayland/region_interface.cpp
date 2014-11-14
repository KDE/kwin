/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "region_interface.h"
#include "resource_p.h"
#include "compositor_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class RegionInterface::Private : public Resource::Private
{
public:
    Private(CompositorInterface *compositor, RegionInterface *q);
    ~Private();
    void create(wl_client *client, quint32 version, quint32 id) override;
    QRegion qtRegion;

    static RegionInterface *get(wl_resource *native);

private:
    void add(const QRect &rect);
    void subtract(const QRect &rect);

    static void unbind(wl_resource *r);
    static void destroyCallback(wl_client *client, wl_resource *r);
    static void addCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height);
    static void subtractCallback(wl_client *client, wl_resource *r, int32_t x, int32_t y, int32_t width, int32_t height);

    static const struct wl_region_interface s_interface;
    RegionInterface *q;
};

const struct wl_region_interface RegionInterface::Private::s_interface = {
    destroyCallback,
    addCallback,
    subtractCallback
};

RegionInterface::Private::Private(CompositorInterface *compositor, RegionInterface *q)
    : Resource::Private(compositor)
    , q(q)
{
}

RegionInterface::Private::~Private() = default;

void RegionInterface::Private::add(const QRect &rect)
{
    qtRegion = qtRegion.united(rect);
    emit q->regionChanged(qtRegion);
}

void RegionInterface::Private::subtract(const QRect &rect)
{
    if (qtRegion.isEmpty()) {
        return;
    }
    qtRegion = qtRegion.subtracted(rect);
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

void RegionInterface::Private::destroyCallback(wl_client *client, wl_resource *r)
{
    Q_UNUSED(client)
    cast<Private>(r)->q->deleteLater();
}

void RegionInterface::Private::unbind(wl_resource *r)
{
    auto region = cast<Private>(r);
    region->resource = nullptr;
    region->q->deleteLater();
}

void RegionInterface::Private::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!resource);
    resource = wl_resource_create(client, &wl_region_interface, version, id);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

RegionInterface *RegionInterface::Private::get(wl_resource *native)
{
    if (!native) {
        return nullptr;
    }
    return cast<Private>(native)->q;
}

RegionInterface::RegionInterface(CompositorInterface *parent)
    : Resource(new Private(parent, this), parent)
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
    return Private::get(native);
}

RegionInterface::Private *RegionInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
