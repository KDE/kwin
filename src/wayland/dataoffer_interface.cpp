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
#include "resource_p.h"
// Qt
#include <QStringList>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataOfferInterface::Private : public Resource::Private
{
public:
    Private(DataSourceInterface *source, DataDeviceInterface *parentInterface, DataOfferInterface *q, wl_resource *parentResource);
    ~Private();
    DataSourceInterface *source;
    DataDeviceInterface *dataDevice;

private:
    DataOfferInterface *q_func() {
        return reinterpret_cast<DataOfferInterface *>(q);
    }
    void receive(const QString &mimeType, qint32 fd);
    static void acceptCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *mimeType);
    static void receiveCallback(wl_client *client, wl_resource *resource, const char *mimeType, int32_t fd);

    static const struct wl_data_offer_interface s_interface;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_data_offer_interface DataOfferInterface::Private::s_interface = {
    acceptCallback,
    receiveCallback,
    resourceDestroyedCallback
};
#endif

DataOfferInterface::Private::Private(DataSourceInterface *source, DataDeviceInterface *parentInterface, DataOfferInterface *q, wl_resource *parentResource)
    : Resource::Private(q, nullptr, parentResource, &wl_data_offer_interface, &s_interface)
    , source(source)
    , dataDevice(parentInterface)
{
    // TODO: connect to new selections
}

DataOfferInterface::Private::~Private() = default;

void DataOfferInterface::Private::acceptCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *mimeType)
{
    Q_UNUSED(client)
    Q_UNUSED(serial)
    auto p = cast<Private>(resource);
    if (!p->source) {
        return;
    }
    p->source->accept(mimeType ? QString::fromUtf8(mimeType) : QString());
}

void DataOfferInterface::Private::receiveCallback(wl_client *client, wl_resource *resource, const char *mimeType, int32_t fd)
{
    Q_UNUSED(client)
    cast<Private>(resource)->receive(QString::fromUtf8(mimeType), fd);
}

void DataOfferInterface::Private::receive(const QString &mimeType, qint32 fd)
{
    source->requestData(mimeType, fd);
}

DataOfferInterface::DataOfferInterface(DataSourceInterface *source, DataDeviceInterface *parentInterface, wl_resource *parentResource)
    : Resource(new Private(source, parentInterface, this, parentResource))
{
    Q_ASSERT(source);
    connect(source, &DataSourceInterface::mimeTypeOffered, this,
        [this](const QString &mimeType) {
            Q_D();
            if (!d->resource) {
                return;
            }
            wl_data_offer_send_offer(d->resource, mimeType.toUtf8().constData());
        }
    );
    QObject::connect(source, &QObject::destroyed, this,
        [this] {
            Q_D();
            d->source = nullptr;
        }
    );
}

DataOfferInterface::~DataOfferInterface() = default;

void DataOfferInterface::sendAllOffers()
{
    Q_D();
    for (const QString &mimeType : d->source->mimeTypes()) {
        wl_data_offer_send_offer(d->resource, mimeType.toUtf8().constData());
    }
}

DataOfferInterface::Private *DataOfferInterface::d_func() const
{
    return reinterpret_cast<DataOfferInterface::Private*>(d.data());
}

}
}
