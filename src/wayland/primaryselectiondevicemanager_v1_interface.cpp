/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "primaryselectiondevicemanager_v1_interface.h"
#include "primaryselectiondevice_v1_interface.h"
#include "primaryselectionsource_v1_interface.h"
#include "display.h"
#include "seat_interface_p.h"
// Wayland
#include <qwayland-server-wp-primary-selection-unstable-v1.h>

static const int s_version = 1;
namespace KWaylandServer
{

class PrimarySelectionDeviceManagerV1InterfacePrivate : public QtWaylandServer::zwp_primary_selection_device_manager_v1
{
public:
    PrimarySelectionDeviceManagerV1InterfacePrivate(PrimarySelectionDeviceManagerV1Interface *q, Display *d);

    PrimarySelectionDeviceManagerV1Interface *q;
protected:
    void zwp_primary_selection_device_manager_v1_create_source(Resource *resource, uint32_t id) override;
    void zwp_primary_selection_device_manager_v1_get_device(Resource *resource, uint32_t id, wl_resource *seat) override;
    void zwp_primary_selection_device_manager_v1_destroy(Resource *resource) override;
};

PrimarySelectionDeviceManagerV1InterfacePrivate::PrimarySelectionDeviceManagerV1InterfacePrivate(PrimarySelectionDeviceManagerV1Interface *q, Display *d)
    : QtWaylandServer::zwp_primary_selection_device_manager_v1(*d, s_version)
    , q(q)
{
}

void PrimarySelectionDeviceManagerV1InterfacePrivate::zwp_primary_selection_device_manager_v1_create_source(Resource *resource, uint32_t id)
{
    wl_resource *data_source_resource = wl_resource_create(resource->client(), &zwp_primary_selection_source_v1_interface, resource->version(), id);
    if (!data_source_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    PrimarySelectionSourceV1Interface *dataSource = new PrimarySelectionSourceV1Interface(q, data_source_resource);
    emit q->dataSourceCreated(dataSource);
}

void PrimarySelectionDeviceManagerV1InterfacePrivate::zwp_primary_selection_device_manager_v1_get_device(Resource *resource, uint32_t id, wl_resource *seat)
{
    SeatInterface *s = SeatInterface::get(seat);
    Q_ASSERT(s);
    if (!s) {
        return;
    }

    wl_resource *data_device_resource = wl_resource_create(resource->client(), &zwp_primary_selection_device_v1_interface, resource->version(), id);
    if (!data_device_resource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }
    PrimarySelectionDeviceV1Interface *dataDevice = new PrimarySelectionDeviceV1Interface(s, data_device_resource);
    emit q->dataDeviceCreated(dataDevice);
}

void PrimarySelectionDeviceManagerV1InterfacePrivate::zwp_primary_selection_device_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PrimarySelectionDeviceManagerV1Interface::PrimarySelectionDeviceManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PrimarySelectionDeviceManagerV1InterfacePrivate(this, display))
{
}

PrimarySelectionDeviceManagerV1Interface::~PrimarySelectionDeviceManagerV1Interface() = default;
}
