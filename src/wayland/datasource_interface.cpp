/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "datasource_interface.h"
#include "datadevicemanager_interface.h"
#include "clientconnection.h"
#include "resource_p.h"
// Qt
#include <QStringList>
// Wayland
#include <wayland-server.h>
// system
#include <unistd.h>

namespace KWaylandServer
{

class DataSourceInterface::Private : public Resource::Private
{
public:
    Private(DataSourceInterface *q, DataDeviceManagerInterface *parent, wl_resource *parentResource);
    ~Private();

    QStringList mimeTypes;
    DataDeviceManagerInterface::DnDActions supportedDnDActions = DataDeviceManagerInterface::DnDAction::None;

private:
    DataSourceInterface *q_func() {
        return reinterpret_cast<DataSourceInterface *>(q);
    }
    void offer(const QString &mimeType);

    static void offerCallback(wl_client *client, wl_resource *resource, const char *mimeType);
    static void setActionsCallback(wl_client *client, wl_resource *resource, uint32_t dnd_actions);

    const static struct wl_data_source_interface s_interface;
};

#ifndef K_DOXYGEN
const struct wl_data_source_interface DataSourceInterface::Private::s_interface = {
    offerCallback,
    resourceDestroyedCallback,
    setActionsCallback
};
#endif

DataSourceInterface::Private::Private(DataSourceInterface *q, DataDeviceManagerInterface *parent, wl_resource *parentResource)
    : Resource::Private(q, parent, parentResource, &wl_data_source_interface, &s_interface)
{
}

DataSourceInterface::Private::~Private() = default;

void DataSourceInterface::Private::offerCallback(wl_client *client, wl_resource *resource, const char *mimeType)
{
    Q_UNUSED(client)
    cast<Private>(resource)->offer(QString::fromUtf8(mimeType));
}

void DataSourceInterface::Private::offer(const QString &mimeType)
{
    mimeTypes << mimeType;
    Q_Q(DataSourceInterface);
    emit q->mimeTypeOffered(mimeType);
}

void DataSourceInterface::Private::setActionsCallback(wl_client *client, wl_resource *resource, uint32_t dnd_actions)
{
    Q_UNUSED(client)
    DataDeviceManagerInterface::DnDActions supportedActions;
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Copy;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Move;
    }
    if (dnd_actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
        supportedActions |= DataDeviceManagerInterface::DnDAction::Ask;
    }
    // verify that the no other actions are sent
    if (dnd_actions & ~(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY | WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE | WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)) {
        wl_resource_post_error(resource, WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK, "Invalid action mask");
        return;
    }
    auto p = cast<Private>(resource);
    if (p->supportedDnDActions!= supportedActions) {
        p->supportedDnDActions = supportedActions;
        emit p->q_func()->supportedDragAndDropActionsChanged();
    }
}

DataSourceInterface::DataSourceInterface(DataDeviceManagerInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, parent, parentResource))
{
    if (wl_resource_get_version(parentResource) < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        Q_D();
        d->supportedDnDActions = DataDeviceManagerInterface::DnDAction::Copy;
    }
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
    if (d->resource) {
        wl_data_source_send_send(d->resource, mimeType.toUtf8().constData(), int32_t(fd));
    }
    close(fd);
}

void DataSourceInterface::cancel()
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_data_source_send_cancelled(d->resource);
    client()->flush();
}

QStringList DataSourceInterface::mimeTypes() const
{
    Q_D();
    return d->mimeTypes;
}

DataSourceInterface *DataSourceInterface::get(wl_resource *native)
{
    return Private::get<DataSourceInterface>(native);
}

DataSourceInterface::Private *DataSourceInterface::d_func() const
{
    return reinterpret_cast<DataSourceInterface::Private*>(d.data());
}

DataDeviceManagerInterface::DnDActions DataSourceInterface::supportedDragAndDropActions() const
{
    Q_D();
    return d->supportedDnDActions;
}

void DataSourceInterface::dropPerformed()
{
    Q_D();
    if (wl_resource_get_version(d->resource) < WL_DATA_SOURCE_DND_DROP_PERFORMED_SINCE_VERSION) {
        return;
    }
    wl_data_source_send_dnd_drop_performed(d->resource);
}

void DataSourceInterface::dndFinished()
{
    Q_D();
    if (wl_resource_get_version(d->resource) < WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION) {
        return;
    }
    wl_data_source_send_dnd_finished(d->resource);
}

void DataSourceInterface::dndAction(DataDeviceManagerInterface::DnDAction action)
{
    Q_D();
    if (wl_resource_get_version(d->resource) < WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        return;
    }
    uint32_t wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (action == DataDeviceManagerInterface::DnDAction::Copy) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    } else if (action == DataDeviceManagerInterface::DnDAction::Move ) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    } else if (action == DataDeviceManagerInterface::DnDAction::Ask) {
        wlAction = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    }
    wl_data_source_send_action(d->resource, wlAction);
}

}
