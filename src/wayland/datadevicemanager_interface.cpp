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
#include "datasource_interface.h"
#include "global_p.h"
#include "display.h"
#include "seat_interface_p.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataDeviceManagerInterface::Private : public Global::Private
{
public:
    Private(DataDeviceManagerInterface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createDataSource(wl_client *client, wl_resource *resource, uint32_t id);
    void getDataDevice(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat);

    static void unbind(wl_resource *resource);
    static void createDataSourceCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getDataDeviceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    DataDeviceManagerInterface *q;
    static const struct wl_data_device_manager_interface s_interface;
    static const quint32 s_version;
    static const qint32 s_dataDeviceVersion;
    static const qint32 s_dataSourceVersion;
};

const quint32 DataDeviceManagerInterface::Private::s_version = 3;
const qint32 DataDeviceManagerInterface::Private::s_dataDeviceVersion = 3;
const qint32 DataDeviceManagerInterface::Private::s_dataSourceVersion = 3;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_data_device_manager_interface DataDeviceManagerInterface::Private::s_interface = {
    createDataSourceCallback,
    getDataDeviceCallback
};
#endif

DataDeviceManagerInterface::Private::Private(DataDeviceManagerInterface *q, Display *d)
    : Global::Private(d, &wl_data_device_manager_interface, s_version)
    , q(q)
{
}

void DataDeviceManagerInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&wl_data_device_manager_interface, qMin(version, s_version), id);
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
    DataSourceInterface *dataSource = new DataSourceInterface(q, resource);
    dataSource->create(display->getConnection(client), qMin(wl_resource_get_version(resource), s_dataSourceVersion) , id);
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
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    DataDeviceInterface *dataDevice = new DataDeviceInterface(s, q, resource);
    dataDevice->create(display->getConnection(client), qMin(wl_resource_get_version(resource), s_dataDeviceVersion), id);
    if (!dataDevice->resource()) {
        wl_resource_post_no_memory(resource);
        return;
    }
    s->d_func()->registerDataDevice(dataDevice);
    emit q->dataDeviceCreated(dataDevice);
}

DataDeviceManagerInterface::DataDeviceManagerInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

DataDeviceManagerInterface::~DataDeviceManagerInterface() = default;

}
}
