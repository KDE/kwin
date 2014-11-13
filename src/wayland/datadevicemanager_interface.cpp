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
#include "datadevicemanager_interface.h"
#include "global_p.h"
#include "display.h"
#include "seat_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

static const quint32 s_version = 1;

class DataDeviceManagerInterface::Private : public Global::Private
{
public:
    Private(DataDeviceManagerInterface *q, Display *d);
    void create() override;

private:
    void bind(wl_client *client, uint32_t version, uint32_t id);
    void createDataSource(wl_client *client, wl_resource *resource, uint32_t id);
    void getDataDevice(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat);

    static void bind(wl_client *client, void *data, uint32_t version, uint32_t id);
    static void unbind(wl_resource *resource);
    static void createDataSourceCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getDataDeviceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    DataDeviceManagerInterface *q;
    static const struct wl_data_device_manager_interface s_interface;
};

const struct wl_data_device_manager_interface DataDeviceManagerInterface::Private::s_interface = {
    createDataSourceCallback,
    getDataDeviceCallback
};

DataDeviceManagerInterface::Private::Private(DataDeviceManagerInterface *q, Display *d)
    : Global::Private(d)
    , q(q)
{
}

void DataDeviceManagerInterface::Private::bind(wl_client *client, void *data, uint32_t version, uint32_t id)
{
    auto m = reinterpret_cast<DataDeviceManagerInterface::Private*>(data);
    m->bind(client, version, id);
}

void DataDeviceManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *resource = wl_resource_create(client, &wl_data_device_manager_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void DataDeviceManagerInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
}

void DataDeviceManagerInterface::Private::createDataSourceCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->createDataSource(client, resource, id);
}

void DataDeviceManagerInterface::Private::createDataSource(wl_client *client, wl_resource *resource, uint32_t id)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(id)
    DataSourceInterface *dataSource = new DataSourceInterface(q);
    dataSource->create(client, wl_resource_get_version(resource), id);
    if (!dataSource->resource()) {
        wl_resource_post_no_memory(resource);
        delete dataSource;
        return;
    }
    emit q->dataSourceCreated(dataSource);
}

void DataDeviceManagerInterface::Private::getDataDeviceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat)
{
    cast(resource)->getDataDevice(client, resource, id, seat);
}

void DataDeviceManagerInterface::Private::getDataDevice(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat)
{
    DataDeviceInterface *dataDevice = new DataDeviceInterface(SeatInterface::get(seat), q);
    dataDevice->create(client, wl_resource_get_version(resource), id);
    if (!dataDevice->resource()) {
        wl_resource_post_no_memory(resource);
        return;
    }
    emit q->dataDeviceCreated(dataDevice);
}

void DataDeviceManagerInterface::Private::create()
{
    Q_ASSERT(!global);
    global = wl_global_create(*display, &wl_data_device_manager_interface, s_version, this, bind);
}

DataDeviceManagerInterface::DataDeviceManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

DataDeviceManagerInterface::~DataDeviceManagerInterface() = default;

}
}
