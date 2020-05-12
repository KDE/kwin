/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroloffer_interface.h"
#include "datacontroldevice_interface.h"
#include "datacontrolsource_interface.h"
// Qt
#include <QStringList>
#include <QPointer>
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>
// system
#include <unistd.h>

namespace KWaylandServer
{

class DataControlOfferInterfacePrivate : public QtWaylandServer::zwlr_data_control_offer_v1
{
public:
    DataControlOfferInterfacePrivate(DataControlOfferInterface *q, AbstractDataSource *source, wl_resource *resource);
    DataControlOfferInterface *q;
    QPointer<AbstractDataSource> source;

protected:
    void zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void zwlr_data_control_offer_v1_destroy(Resource *resource) override;
    void zwlr_data_control_offer_v1_destroy_resource(Resource *resource) override;
};

DataControlOfferInterfacePrivate::DataControlOfferInterfacePrivate(DataControlOfferInterface *_q, AbstractDataSource *source, wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_offer_v1(resource)
    , q(_q)
    , source(source)
{
}

void DataControlOfferInterfacePrivate::zwlr_data_control_offer_v1_destroy(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataControlOfferInterfacePrivate::zwlr_data_control_offer_v1_destroy_resource(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void DataControlOfferInterfacePrivate::zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mimeType, qint32 fd)
{
    Q_UNUSED(resource)
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

DataControlOfferInterface::DataControlOfferInterface(AbstractDataSource *source, wl_resource *resource)
    : QObject()
    , d(new DataControlOfferInterfacePrivate(this, source, resource))
{
    Q_ASSERT(source);
    connect(source, &AbstractDataSource::mimeTypeOffered, this,
        [this](const QString &mimeType) {
            d->send_offer(mimeType);
        }
    );
}

DataControlOfferInterface::~DataControlOfferInterface() = default;

void DataControlOfferInterface::sendAllOffers()
{
    Q_ASSERT(d->source);
    for (const QString &mimeType : d->source->mimeTypes()) {
        d->send_offer(mimeType);
    }
}

wl_resource *DataControlOfferInterface::resource() const
{
    return d->resource()->handle;
}

}
