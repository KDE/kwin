/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontrolsource_v1.h"
#include "clientconnection.h"
#include "datacontroldevicemanager_v1.h"
#include "utils/resource.h"
// Qt
#include <QStringList>
// Wayland
#include <qwayland-server-wlr-data-control-unstable-v1.h>
// system
#include <unistd.h>

namespace KWin
{
class DataControlSourceV1InterfacePrivate : public QtWaylandServer::zwlr_data_control_source_v1
{
public:
    DataControlSourceV1InterfacePrivate(DataControlSourceV1Interface *q, ::wl_resource *resource);

    QStringList mimeTypes;
    DataControlSourceV1Interface *q;

protected:
    void zwlr_data_control_source_v1_destroy_resource(Resource *resource) override;
    void zwlr_data_control_source_v1_offer(Resource *resource, const QString &mime_type) override;
    void zwlr_data_control_source_v1_destroy(Resource *resource) override;
};

DataControlSourceV1InterfacePrivate::DataControlSourceV1InterfacePrivate(DataControlSourceV1Interface *_q, ::wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_source_v1(resource)
    , q(_q)
{
}

void DataControlSourceV1InterfacePrivate::zwlr_data_control_source_v1_destroy_resource(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void DataControlSourceV1InterfacePrivate::zwlr_data_control_source_v1_offer(Resource *, const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_EMIT q->mimeTypeOffered(mimeType);
}

void DataControlSourceV1InterfacePrivate::zwlr_data_control_source_v1_destroy(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlSourceV1Interface::DataControlSourceV1Interface(DataControlDeviceManagerV1Interface *parent, ::wl_resource *resource)
    : AbstractDataSource(parent)
    , d(new DataControlSourceV1InterfacePrivate(this, resource))
{
}

DataControlSourceV1Interface::~DataControlSourceV1Interface() = default;

void DataControlSourceV1Interface::requestData(const QString &mimeType, qint32 fd)
{
    d->send_send(mimeType, fd);
    close(fd);
}

void DataControlSourceV1Interface::cancel()
{
    d->send_cancelled();
}

QStringList DataControlSourceV1Interface::mimeTypes() const
{
    return d->mimeTypes;
}

wl_client *DataControlSourceV1Interface::client() const
{
    return d->resource()->client();
}

DataControlSourceV1Interface *DataControlSourceV1Interface::get(wl_resource *native)
{
    if (auto sourcePrivate = resource_cast<DataControlSourceV1InterfacePrivate *>(native)) {
        return sourcePrivate->q;
    }
    return nullptr;
}

}

#include "moc_datacontrolsource_v1.cpp"
