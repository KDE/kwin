/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datacontrolsource_v1.h"
#include "clientconnection.h"
#include "datacontroldevicemanager_v1.h"
#include "datacontrolsource_v1_p.h"
#include "utils/resource.h"
// Qt
#include <QStringList>
// Wayland
#include <qwayland-server-ext-data-control-v1.h>
// system
#include <unistd.h>

namespace KWin
{

ExtDataControlSourcePrivate::ExtDataControlSourcePrivate(wl_resource *resource)
    : QtWaylandServer::ext_data_control_source_v1(resource)
{
}

void ExtDataControlSourcePrivate::ext_data_control_source_v1_destroy_resource(QtWaylandServer::ext_data_control_source_v1::Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void ExtDataControlSourcePrivate::ext_data_control_source_v1_offer(Resource *, const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_EMIT q->mimeTypeOffered(mimeType);
}

void ExtDataControlSourcePrivate::ext_data_control_source_v1_destroy(QtWaylandServer::ext_data_control_source_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

WlrDataControlSourcePrivate::WlrDataControlSourcePrivate(wl_resource *resource)
    : QtWaylandServer::zwlr_data_control_source_v1(resource)
{
}

void WlrDataControlSourcePrivate::zwlr_data_control_source_v1_destroy_resource(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void WlrDataControlSourcePrivate::zwlr_data_control_source_v1_offer(Resource *, const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_EMIT q->mimeTypeOffered(mimeType);
}

void WlrDataControlSourcePrivate::zwlr_data_control_source_v1_destroy(QtWaylandServer::zwlr_data_control_source_v1::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataControlSourceV1Interface::DataControlSourceV1Interface(DataControlDeviceManagerV1Interface *parent, std::unique_ptr<DataControlSourceV1InterfacePrivate> &&d)
    : AbstractDataSource(parent)
    , d(std::move(d))
{
    this->d->q = this;
}

DataControlSourceV1Interface::~DataControlSourceV1Interface() = default;

void DataControlSourceV1Interface::requestData(const QString &mimeType, qint32 fd)
{
    d->sendSend(mimeType, fd);
    close(fd);
}

void DataControlSourceV1Interface::cancel()
{
    d->sendCancelled();
}

QStringList DataControlSourceV1Interface::mimeTypes() const
{
    return d->mimeTypes;
}

wl_client *DataControlSourceV1Interface::client() const
{
    return d->client();
}

DataControlSourceV1Interface *DataControlSourceV1Interface::get(wl_resource *native)
{
    if (auto sourcePrivate = resource_cast<ExtDataControlSourcePrivate *>(native)) {
        return sourcePrivate->q;
    }
    if (auto sourcePrivate = resource_cast<WlrDataControlSourcePrivate *>(native)) {
        return sourcePrivate->q;
    }
    return nullptr;
}

}

#include "moc_datacontrolsource_v1.cpp"
