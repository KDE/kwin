/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroldevice_interface.h"
#include "datacontroldevicemanager_interface.h"
#include "datacontroloffer_interface.h"
#include "datacontrolsource_interface.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "seat_interface_p.h"

// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>

namespace KWaylandServer
{

class DataControlDeviceInterfacePrivate: public QtWaylandServer::zwlr_data_control_device_v1
{
public:
    DataControlDeviceInterfacePrivate(DataControlDeviceInterface *q, SeatInterface *seat, wl_resource *resource);

    DataControlOfferInterface *createDataOffer(AbstractDataSource *source);

    DataControlDeviceInterface *q;
    QPointer<SeatInterface> seat;
    QPointer<DataControlSourceInterface> selection;

protected:
    void zwlr_data_control_device_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source) override;
    void zwlr_data_control_device_v1_destroy(Resource *resource) override;
};


DataControlDeviceInterfacePrivate::DataControlDeviceInterfacePrivate(DataControlDeviceInterface *_q, SeatInterface *seat, wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_device_v1(resource)
    ,  q(_q)
    , seat(seat)
{
}

void DataControlDeviceInterfacePrivate::zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source)
{
    DataControlSourceInterface *dataSource = nullptr;

    if (source) {
        dataSource = DataControlSourceInterface::get(source);
         Q_ASSERT(dataSource);
         if (dataSource == seat->selection()) {
            wl_resource_post_error(resource->handle, error::error_used_source,
                                   "source given to set_selection was already used before");
            return;
        }
    }
    if (selection) {
        selection->cancel();
    }
    selection = dataSource;
    emit q->selectionChanged(selection);
}

void DataControlDeviceInterfacePrivate::zwlr_data_control_device_v1_destroy(QtWaylandServer::zwlr_data_control_device_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlOfferInterface *DataControlDeviceInterfacePrivate::createDataOffer(AbstractDataSource *source)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(resource()->client(), &zwlr_data_control_offer_v1_interface, resource()->version(), 0);
    if (!data_offer_resource) {
        return nullptr;
    }

    DataControlOfferInterface *offer = new DataControlOfferInterface(source, data_offer_resource);
    send_data_offer(offer->resource());
    offer->sendAllOffers();
    return offer;
}

void DataControlDeviceInterfacePrivate::zwlr_data_control_device_v1_destroy_resource(QtWaylandServer::zwlr_data_control_device_v1::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

DataControlDeviceInterface::DataControlDeviceInterface(SeatInterface *seat, wl_resource *resource)
    : QObject()
    , d(new DataControlDeviceInterfacePrivate(this, seat, resource))
{
    seat->d_func()->registerDataControlDevice(this);
}

DataControlDeviceInterface::~DataControlDeviceInterface() = default;

SeatInterface *DataControlDeviceInterface::seat() const
{
    return d->seat;
}

DataControlSourceInterface *DataControlDeviceInterface::selection() const
{
    return d->selection;
}

void DataControlDeviceInterface::sendSelection(AbstractDataSource *other)
{
    if (!other) {
        sendClearSelection();
        return;
    }
    auto r = d->createDataOffer(other);
    if (!r) {
        return;
    }
    d->send_selection(r->resource());
}

void DataControlDeviceInterface::sendClearSelection()
{
    d->send_selection(nullptr);
}

}
