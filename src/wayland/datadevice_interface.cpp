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
#include "datasource_interface.h"
#include "seat_interface.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataDeviceInterface::Private
{
public:
    Private(SeatInterface *seat, DataDeviceInterface *q);
    ~Private();

    void create(wl_client *client, quint32 version, quint32 id);

    SeatInterface *seat;
    wl_resource *device = nullptr;
    DataSourceInterface *source = nullptr;
    SurfaceInterface *surface = nullptr;
    SurfaceInterface *icon = nullptr;

    DataSourceInterface *selection = nullptr;

private:
    void startDrag(DataSourceInterface *dataSource, SurfaceInterface *origin, SurfaceInterface *icon);
    void setSelection(DataSourceInterface *dataSource);
    static void startDragCallback(wl_client *client, wl_resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial);
    static void setSelectionCallback(wl_client *client, wl_resource *resource, wl_resource *source, uint32_t serial);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    DataDeviceInterface *q;

    static const struct wl_data_device_interface s_interface;
};

const struct wl_data_device_interface DataDeviceInterface::Private::s_interface = {
    startDragCallback,
    setSelectionCallback
};

DataDeviceInterface::Private::Private(SeatInterface *seat, DataDeviceInterface *q)
    : seat(seat)
    , q(q)
{
}

DataDeviceInterface::Private::~Private()
{
    if (device) {
        wl_resource_destroy(device);
    }
}

void DataDeviceInterface::Private::startDragCallback(wl_client *client, wl_resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial)
{
    Q_UNUSED(client)
    Q_UNUSED(serial)
    // TODO: verify serial
    cast(resource)->startDrag(DataSourceInterface::get(source), SurfaceInterface::get(origin), SurfaceInterface::get(icon));
}

void DataDeviceInterface::Private::startDrag(DataSourceInterface *dataSource, SurfaceInterface *origin, SurfaceInterface *i)
{
    if (seat->pointer()->focusedSurface() != origin) {
        wl_resource_post_error(device, 0, "Surface doesn't have pointer grab");
        return;
    }
    source = dataSource;
    surface = origin;
    icon = i;
    emit q->dragStarted();
}

void DataDeviceInterface::Private::setSelectionCallback(wl_client *client, wl_resource *resource, wl_resource *source, uint32_t serial)
{
    Q_UNUSED(client)
    Q_UNUSED(serial)
    // TODO: verify serial
    cast(resource)->setSelection(DataSourceInterface::get(source));
}

void DataDeviceInterface::Private::setSelection(DataSourceInterface *dataSource)
{
    selection = dataSource;
    if (selection) {
        emit q->selectionChanged(selection);
    } else {
        emit q->selectionCleared();
    }
}

void DataDeviceInterface::Private::unbind(wl_resource *resource)
{
    auto s = cast(resource);
    s->device = nullptr;
    s->q->deleteLater();
}

void DataDeviceInterface::Private::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!device);
    device = wl_resource_create(client, &wl_data_device_interface, version, id);
    if (!device) {
        return;
    }
    wl_resource_set_implementation(device, &s_interface, this, unbind);
}


DataDeviceInterface::DataDeviceInterface(SeatInterface *seat, DataDeviceManagerInterface *parent)
    : QObject(/*parent*/)
    , d(new Private(seat, this))
{
}

DataDeviceInterface::~DataDeviceInterface() = default;

void DataDeviceInterface::create(wl_client *client, quint32 version, quint32 id)
{
    d->create(client, version, id);
}

SeatInterface *DataDeviceInterface::seat() const
{
    return d->seat;
}

wl_resource *DataDeviceInterface::resource() const
{
    return d->device;
}

DataSourceInterface *DataDeviceInterface::dragSource() const
{
    return d->source;
}

SurfaceInterface *DataDeviceInterface::icon() const
{
    return d->icon;
}

SurfaceInterface *DataDeviceInterface::origin() const
{
    return d->surface;
}

DataSourceInterface *DataDeviceInterface::selection() const
{
    return d->selection;
}

}
}
