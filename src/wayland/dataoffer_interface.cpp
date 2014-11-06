/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "dataoffer_interface.h"
#include "datadevice_interface.h"
#include "datasource_interface.h"
// Qt
#include <QStringList>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataOfferInterface::Private
{
public:
    Private(DataSourceInterface *source, DataDeviceInterface *parentInterface, DataOfferInterface *q);
    ~Private();
    void create(wl_client *client, quint32 version, quint32 id);
    wl_resource *offer = nullptr;
    DataSourceInterface *source;
    DataDeviceInterface *dataDevice;

private:
    void receive(const QString &mimeType, qint32 fd);
    static void acceptCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *mimeType);
    static void receiveCallback(wl_client *client, wl_resource *resource, const char *mimeType, int32_t fd);
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    DataOfferInterface *q;

    static const struct wl_data_offer_interface s_interface;
};

const struct wl_data_offer_interface DataOfferInterface::Private::s_interface = {
    acceptCallback,
    receiveCallback,
    destroyCallback
};

DataOfferInterface::Private::Private(DataSourceInterface *source, DataDeviceInterface *parentInterface, DataOfferInterface *q)
    : source(source)
    , dataDevice(parentInterface)
    , q(q)
{
    // TODO: connect to new selections
}

DataOfferInterface::Private::~Private()
{
    if (offer) {
        wl_resource_destroy(offer);
    }
}

void DataOfferInterface::Private::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!offer);
    offer = wl_resource_create(client, &wl_data_offer_interface, version, id);
    if (!offer) {
        return;
    }
    wl_resource_set_implementation(offer, &s_interface, this, unbind);
}

void DataOfferInterface::Private::acceptCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *mimeType)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(serial)
    Q_UNUSED(mimeType)
}

void DataOfferInterface::Private::destroyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->q->deleteLater();
}

void DataOfferInterface::Private::receiveCallback(wl_client *client, wl_resource *resource, const char *mimeType, int32_t fd)
{
    Q_UNUSED(client)
    cast(resource)->receive(QString::fromUtf8(mimeType), fd);
}

void DataOfferInterface::Private::receive(const QString &mimeType, qint32 fd)
{
    source->requestData(mimeType, fd);
}

void DataOfferInterface::Private::unbind(wl_resource *resource)
{
    auto o = cast(resource);
    o->offer = nullptr;
    o->q->deleteLater();
}

DataOfferInterface::DataOfferInterface(DataSourceInterface *source, DataDeviceInterface *parentInterface)
    : QObject(parentInterface)
    , d(new Private(source, parentInterface, this))
{
    connect(source, &DataSourceInterface::mimeTypeOffered, this,
        [this](const QString &mimeType) {
            if (!d->offer) {
                return;
            }
            wl_data_offer_send_offer(d->offer, mimeType.toUtf8().constData());
        }
    );
}

DataOfferInterface::~DataOfferInterface() = default;

void DataOfferInterface::create(wl_client *client, quint32 version, quint32 id)
{
    d->create(client, version, id);
}

void DataOfferInterface::sendAllOffers()
{
    for (const QString &mimeType : d->source->mimeTypes()) {
        wl_data_offer_send_offer(d->offer, mimeType.toUtf8().constData());
    }
}

wl_resource *DataOfferInterface::resource() const
{
    return d->offer;
}

}
}
