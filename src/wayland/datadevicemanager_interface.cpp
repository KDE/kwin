/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datadevicemanager_interface.h"
#include "datasource_interface.h"
#include "display.h"
#include "seat_interface_p.h"
// Wayland
#include <qwayland-server-wayland.h>

namespace KWaylandServer
{
static const quint32 s_version = 3;

class DataDeviceManagerInterfacePrivate : public QtWaylandServer::wl_data_device_manager
{
public:
    DataDeviceManagerInterfacePrivate(DataDeviceManagerInterface *q, Display *d);

    DataDeviceManagerInterface *q;

private:
    void createDataSource(wl_client *client, wl_resource *resource, uint32_t id);
    void getDataDevice(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *seat);

protected:
    void data_device_manager_create_data_source(Resource *resource, uint32_t id) override;
    void data_device_manager_get_data_device(Resource *resource, uint32_t id, wl_resource *seat) override;
};

DataDeviceManagerInterfacePrivate::DataDeviceManagerInterfacePrivate(DataDeviceManagerInterface *q, Display *d)
    : QtWaylandServer::wl_data_device_manager(*d, s_version)
    , q(q)
{
}

void DataDeviceManagerInterfacePrivate::data_device_manager_create_data_source(Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &wl_data_source_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataSourceInterface *dataSource = new DataSourceInterface(data_source_resource);
    Q_EMIT q->dataSourceCreated(dataSource);
}

void DataDeviceManagerInterfacePrivate::data_device_manager_get_data_device(Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    if (!s) {
        return;
    }

    wl_resource *data_device_resource = wl_resource_create(resource->client(), &wl_data_device_interface, resource->version(), id);
    if (!data_device_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataDeviceInterface *dataDevice = new DataDeviceInterface(s, data_device_resource);
    Q_EMIT q->dataDeviceCreated(dataDevice);
}

DataDeviceManagerInterface::DataDeviceManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new DataDeviceManagerInterfacePrivate(this, display))
{
}

DataDeviceManagerInterface::~DataDeviceManagerInterface() = default;

}
