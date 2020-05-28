/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "datacontroldevicemanager_interface.h"
#include "datacontroldevice_interface.h"
#include "datacontrolsource_interface.h"
#include "global_p.h"
#include "display.h"
#include "seat_interface_p.h"
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>

static const int s_version = 1;
namespace KWaylandServer
{

class DataControlDeviceManagerInterfacePrivate : public QtWaylandServer::zwlr_data_control_manager_v1
{
public:
    DataControlDeviceManagerInterfacePrivate(DataControlDeviceManagerInterface *q, Display *d);

    DataControlDeviceManagerInterface *q;

protected:
    void zwlr_data_control_manager_v1_create_data_source(Resource *resource, uint32_t id) override;
    void zwlr_data_control_manager_v1_get_data_device(Resource *resource, uint32_t id, wl_resource *seat) override;
    void zwlr_data_control_manager_v1_destroy(Resource *resource) override;
};

DataControlDeviceManagerInterfacePrivate::DataControlDeviceManagerInterfacePrivate(DataControlDeviceManagerInterface *q, Display *d)
    : QtWaylandServer::zwlr_data_control_manager_v1(*d, s_version)
    , q(q)
{
}

void DataControlDeviceManagerInterfacePrivate::zwlr_data_control_manager_v1_create_data_source(Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &zwlr_data_control_source_v1_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataControlSourceInterface *dataSource = new DataControlSourceInterface(q, data_source_resource);
    emit q->dataSourceCreated(dataSource);
}

void DataControlDeviceManagerInterfacePrivate::zwlr_data_control_manager_v1_get_data_device(Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    if (!s) {
        return;
    }

    wl_resource *data_device_resource = wl_resource_create(resource->client(), &zwlr_data_control_device_v1_interface, resource->version(), id);
    if (!data_device_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataControlDeviceInterface *dataDevice = new DataControlDeviceInterface(s, data_device_resource);
    emit q->dataDeviceCreated(dataDevice);
}

void DataControlDeviceManagerInterfacePrivate::zwlr_data_control_manager_v1_destroy(QtWaylandServer::zwlr_data_control_manager_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlDeviceManagerInterface::DataControlDeviceManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new DataControlDeviceManagerInterfacePrivate(this, display))
{

}

DataControlDeviceManagerInterface::~DataControlDeviceManagerInterface() = default;

}
