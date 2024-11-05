/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "datacontroldevicemanager_v1.h"
#include "datacontroldevice_v1.h"
#include "datacontroldevice_v1_p.h"
#include "datacontrolsource_v1.h"
#include "datacontrolsource_v1_p.h"
#include "display.h"
#include "seat_p.h"
// Wayland
#include <qwayland-server-ext-data-control-v1.h>
#include <qwayland-server-wlr-data-control-unstable-v1.h>

static const int s_version = 1;
static const int s_wlr_data_control_version = 2;
namespace KWin
{

class DataControlDeviceManagerV1InterfacePrivate : public QtWaylandServer::ext_data_control_manager_v1, public QtWaylandServer::zwlr_data_control_manager_v1
{
public:
    DataControlDeviceManagerV1InterfacePrivate(DataControlDeviceManagerV1Interface *q, Display *d);

    DataControlDeviceManagerV1Interface *q;

protected:
    void ext_data_control_manager_v1_create_data_source(ext_data_control_manager_v1::Resource *resource, uint32_t id) override;
    void ext_data_control_manager_v1_get_data_device(ext_data_control_manager_v1::Resource *resource, uint32_t id, wl_resource *seat) override;
    void ext_data_control_manager_v1_destroy(ext_data_control_manager_v1::Resource *resource) override;
    void zwlr_data_control_manager_v1_create_data_source(zwlr_data_control_manager_v1::Resource *resource, uint32_t id) override;
    void zwlr_data_control_manager_v1_get_data_device(zwlr_data_control_manager_v1::Resource *resource, uint32_t id, struct ::wl_resource *seat) override;
    void zwlr_data_control_manager_v1_destroy(zwlr_data_control_manager_v1::Resource *resource) override;
};

DataControlDeviceManagerV1InterfacePrivate::DataControlDeviceManagerV1InterfacePrivate(DataControlDeviceManagerV1Interface *q, Display *d)
    : QtWaylandServer::ext_data_control_manager_v1(*d, s_version)
    , QtWaylandServer::zwlr_data_control_manager_v1(*d, s_wlr_data_control_version)
    , q(q)
{
}

void DataControlDeviceManagerV1InterfacePrivate::ext_data_control_manager_v1_create_data_source(ext_data_control_manager_v1::Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &ext_data_control_source_v1_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataControlSourceV1Interface *dataSource = new DataControlSourceV1Interface(q, std::make_unique<ExtDataControlSourcePrivate>(data_source_resource));
    Q_EMIT q->dataSourceCreated(dataSource);
}

void DataControlDeviceManagerV1InterfacePrivate::zwlr_data_control_manager_v1_create_data_source(zwlr_data_control_manager_v1::Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &zwlr_data_control_source_v1_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    DataControlSourceV1Interface *dataSource = new DataControlSourceV1Interface(q, std::make_unique<WlrDataControlSourcePrivate>(data_source_resource));
    Q_EMIT q->dataSourceCreated(dataSource);
}

void DataControlDeviceManagerV1InterfacePrivate::ext_data_control_manager_v1_get_data_device(ext_data_control_manager_v1::Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    if (!s) {
        return;
    }

    wl_resource *data_device_resource = wl_resource_create(resource->client(), &ext_data_control_device_v1_interface, resource->version(), id);
    if (!data_device_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    auto dataDevice = new DataControlDeviceV1Interface(std::make_unique<ExtDataControlDevicePrivate>(s, data_device_resource));

    Q_EMIT q->dataDeviceCreated(dataDevice);
}

void DataControlDeviceManagerV1InterfacePrivate::zwlr_data_control_manager_v1_get_data_device(zwlr_data_control_manager_v1::Resource *resource, uint32_t id, wl_resource *seat)
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
    auto dataDevice = new DataControlDeviceV1Interface(std::make_unique<WlrDataControlDevicePrivate>(s, data_device_resource));

    Q_EMIT q->dataDeviceCreated(dataDevice);
}

void DataControlDeviceManagerV1InterfacePrivate::ext_data_control_manager_v1_destroy(QtWaylandServer::ext_data_control_manager_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
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

DataControlDeviceManagerV1Interface::~DataControlDeviceManagerV1Interface()
{
}
}

#include "moc_datacontroldevicemanager_v1.cpp"
