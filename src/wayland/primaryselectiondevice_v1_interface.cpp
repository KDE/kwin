/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "primaryselectiondevice_v1_interface.h"
#include "primaryselectiondevicemanager_v1_interface.h"
#include "primaryselectionoffer_v1_interface.h"
#include "primaryselectionsource_v1_interface.h"
#include "display.h"
#include "seat_interface.h"
#include "seat_interface_p.h"

// Wayland
#include <qwayland-server-wp-primary-selection-unstable-v1.h>

namespace KWaylandServer
{

class PrimarySelectionDeviceV1InterfacePrivate: public QtWaylandServer::zwp_primary_selection_device_v1
{
public:
    PrimarySelectionDeviceV1InterfacePrivate(PrimarySelectionDeviceV1Interface *q, SeatInterface *seat, wl_resource *resource);

    PrimarySelectionOfferV1Interface *createDataOffer(AbstractDataSource *source);

    PrimarySelectionDeviceV1Interface *q;
    SeatInterface *seat;
    QPointer<PrimarySelectionSourceV1Interface> selection;

private:
    void setSelection(PrimarySelectionSourceV1Interface *dataSource);

protected:
    void zwp_primary_selection_device_v1_destroy_resource(Resource *resource) override;
    void zwp_primary_selection_device_v1_set_selection(Resource *resource, wl_resource *source, uint32_t serial) override;
    void zwp_primary_selection_device_v1_destroy(Resource *resource) override;
};


PrimarySelectionDeviceV1InterfacePrivate::PrimarySelectionDeviceV1InterfacePrivate(PrimarySelectionDeviceV1Interface *_q, SeatInterface *seat, wl_resource *resource)
    : QtWaylandServer::zwp_primary_selection_device_v1(resource)
    ,  q(_q)
    , seat(seat)
{
}

void PrimarySelectionDeviceV1InterfacePrivate::zwp_primary_selection_device_v1_set_selection(Resource *resource, wl_resource *source, uint32_t serial)
{
    Q_UNUSED(resource)
    Q_UNUSED(serial)
    PrimarySelectionSourceV1Interface *dataSource = nullptr;

    if (source) {
      dataSource = PrimarySelectionSourceV1Interface::get(source);
      Q_ASSERT(dataSource);
    }

    if (selection == dataSource) {
        return;
    }
    if (selection) {
        selection->cancel();
    }
    selection = dataSource;
    if (selection) {
        emit q->selectionChanged(selection);
    }
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

void PrimarySelectionDeviceV1InterfacePrivate::zwp_primary_selection_device_v1_destroy_resource(QtWaylandServer::zwp_primary_selection_device_v1::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

PrimarySelectionDeviceV1Interface::PrimarySelectionDeviceV1Interface(SeatInterface *seat, wl_resource *resource)
    : QObject()
    , d(new PrimarySelectionDeviceV1InterfacePrivate(this, seat, resource))
{
    seat->d_func()->registerPrimarySelectionDevice(this);
}

PrimarySelectionDeviceV1Interface::~PrimarySelectionDeviceV1Interface() = default;

SeatInterface *PrimarySelectionDeviceV1Interface::seat() const
{
    return d->seat;
}

PrimarySelectionSourceV1Interface *PrimarySelectionDeviceV1Interface::selection() const
{
    return d->selection;
}

void PrimarySelectionDeviceV1Interface::sendSelection(AbstractDataSource *other)
{
    if (!other) {
        sendClearSelection();
        return;
    }
    PrimarySelectionOfferV1Interface *offer = d->createDataOffer(other);
    if (!offer) {
        return;
    }
    d->send_selection(offer->resource());
}

void PrimarySelectionDeviceV1Interface::sendClearSelection()
{
    d->send_selection(nullptr);
}

wl_client *PrimarySelectionDeviceV1Interface::client() const
{
    return d->resource()->client();
}

}
