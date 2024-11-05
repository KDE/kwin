/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontroloffer_v1.h"
#include "datacontroldevice_v1.h"
#include "datacontroloffer_v1_p.h"
#include "datacontrolsource_v1.h"
// Qt
#include <QPointer>
#include <QStringList>
// Wayland
#include <qwayland-server-ext-data-control-v1.h>
// system
#include <unistd.h>

namespace KWin
{

DataControlOfferV1InterfacePrivate::DataControlOfferV1InterfacePrivate(AbstractDataSource *source)
    : source(source)
{
}

ExtDataControlOfferPrivate::ExtDataControlOfferPrivate(AbstractDataSource *source, wl_resource *resource)
    : DataControlOfferV1InterfacePrivate(source)
    , QtWaylandServer::ext_data_control_offer_v1(resource)
{
}

void ExtDataControlOfferPrivate::ext_data_control_offer_v1_destroy(QtWaylandServer::ext_data_control_offer_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ExtDataControlOfferPrivate::ext_data_control_offer_v1_destroy_resource(QtWaylandServer::ext_data_control_offer_v1::Resource *resource)
{
    delete q;
}

void ExtDataControlOfferPrivate::ext_data_control_offer_v1_receive(Resource *resource, const QString &mimeType, qint32 fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

WlrDataControlOfferPrivate::WlrDataControlOfferPrivate(AbstractDataSource *source, wl_resource *resource)
    : DataControlOfferV1InterfacePrivate(source)
    , QtWaylandServer::zwlr_data_control_offer_v1(resource)
{
}

void WlrDataControlOfferPrivate::zwlr_data_control_offer_v1_destroy(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void WlrDataControlOfferPrivate::zwlr_data_control_offer_v1_destroy_resource(QtWaylandServer::zwlr_data_control_offer_v1::Resource *resource)
{
    delete q;
}

void WlrDataControlOfferPrivate::zwlr_data_control_offer_v1_receive(Resource *resource, const QString &mimeType, qint32 fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

DataControlOfferV1Interface::DataControlOfferV1Interface(std::unique_ptr<DataControlOfferV1InterfacePrivate> &&dd)
    : QObject()
    , d(std::move(dd))
{
    d->q = this;
    connect(d->source, &AbstractDataSource::mimeTypeOffered, this, [this](const QString &mimeType) {
        this->d->sendOffer(mimeType);
    });
}

DataControlOfferV1Interface::~DataControlOfferV1Interface() = default;

void DataControlOfferV1Interface::sendAllOffers()
{
    Q_ASSERT(d->source);
    for (const QString &mimeType : d->source->mimeTypes()) {
        d->sendOffer(mimeType);
    }
}
}

#include "moc_datacontroloffer_v1.cpp"
