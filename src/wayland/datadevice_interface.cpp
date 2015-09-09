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
#include "datadevice_interface.h"
#include "datadevicemanager_interface.h"
#include "dataoffer_interface.h"
#include "datasource_interface.h"
#include "display.h"
#include "resource_p.h"
#include "pointer_interface.h"
#include "seat_interface.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataDeviceInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *seat, DataDeviceInterface *q, DataDeviceManagerInterface *manager, wl_resource *parentResource);
    ~Private();

    DataOfferInterface *createDataOffer(DataSourceInterface *source);

    SeatInterface *seat;
    DataSourceInterface *source = nullptr;
    SurfaceInterface *surface = nullptr;
    SurfaceInterface *icon = nullptr;

    DataSourceInterface *selection = nullptr;

private:
    DataDeviceInterface *q_func() {
        return reinterpret_cast<DataDeviceInterface*>(q);
    }
    void startDrag(DataSourceInterface *dataSource, SurfaceInterface *origin, SurfaceInterface *icon);
    void setSelection(DataSourceInterface *dataSource);
    static void startDragCallback(wl_client *client, wl_resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial);
    static void setSelectionCallback(wl_client *client, wl_resource *resource, wl_resource *source, uint32_t serial);
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_data_device_interface s_interface;
};

const struct wl_data_device_interface DataDeviceInterface::Private::s_interface = {
    startDragCallback,
    setSelectionCallback,
    releaseCallback
};

DataDeviceInterface::Private::Private(SeatInterface *seat, DataDeviceInterface *q, DataDeviceManagerInterface *manager, wl_resource *parentResource)
    : Resource::Private(q, manager, parentResource, &wl_data_device_interface, &s_interface)
    , seat(seat)
{
}

DataDeviceInterface::Private::~Private() = default;

void DataDeviceInterface::Private::startDragCallback(wl_client *client, wl_resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial)
{
    Q_UNUSED(client)
    Q_UNUSED(serial)
    // TODO: verify serial
    cast<Private>(resource)->startDrag(DataSourceInterface::get(source), SurfaceInterface::get(origin), SurfaceInterface::get(icon));
}

void DataDeviceInterface::Private::startDrag(DataSourceInterface *dataSource, SurfaceInterface *origin, SurfaceInterface *i)
{
    if (seat->focusedPointerSurface() != origin) {
        wl_resource_post_error(resource, 0, "Surface doesn't have pointer grab");
        return;
    }
    source = dataSource;
    surface = origin;
    icon = i;
    Q_Q(DataDeviceInterface);
    emit q->dragStarted();
}

void DataDeviceInterface::Private::setSelectionCallback(wl_client *client, wl_resource *resource, wl_resource *source, uint32_t serial)
{
    Q_UNUSED(client)
    Q_UNUSED(serial)
    // TODO: verify serial
    cast<Private>(resource)->setSelection(DataSourceInterface::get(source));
}

void DataDeviceInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    Private *p = reinterpret_cast<Private*>(wl_resource_get_user_data(resource));
    wl_resource_destroy(resource);
    p->q->deleteLater();
}

void DataDeviceInterface::Private::setSelection(DataSourceInterface *dataSource)
{
    Q_Q(DataDeviceInterface);
    selection = dataSource;
    if (selection) {
        emit q->selectionChanged(selection);
    } else {
        emit q->selectionCleared();
    }
}

DataOfferInterface *DataDeviceInterface::Private::createDataOffer(DataSourceInterface *source)
{
    Q_Q(DataDeviceInterface);
    DataOfferInterface *offer = new DataOfferInterface(source, q, resource);
    auto c = q->global()->display()->getConnection(wl_resource_get_client(resource));
    offer->create(c, wl_resource_get_version(resource), 0);
    if (!offer->resource()) {
        // TODO: send error?
        delete offer;
        return nullptr;
    }
    wl_data_device_send_data_offer(resource, offer->resource());
    offer->sendAllOffers();
    return offer;
}

DataDeviceInterface::DataDeviceInterface(SeatInterface *seat, DataDeviceManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(seat, this, parent, parentResource))
{
}

DataDeviceInterface::~DataDeviceInterface() = default;

SeatInterface *DataDeviceInterface::seat() const
{
    Q_D();
    return d->seat;
}

DataSourceInterface *DataDeviceInterface::dragSource() const
{
    Q_D();
    return d->source;
}

SurfaceInterface *DataDeviceInterface::icon() const
{
    Q_D();
    return d->icon;
}

SurfaceInterface *DataDeviceInterface::origin() const
{
    Q_D();
    return d->surface;
}

DataSourceInterface *DataDeviceInterface::selection() const
{
    Q_D();
    return d->selection;
}

void DataDeviceInterface::sendSelection(DataDeviceInterface *other)
{
    Q_D();
    auto r = d->createDataOffer(other->selection());
    if (!r) {
        return;
    }
    if (!d->resource) {
        return;
    }
    wl_data_device_send_selection(d->resource, r->resource());
}

void DataDeviceInterface::sendClearSelection()
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_data_device_send_selection(d->resource, nullptr);
}

DataDeviceInterface::Private *DataDeviceInterface::d_func() const
{
    return reinterpret_cast<DataDeviceInterface::Private*>(d.data());
}

}
}
