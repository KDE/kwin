/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "datacontroldevicemanager_v1_interface.h"
#include "datacontroldevice_v1_interface.h"
#include "datacontrolsource_v1_interface.h"
#include "global_p.h"
#include "display.h"
#include "seat_interface_p.h"
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>

static const int s_version = 1;
namespace KWaylandServer
{

class DataControlDeviceManagerV1InterfacePrivate : public QtWaylandServer::zwlr_data_control_manager_v1
{
public:
    DataControlDeviceManagerV1InterfacePrivate(DataControlDeviceManagerV1Interface *q, Display *d);

    DataControlDeviceManagerV1Interface *q;

protected:
    void zwlr_data_control_manager_v1_create_data_source(Resource *resource, uint32_t id) override;
    void zwlr_data_control_manager_v1_get_data_device(Resource *resource, uint32_t id, wl_resource *seat) override;
    void zwlr_data_control_manager_v1_destroy(Resource *resource) override;
};

DataControlDeviceManagerV1InterfacePrivate::DataControlDeviceManagerV1InterfacePrivate(DataControlDeviceManagerV1Interface *q, Display *d)
    : QtWaylandServer::zwlr_data_control_manager_v1(*d, s_version)
    , q(q)
{
}

void DataControlDeviceManagerV1InterfacePrivate::zwlr_data_control_manager_v1_create_data_source(Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &zwlr_data_control_source_v1_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataControlSourceV1Interface *dataSource = new DataControlSourceV1Interface(q, data_source_resource);
    emit q->dataSourceCreated(dataSource);
}

void DataControlDeviceManagerV1InterfacePrivate::zwlr_data_control_manager_v1_get_data_device(Resource *resource, uint32_t id, wl_resource *seat)
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
    DataControlDeviceV1Interface *dataDevice = new DataControlDeviceV1Interface(s, data_device_resource);
    emit q->dataDeviceCreated(dataDevice);
}

void DataControlDeviceManagerV1InterfacePrivate::zwlr_data_control_manager_v1_destroy(QtWaylandServer::zwlr_data_control_manager_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlDeviceManagerV1Interface::DataControlDeviceManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new DataControlDeviceManagerV1InterfacePrivate(this, display))
{

}

DataControlDeviceManagerV1Interface::~DataControlDeviceManagerV1Interface() = default;

}
