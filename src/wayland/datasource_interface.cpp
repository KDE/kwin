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
// Qt
#include <QStringList>
// Wayland
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class DataSourceInterface::Private
{
public:
    Private(DataSourceInterface *q);
    ~Private();
    void create(wl_client *client, quint32 version, quint32 id);

    static DataSourceInterface *get(wl_resource *r) {
        auto s = cast(r);
        return s ? s->q : nullptr;
    }

    wl_resource *dataSource = nullptr;
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

DataSourceInterface::Private::Private(DataSourceInterface *q)
    : q(q)
{
}

DataSourceInterface::Private::~Private()
{
    if (dataSource) {
        wl_resource_destroy(dataSource);
    }
}

void DataSourceInterface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    auto s = cast(resource);
    s->dataSource = nullptr;
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
    Q_ASSERT(!dataSource);
    dataSource = wl_resource_create(client, &wl_data_source_interface, version, id);
    if (!dataSource) {
        return;
    }
    wl_resource_set_implementation(dataSource, &s_interface, this, unbind);
}

DataSourceInterface::DataSourceInterface(DataDeviceManagerInterface *parent)
    : QObject(/*parent*/)
    , d(new Private(this))
{
    Q_UNUSED(parent)
}

DataSourceInterface::~DataSourceInterface() = default;

void DataSourceInterface::create(wl_client *client, quint32 version, quint32 id)
{
    d->create(client, version, id);
}

void DataSourceInterface::accept(const QString &mimeType)
{
    // TODO: does this require a sanity check on the possible mimeType?
    wl_data_source_send_target(d->dataSource, mimeType.isEmpty() ? nullptr : mimeType.toUtf8().constData());
}

void DataSourceInterface::requestData(const QString &mimeType, qint32 fd)
{
    // TODO: does this require a sanity check on the possible mimeType?
    wl_data_source_send_send(d->dataSource, mimeType.toUtf8().constData(), int32_t(fd));
}

void DataSourceInterface::cancel()
{
    wl_data_source_send_cancelled(d->dataSource);
}

wl_resource *DataSourceInterface::resource() const
{
    return d->dataSource;
}

QStringList DataSourceInterface::mimeTypes() const
{
    return d->mimeTypes;
}

DataSourceInterface *DataSourceInterface::get(wl_resource *native)
{
    return Private::get(native);
}

}
}
