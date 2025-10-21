/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "primaryselectiondevice_v1.h"
#include "display.h"
#include "primaryselectiondevicemanager_v1.h"
#include "primaryselectionoffer_v1.h"
#include "primaryselectionsource_v1.h"
#include "seat.h"
#include "seat_p.h"

// Wayland
#include <qwayland-server-primary-selection-unstable-v1.h>

namespace KWin
{
class PrimarySelectionDeviceV1InterfacePrivate : public QtWaylandServer::zwp_primary_selection_device_v1
{
public:
    PrimarySelectionDeviceV1InterfacePrivate(PrimarySelectionDeviceV1Interface *q, SeatInterface *seat, wl_resource *resource);

    PrimarySelectionOfferV1Interface *createDataOffer(AbstractDataSource *source);

    PrimarySelectionDeviceV1Interface *q;
    SeatInterface *seat;

private:
    void setSelection(PrimarySelectionSourceV1Interface *dataSource);

protected:
    void zwp_primary_selection_device_v1_destroy_resource(Resource *resource) override;
    void zwp_primary_selection_device_v1_set_selection(Resource *resource, wl_resource *source, uint32_t serial) override;
    void zwp_primary_selection_device_v1_destroy(Resource *resource) override;
};

PrimarySelectionDeviceV1InterfacePrivate::PrimarySelectionDeviceV1InterfacePrivate(PrimarySelectionDeviceV1Interface *_q,
                                                                                   SeatInterface *seat,
                                                                                   wl_resource *resource)
    : QtWaylandServer::zwp_primary_selection_device_v1(resource)
    , q(_q)
    , seat(seat)
{
}

void PrimarySelectionDeviceV1InterfacePrivate::zwp_primary_selection_device_v1_set_selection(Resource *resource, wl_resource *source, uint32_t serial)
{
    PrimarySelectionSourceV1Interface *dataSource = nullptr;

    if (source) {
        dataSource = PrimarySelectionSourceV1Interface::get(source);
        Q_ASSERT(dataSource);
    }

    Q_EMIT q->selectionChanged(dataSource, serial);
}

void PrimarySelectionDeviceV1InterfacePrivate::zwp_primary_selection_device_v1_destroy(QtWaylandServer::zwp_primary_selection_device_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PrimarySelectionOfferV1Interface *PrimarySelectionDeviceV1InterfacePrivate::createDataOffer(AbstractDataSource *source)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(resource()->client(), &zwp_primary_selection_offer_v1_interface, resource()->version(), 0);
    if (!data_offer_resource) {
        wl_resource_post_no_memory(resource()->handle);
        return nullptr;
    }

    PrimarySelectionOfferV1Interface *offer = new PrimarySelectionOfferV1Interface(source, data_offer_resource);
    send_data_offer(offer->resource());
    offer->sendAllOffers();
    return offer;
}

void PrimarySelectionDeviceV1InterfacePrivate::zwp_primary_selection_device_v1_destroy_resource(
    QtWaylandServer::zwp_primary_selection_device_v1::Resource *resource)
{
    delete q;
}

PrimarySelectionDeviceV1Interface::PrimarySelectionDeviceV1Interface(SeatInterface *seat, wl_resource *resource)
    : QObject()
    , d(new PrimarySelectionDeviceV1InterfacePrivate(this, seat, resource))
{
    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(seat);
    seatPrivate->registerPrimarySelectionDevice(this);
}

PrimarySelectionDeviceV1Interface::~PrimarySelectionDeviceV1Interface() = default;

SeatInterface *PrimarySelectionDeviceV1Interface::seat() const
{
    return d->seat;
}

void PrimarySelectionDeviceV1Interface::sendSelection(AbstractDataSource *other)
{
    PrimarySelectionOfferV1Interface *offer = d->createDataOffer(other);
    d->send_selection(offer ? offer->resource() : nullptr);
}

wl_client *PrimarySelectionDeviceV1Interface::client() const
{
    return d->resource()->client();
}

}

#include "moc_primaryselectiondevice_v1.cpp"
