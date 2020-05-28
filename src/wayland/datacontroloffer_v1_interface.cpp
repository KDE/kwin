/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroloffer_v1_interface.h"
#include "datacontroldevice_v1_interface.h"
#include "datacontrolsource_v1_interface.h"
// Qt
#include <QStringList>
#include <QPointer>
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>
// system
#include <unistd.h>

namespace KWaylandServer
{

class DataControlOfferV1InterfacePrivate : public QtWaylandServer::zwlr_data_control_offer_v1
{
public:
    DataControlOfferV1InterfacePrivate(DataControlOfferV1Interface *q, AbstractDataSource *source, wl_resource *resource);

    DataControlOfferV1Interface *q;
    QPointer<AbstractDataSource> source;

protected:
    void zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_offer_v1_destroy(Resource *resource) override;
    void zwlr_data_control_offer_v1_destroy_resource(Resource *resource) override;
};

DataControlOfferV1InterfacePrivate::DataControlOfferV1InterfacePrivate(DataControlOfferV1Interface *_q, AbstractDataSource *source, wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_offer_v1(resource)
    , q(_q)
    , source(source)
{
}

void DataControlOfferV1InterfacePrivate::zwlr_data_control_offer_v1_destroy(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataControlOfferV1InterfacePrivate::zwlr_data_control_offer_v1_destroy_resource(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void DataControlOfferV1InterfacePrivate::zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mimeType, qint32 fd)
{
    Q_UNUSED(resource)
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

DataControlOfferV1Interface::DataControlOfferV1Interface(AbstractDataSource *source, wl_resource *resource)
    : QObject()
    , d(new DataControlOfferV1InterfacePrivate(this, source, resource))
{
    Q_ASSERT(source);
    connect(source, &AbstractDataSource::mimeTypeOffered, this,
        [this](const QString &mimeType) {
            d->send_offer(mimeType);
        }
    );
}

DataControlOfferV1Interface::~DataControlOfferV1Interface() = default;

void DataControlOfferV1Interface::sendAllOffers()
{
    Q_ASSERT(d->source);
    for (const QString &mimeType : d->source->mimeTypes()) {
        d->send_offer(mimeType);
    }
}

wl_resource *DataControlOfferV1Interface::resource() const
{
    return d->resource()->handle;
}

}
