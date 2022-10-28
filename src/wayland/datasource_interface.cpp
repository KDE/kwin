/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datasource_interface.h"
#include "clientconnection.h"
#include "datadevicemanager_interface.h"
#include "utils.h"
// Qt
#include <QStringList>
// Wayland
#include <qwayland-server-wayland.h>
// system
#include <unistd.h>

namespace KWaylandServer
{
class DataSourceInterfacePrivate : public QtWaylandServer::wl_data_source
{
public:
    DataSourceInterfacePrivate(DataSourceInterface *_q, ::wl_resource *resource);

    DataSourceInterface *q;
    QStringList mimeTypes;
    DataDeviceManagerInterface::DnDActions supportedDnDActions = DataDeviceManagerInterface::DnDAction::None;
    DataDeviceManagerInterface::DnDAction selectedDndAction = DataDeviceManagerInterface::DnDAction::None;
    bool isAccepted = false;

protected:
    void data_source_destroy_resource(Resource *resource) override;
    void data_source_offer(Resource *resource, const QString &mime_type) override;
    void data_source_destroy(Resource *resource) override;
    void data_source_set_actions(Resource *resource, uint32_t dnd_actions) override;

private:
    void offer(const QString &mimeType);
};

DataSourceInterfacePrivate::DataSourceInterfacePrivate(DataSourceInterface *_q, ::wl_resource *resource)
    : QtWaylandServer::wl_data_source(resource)
    , q(_q)
{
}

void DataSourceInterfacePrivate::data_source_destroy_resource(Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

void DataSourceInterfacePrivate::data_source_offer(QtWaylandServer::wl_data_source::Resource *resource, const QString &mime_type)
{
    mimeTypes << mime_type;
    Q_EMIT q->mimeTypeOffered(mime_type);
}

void DataSourceInterfacePrivate::data_source_destroy(QtWaylandServer::wl_data_source::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataSourceInterfacePrivate::offer(const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_EMIT q->mimeTypeOffered(mimeType);
}

void DataSourceInterfacePrivate::data_source_set_actions(Resource *resource, uint32_t dnd_actions)
{
    // verify that the no other actions are sent
    if (dnd_actions
        & ~(QtWaylandServer::wl_data_device_manager::dnd_action_copy | QtWaylandServer::wl_data_device_manager::dnd_action_move
            | QtWaylandServer::wl_data_device_manager::dnd_action_ask)) {
        wl_resource_post_error(resource->handle, error_invalid_action_mask, "Invalid action mask");
        return;
    }
    DataDeviceManagerInterface::DnDActions supportedActions;
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_copy) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Copy;
    }
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_move) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Move;
    }
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_ask) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Ask;
    }
    if (supportedDnDActions != supportedActions) {
        supportedDnDActions = supportedActions;
        Q_EMIT q->supportedDragAndDropActionsChanged();
    }
}

DataSourceInterface::DataSourceInterface(wl_resource *resource)
    : d(new DataSourceInterfacePrivate(this, resource))
{
    if (d->resource()->version() < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        d->supportedDnDActions = DataDeviceManagerInterface::DnDAction::Copy;
    }
}

DataSourceInterface::~DataSourceInterface() = default;

void DataSourceInterface::accept(const QString &mimeType)
{
    d->send_target(mimeType);
    d->isAccepted = !mimeType.isNull();
}

void DataSourceInterface::requestData(const QString &mimeType, qint32 fd)
{
    d->send_send(mimeType, int32_t(fd));
    close(fd);
}

void DataSourceInterface::cancel()
{
    d->send_cancelled();
}

QStringList DataSourceInterface::mimeTypes() const
{
    return d->mimeTypes;
}

DataSourceInterface *DataSourceInterface::get(wl_resource *native)
{
    if (auto sourcePrivate = resource_cast<DataSourceInterfacePrivate *>(native)) {
        return sourcePrivate->q;
    }
    return nullptr;
}

DataDeviceManagerInterface::DnDActions DataSourceInterface::supportedDragAndDropActions() const
{
    return d->supportedDnDActions;
}

DataDeviceManagerInterface::DnDAction DataSourceInterface::selectedDndAction() const
{
    return d->selectedDndAction;
}

void DataSourceInterface::dropPerformed()
{
    if (d->resource()->version() < WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION) {
        return;
    }
    d->send_dnd_drop_performed();
}

void DataSourceInterface::dndFinished()
{
    if (d->resource()->version() < WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
        return;
    }
    d->send_dnd_finished();
}

void DataSourceInterface::dndAction(DataDeviceManagerInterface::DnDAction action)
{
    d->selectedDndAction = action;

    if (d->resource()->version() < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        return;
    }
    uint32_t wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_none;
    if (action == DataDeviceManagerInterface::DnDAction::Copy) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_copy;
    } else if (action == DataDeviceManagerInterface::DnDAction::Move) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_move;
    } else if (action == DataDeviceManagerInterface::DnDAction::Ask) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_ask;
    }
    d->send_action(wlAction);
}

void DataSourceInterface::dndCancelled()
{
    // for v3 or less, cancel should not be called after a failed drag operation
    if (wl_resource_get_version(resource()) < 3) {
        return;
    }
    d->send_cancelled();
}

wl_resource *DataSourceInterface::resource() const
{
    return d->resource()->handle;
}

wl_client *DataSourceInterface::client() const
{
    return d->resource()->client();
}

bool DataSourceInterface::isAccepted() const
{
    return d->isAccepted;
}

void DataSourceInterface::setAccepted(bool accepted)
{
    d->isAccepted = accepted;
}

}
