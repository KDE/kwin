/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "primaryselectionoffer_v1_interface.h"
#include "primaryselectiondevice_v1_interface.h"
#include "primaryselectionsource_v1_interface.h"
// Qt
#include <QPointer>
#include <QStringList>
// Wayland
#include <qwayland-server-wp-primary-selection-unstable-v1.h>
// system
#include <unistd.h>

namespace KWaylandServer
{
class PrimarySelectionOfferV1InterfacePrivate : public QtWaylandServer::zwp_primary_selection_offer_v1
{
public:
    PrimarySelectionOfferV1InterfacePrivate(PrimarySelectionOfferV1Interface *q, AbstractDataSource *source, wl_resource *resource);
    PrimarySelectionOfferV1Interface *q;
    QPointer<AbstractDataSource> source;

protected:
    void zwp_primary_selection_offer_v1_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void zwp_primary_selection_offer_v1_destroy(Resource *resource) override;
    void zwp_primary_selection_offer_v1_destroy_resource(Resource *resource) override;
};

PrimarySelectionOfferV1InterfacePrivate::PrimarySelectionOfferV1InterfacePrivate(PrimarySelectionOfferV1Interface *_q,
                                                                                 AbstractDataSource *source,
                                                                                 wl_resource *resource)
    : QtWaylandServer::zwp_primary_selection_offer_v1(resource)
    , q(_q)
    , source(source)
{
}

void PrimarySelectionOfferV1InterfacePrivate::zwp_primary_selection_offer_v1_destroy(QtWaylandServer::zwp_primary_selection_offer_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PrimarySelectionOfferV1InterfacePrivate::zwp_primary_selection_offer_v1_destroy_resource(
    QtWaylandServer::zwp_primary_selection_offer_v1::Resource *resource)
{
    delete q;
}

void PrimarySelectionOfferV1InterfacePrivate::zwp_primary_selection_offer_v1_receive(Resource *resource, const QString &mimeType, qint32 fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mimeType, fd);
}

PrimarySelectionOfferV1Interface::PrimarySelectionOfferV1Interface(AbstractDataSource *source, wl_resource *resource)
    : QObject()
    , d(new PrimarySelectionOfferV1InterfacePrivate(this, source, resource))
{
    Q_ASSERT(source);
    connect(source, &AbstractDataSource::mimeTypeOffered, this, [this](const QString &mimeType) {
        d->send_offer(mimeType);
    });
}

PrimarySelectionOfferV1Interface::~PrimarySelectionOfferV1Interface() = default;

void PrimarySelectionOfferV1Interface::sendAllOffers()
{
    for (const QString &mimeType : d->source->mimeTypes()) {
        d->send_offer(mimeType);
    }
}

wl_resource *PrimarySelectionOfferV1Interface::resource() const
{
    return d->resource()->handle;
}

}
