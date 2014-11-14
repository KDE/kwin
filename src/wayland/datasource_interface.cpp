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
#include "datasource_interface.h"
#include "datadevicemanager_interface.h"
#include "resource_p.h"
// Qt
#include <QStringList>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataSourceInterface::Private : public Resource::Private
{
public:
    Private(DataSourceInterface *q, DataDeviceManagerInterface *parent);
    ~Private();
    void create(wl_client *client, quint32 version, quint32 id) override;

    static DataSourceInterface *get(wl_resource *r) {
        auto s = cast(r);
        return s ? s->q : nullptr;
    }

    QStringList mimeTypes;

private:
    void offer(const QString &mimeType);

    static void offerCallback(wl_client *client, wl_resource *resource, const char *mimeType);
    static void destroyCallack(wl_client *client, wl_resource *resource);
    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return r ? reinterpret_cast<Private*>(wl_resource_get_user_data(r)) : nullptr;
    }

    const static struct wl_data_source_interface s_interface;
    DataSourceInterface *q;
};

const struct wl_data_source_interface DataSourceInterface::Private::s_interface = {
    offerCallback,
    destroyCallack
};

DataSourceInterface::Private::Private(DataSourceInterface *q, DataDeviceManagerInterface *parent)
    : Resource::Private(parent)
    , q(q)
{
}

DataSourceInterface::Private::~Private() = default;

void DataSourceInterface::Private::unbind(wl_resource *resource)
{
    auto s = cast(resource);
    s->resource = nullptr;
    s->q->deleteLater();
}

void DataSourceInterface::Private::destroyCallack(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    cast(resource)->q->deleteLater();
}

void DataSourceInterface::Private::offerCallback(wl_client *client, wl_resource *resource, const char *mimeType)
{
    Q_UNUSED(client)
    cast(resource)->offer(QString::fromUtf8(mimeType));
}

void DataSourceInterface::Private::offer(const QString &mimeType)
{
    mimeTypes << mimeType;
    emit q->mimeTypeOffered(mimeType);
}

void DataSourceInterface::Private::create(wl_client *client, quint32 version, quint32 id)
{
    Q_ASSERT(!resource);
    resource = wl_resource_create(client, &wl_data_source_interface, version, id);
    if (!resource) {
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
}

DataSourceInterface::DataSourceInterface(DataDeviceManagerInterface *parent)
    : Resource(new Private(this, parent))
{
}

DataSourceInterface::~DataSourceInterface() = default;

void DataSourceInterface::accept(const QString &mimeType)
{
    Q_D();
    // TODO: does this require a sanity check on the possible mimeType?
    wl_data_source_send_target(d->resource, mimeType.isEmpty() ? nullptr : mimeType.toUtf8().constData());
}

void DataSourceInterface::requestData(const QString &mimeType, qint32 fd)
{
    Q_D();
    // TODO: does this require a sanity check on the possible mimeType?
    wl_data_source_send_send(d->resource, mimeType.toUtf8().constData(), int32_t(fd));
}

void DataSourceInterface::cancel()
{
    Q_D();
    wl_data_source_send_cancelled(d->resource);
}

QStringList DataSourceInterface::mimeTypes() const
{
    Q_D();
    return d->mimeTypes;
}

DataSourceInterface *DataSourceInterface::get(wl_resource *native)
{
    return Private::get(native);
}

DataSourceInterface::Private *DataSourceInterface::d_func() const
{
    return reinterpret_cast<DataSourceInterface::Private*>(d.data());
}

}
}
