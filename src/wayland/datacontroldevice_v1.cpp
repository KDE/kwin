/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroldevice_v1.h"
#include "datacontroldevicemanager_v1.h"
#include "datacontroloffer_v1.h"
#include "datacontrolsource_v1.h"
#include "display.h"
#include "seat.h"
#include "seat_p.h"
#include "surface.h"

// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>

namespace KWin
{
class DataControlDeviceV1InterfacePrivate : public QtWaylandServer::zwlr_data_control_device_v1
{
public:
    DataControlDeviceV1InterfacePrivate(DataControlDeviceV1Interface *q, SeatInterface *seat, wl_resource *resource);

    DataControlOfferV1Interface *createDataOffer(AbstractDataSource *source);

    DataControlDeviceV1Interface *q;
    QPointer<SeatInterface> seat;
    QPointer<DataControlSourceV1Interface> selection;
    QPointer<DataControlSourceV1Interface> primarySelection;

protected:
    void zwlr_data_control_device_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source) override;
    void zwlr_data_control_device_v1_set_primary_selection(Resource *resource, struct ::wl_resource *source) override;
    void zwlr_data_control_device_v1_destroy(Resource *resource) override;
};

DataControlDeviceV1InterfacePrivate::DataControlDeviceV1InterfacePrivate(DataControlDeviceV1Interface *_q, SeatInterface *seat, wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_device_v1(resource)
    , q(_q)
    , seat(seat)
{
}

void DataControlDeviceV1InterfacePrivate::zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source)
{
    DataControlSourceV1Interface *dataSource = nullptr;

    if (source) {
        dataSource = DataControlSourceV1Interface::get(source);
        Q_ASSERT(dataSource);
        if (dataSource == seat->selection() || dataSource == seat->primarySelection()) {
            wl_resource_post_error(resource->handle, error::error_used_source, "source given to set_selection was already used before");
            return;
        }
    }
    if (selection) {
        selection->cancel();
    }
    selection = dataSource;
    Q_EMIT q->selectionChanged(selection);
}

void DataControlDeviceV1InterfacePrivate::zwlr_data_control_device_v1_set_primary_selection(Resource *resource, wl_resource *source)
{
    DataControlSourceV1Interface *dataSource = nullptr;

    if (source) {
        dataSource = DataControlSourceV1Interface::get(source);
        Q_ASSERT(dataSource);
        if (dataSource == seat->selection() || dataSource == seat->primarySelection()) {
            wl_resource_post_error(resource->handle, error::error_used_source, "source given to set_primary_selection was already used before");
            return;
        }
    }
    if (primarySelection) {
        primarySelection->cancel();
    }
    primarySelection = dataSource;
    Q_EMIT q->primarySelectionChanged(primarySelection);
}

void DataControlDeviceV1InterfacePrivate::zwlr_data_control_device_v1_destroy(QtWaylandServer::zwlr_data_control_device_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlOfferV1Interface *DataControlDeviceV1InterfacePrivate::createDataOffer(AbstractDataSource *source)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(resource()->client(), &zwlr_data_control_offer_v1_interface, resource()->version(), 0);
    if (!data_offer_resource) {
        return nullptr;
    }

    DataControlOfferV1Interface *offer = new DataControlOfferV1Interface(source, data_offer_resource);
    send_data_offer(offer->resource());
    offer->sendAllOffers();
    return offer;
}

void DataControlDeviceV1InterfacePrivate::zwlr_data_control_device_v1_destroy_resource(QtWaylandServer::zwlr_data_control_device_v1::Resource *resource)
{
    delete q;
}

DataControlDeviceV1Interface::DataControlDeviceV1Interface(SeatInterface *seat, wl_resource *resource)
    : QObject()
    , d(new DataControlDeviceV1InterfacePrivate(this, seat, resource))
{
    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(seat);
    seatPrivate->registerDataControlDevice(this);
}

DataControlDeviceV1Interface::~DataControlDeviceV1Interface() = default;

SeatInterface *DataControlDeviceV1Interface::seat() const
{
    return d->seat;
}

DataControlSourceV1Interface *DataControlDeviceV1Interface::selection() const
{
    return d->selection;
}

DataControlSourceV1Interface *DataControlDeviceV1Interface::primarySelection() const
{
    return d->primarySelection;
}

void DataControlDeviceV1Interface::sendSelection(AbstractDataSource *other)
{
    DataControlOfferV1Interface *offer = d->createDataOffer(other);
    d->send_selection(offer ? offer->resource() : nullptr);
}

void DataControlDeviceV1Interface::sendPrimarySelection(AbstractDataSource *other)
{
    if (d->resource()->version() >= ZWLR_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION_SINCE_VERSION) {
        DataControlOfferV1Interface *offer = d->createDataOffer(other);
        d->send_primary_selection(offer ? offer->resource() : nullptr);
    }
}
}

#include "moc_datacontroldevice_v1.cpp"
