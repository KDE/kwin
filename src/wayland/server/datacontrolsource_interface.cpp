/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontrolsource_interface.h"
#include "datacontroldevicemanager_interface.h"
#include "clientconnection.h"
#include "resource_p.h"
// Qt
#include <QStringList>
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>
// system
#include <unistd.h>

namespace KWaylandServer
{

class DataControlSourceInterfacePrivate : public QtWaylandServer::zwlr_data_control_source_v1
{
public:
    DataControlSourceInterfacePrivate(DataControlSourceInterface *q, ::wl_resource *resource);

    QStringList mimeTypes;
    DataControlSourceInterface *q;

protected:
    void zwlr_data_control_source_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_source_v1_offer(Resource *resource, const QString &mime_type) override;
    void zwlr_data_control_source_v1_destroy(Resource *resource) override;
};

DataControlSourceInterfacePrivate::DataControlSourceInterfacePrivate(DataControlSourceInterface *_q, ::wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_source_v1(resource)
    , q(_q)
{
}

void DataControlSourceInterfacePrivate::zwlr_data_control_source_v1_destroy_resource(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void DataControlSourceInterfacePrivate::zwlr_data_control_source_v1_offer(Resource *, const QString &mimeType)
{
    mimeTypes << mimeType;
    emit q->mimeTypeOffered(mimeType);
}

void DataControlSourceInterfacePrivate::zwlr_data_control_source_v1_destroy(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlSourceInterface::DataControlSourceInterface(DataControlDeviceManagerInterface *parent, ::wl_resource *resource)
    : AbstractDataSource(nullptr, parent)
    , d(new DataControlSourceInterfacePrivate(this, resource))
{
}

DataControlSourceInterface::~DataControlSourceInterface() = default;

void DataControlSourceInterface::requestData(const QString &mimeType, qint32 fd)
{
    d->send_send(mimeType, fd);
    close(fd);
}

void DataControlSourceInterface::cancel()
{
    d->send_cancelled();
}

QStringList DataControlSourceInterface::mimeTypes() const
{
    return d->mimeTypes;
}

wl_client *DataControlSourceInterface::client()
{
    return d->resource()->client();
}

DataControlSourceInterface *DataControlSourceInterface::get(wl_resource *native)
{
    auto priv = static_cast<DataControlSourceInterfacePrivate*>(QtWaylandServer::zwlr_data_control_source_v1::Resource::fromResource(native)->object());
    return priv->q;
}

}
