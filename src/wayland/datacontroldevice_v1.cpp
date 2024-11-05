/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroldevice_v1.h"
#include "datacontroldevice_v1_p.h"
#include "datacontroldevicemanager_v1.h"
#include "datacontroloffer_v1.h"
#include "datacontroloffer_v1_p.h"
#include "datacontrolsource_v1.h"
#include "display.h"
#include "seat.h"
#include "seat_p.h"
#include "surface.h"

// Wayland
#include <qwayland-server-ext-data-control-v1.h>

namespace KWin
{

template<typename T, typename D>
DataControlOfferV1Interface *createDataOffer(AbstractDataSource *source, D *device)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(device->resource()->client(), T::interface(), device->resource()->version(), 0);
    auto offer = new DataControlOfferV1Interface(std::make_unique<T>(source, data_offer_resource));
    device->send_data_offer(data_offer_resource);
    offer->sendAllOffers();
    return offer;
}

DataControlDeviceV1InterfacePrivate::DataControlDeviceV1InterfacePrivate(SeatInterface *seat)
    : seat(seat)
{
}

ExtDataControlDevicePrivate::ExtDataControlDevicePrivate(SeatInterface *seat, wl_resource *resource)
    : DataControlDeviceV1InterfacePrivate(seat)
    , QtWaylandServer::ext_data_control_device_v1(resource)
{
}

void ExtDataControlDevicePrivate::ext_data_control_device_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtDataControlDevicePrivate::ext_data_control_device_v1_destroy_resource(QtWaylandServer::ext_data_control_device_v1::Resource *resource)
{
    delete q;
}

void ExtDataControlDevicePrivate::ext_data_control_device_v1_set_selection(Resource *resource, wl_resource *source)
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

void ExtDataControlDevicePrivate::ext_data_control_device_v1_set_primary_selection(Resource *resource, wl_resource *source)
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

void ExtDataControlDevicePrivate::sendSelection(AbstractDataSource *source)
{
    auto offer = createDataOffer<ExtDataControlOfferPrivate>(source, this);
    send_selection(offer ? static_cast<ExtDataControlOfferPrivate *>(offer->d.get())->resource()->handle : nullptr);
}

void ExtDataControlDevicePrivate::sendPrimarySelection(AbstractDataSource *source)
{
    auto offer = createDataOffer<ExtDataControlOfferPrivate>(source, this);
    send_primary_selection(offer ? static_cast<ExtDataControlOfferPrivate *>(offer->d.get())->resource()->handle : nullptr);
}

WlrDataControlDevicePrivate::WlrDataControlDevicePrivate(SeatInterface *seat, wl_resource *resource)
    : DataControlDeviceV1InterfacePrivate(seat)
    , QtWaylandServer::zwlr_data_control_device_v1(resource)
{
}

void WlrDataControlDevicePrivate::zwlr_data_control_device_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void WlrDataControlDevicePrivate::zwlr_data_control_device_v1_destroy_resource(QtWaylandServer::zwlr_data_control_device_v1::Resource *resource)
{
    delete q;
}

void WlrDataControlDevicePrivate::zwlr_data_control_device_v1_set_selection(Resource *resource, wl_resource *source)
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

void WlrDataControlDevicePrivate::zwlr_data_control_device_v1_set_primary_selection(Resource *resource, wl_resource *source)
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

void WlrDataControlDevicePrivate::sendSelection(AbstractDataSource *source)
{
    auto offer = createDataOffer<WlrDataControlOfferPrivate>(source, this);
    send_selection(offer ? static_cast<WlrDataControlOfferPrivate *>(offer->d.get())->resource()->handle : nullptr);
}

void WlrDataControlDevicePrivate::sendPrimarySelection(AbstractDataSource *source)
{
    if (resource()->version() < ZWLR_DATA_CONTROL_DEVICE_V1_PRIMARY_SELECTION_SINCE_VERSION) {
        return;
    }
    auto offer = createDataOffer<WlrDataControlOfferPrivate>(source, this);
    send_primary_selection(offer ? static_cast<WlrDataControlOfferPrivate *>(offer->d.get())->resource()->handle : nullptr);
}

DataControlDeviceV1Interface::DataControlDeviceV1Interface(std::unique_ptr<DataControlDeviceV1InterfacePrivate> &&dd)
    : QObject()
    , d(std::move(dd))
{
    d->q = this;
    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(d->seat);
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
    d->sendSelection(other);
}

void DataControlDeviceV1Interface::sendPrimarySelection(AbstractDataSource *other)
{
    d->sendPrimarySelection(other);
}
}

#include "moc_datacontroldevice_v1.cpp"
