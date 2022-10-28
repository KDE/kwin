/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "dataoffer_interface.h"
#include "datadevice_interface.h"
#include "datasource_interface.h"

// Qt
#include <QPointer>
#include <QStringList>
// Wayland
#include <qwayland-server-wayland.h>
// system
#include <unistd.h>

namespace KWaylandServer
{
class DataOfferInterfacePrivate : public QtWaylandServer::wl_data_offer
{
public:
    DataOfferInterfacePrivate(AbstractDataSource *source, DataOfferInterface *q, wl_resource *resource);
    DataOfferInterface *q;
    QPointer<AbstractDataSource> source;

    std::optional<DataDeviceManagerInterface::DnDActions> supportedDnDActions = std::nullopt;
    std::optional<DataDeviceManagerInterface::DnDAction> preferredDnDAction = std::nullopt;

protected:
    void data_offer_destroy_resource(Resource *resource) override;
    void data_offer_accept(Resource *resource, uint32_t serial, const QString &mime_type) override;
    void data_offer_receive(Resource *resource, const QString &mime_type, int32_t fd) override;
    void data_offer_destroy(Resource *resource) override;
    void data_offer_finish(Resource *resource) override;
    void data_offer_set_actions(Resource *resource, uint32_t dnd_actions, uint32_t preferred_action) override;
};

DataOfferInterfacePrivate::DataOfferInterfacePrivate(AbstractDataSource *_source, DataOfferInterface *_q, wl_resource *resource)
    : QtWaylandServer::wl_data_offer(resource)
    , q(_q)
    , source(_source)
{
    // defaults are set to sensible values for < version 3 interfaces
    if (wl_resource_get_version(resource) < WL_DATA_OFFER_ACTION_SINCE_VERSION) {
        supportedDnDActions = DataDeviceManagerInterface::DnDAction::Copy | DataDeviceManagerInterface::DnDAction::Move;
        preferredDnDAction = DataDeviceManagerInterface::DnDAction::Copy;
    }
}

void DataOfferInterfacePrivate::data_offer_accept(Resource *resource, uint32_t serial, const QString &mime_type)
{
    if (!source) {
        return;
    }
    source->accept(mime_type);
}

void DataOfferInterfacePrivate::data_offer_receive(Resource *resource, const QString &mime_type, int32_t fd)
{
    if (!source) {
        close(fd);
        return;
    }
    source->requestData(mime_type, fd);
}

void DataOfferInterfacePrivate::data_offer_destroy(QtWaylandServer::wl_data_offer::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DataOfferInterfacePrivate::data_offer_finish(Resource *resource)
{
    if (!source) {
        return;
    }
    source->dndFinished();
    // TODO: It is a client error to perform other requests than wl_data_offer.destroy after this one
}

void DataOfferInterfacePrivate::data_offer_set_actions(Resource *resource, uint32_t dnd_actions, uint32_t preferred_action)
{
    // TODO: check it's drag and drop, otherwise send error
    // verify that the no other actions are sent
    if (dnd_actions
        & ~(QtWaylandServer::wl_data_device_manager::dnd_action_copy | QtWaylandServer::wl_data_device_manager::dnd_action_move
            | QtWaylandServer::wl_data_device_manager::dnd_action_ask)) {
        wl_resource_post_error(resource->handle, error_invalid_action_mask, "Invalid action mask");
        return;
    }

    if (preferred_action != QtWaylandServer::wl_data_device_manager::dnd_action_copy
        && preferred_action != QtWaylandServer::wl_data_device_manager::dnd_action_move
        && preferred_action != QtWaylandServer::wl_data_device_manager::dnd_action_ask
        && preferred_action != QtWaylandServer::wl_data_device_manager::dnd_action_none) {
        wl_resource_post_error(resource->handle, error_invalid_action, "Invalid preferred action");
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

    DataDeviceManagerInterface::DnDAction preferredAction = DataDeviceManagerInterface::DnDAction::None;
    if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_copy) {
        preferredAction = DataDeviceManagerInterface::DnDAction::Copy;
    } else if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_move) {
        preferredAction = DataDeviceManagerInterface::DnDAction::Move;
    } else if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_ask) {
        preferredAction = DataDeviceManagerInterface::DnDAction::Ask;
    }

    if (supportedDnDActions != supportedActions || preferredDnDAction != preferredAction) {
        supportedDnDActions = supportedActions;
        preferredDnDAction = preferredAction;
        Q_EMIT q->dragAndDropActionsChanged();
    }
}

void DataOfferInterface::sendSourceActions()
{
    if (!d->source) {
        return;
    }
    if (d->resource()->version() < WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
        return;
    }
    uint32_t wlActions = QtWaylandServer::wl_data_device_manager::dnd_action_none;
    const auto actions = d->source->supportedDragAndDropActions();
    if (actions.testFlag(DataDeviceManagerInterface::DnDAction::Copy)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_copy;
    }
    if (actions.testFlag(DataDeviceManagerInterface::DnDAction::Move)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_move;
    }
    if (actions.testFlag(DataDeviceManagerInterface::DnDAction::Ask)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_ask;
    }
    d->send_source_actions(wlActions);
}

void DataOfferInterfacePrivate::data_offer_destroy_resource(QtWaylandServer::wl_data_offer::Resource *resource)
{
    delete q;
}

DataOfferInterface::DataOfferInterface(AbstractDataSource *source, wl_resource *resource)
    : QObject(nullptr)
    , d(new DataOfferInterfacePrivate(source, this, resource))
{
    Q_ASSERT(source);
    connect(source, &DataSourceInterface::mimeTypeOffered, this, [this](const QString &mimeType) {
        d->send_offer(mimeType);
    });
}

DataOfferInterface::~DataOfferInterface() = default;

void DataOfferInterface::sendAllOffers()
{
    for (const QString &mimeType : d->source->mimeTypes()) {
        d->send_offer(mimeType);
    }
}

wl_resource *DataOfferInterface::resource() const
{
    return d->resource()->handle;
}

std::optional<DataDeviceManagerInterface::DnDActions> DataOfferInterface::supportedDragAndDropActions() const
{
    return d->supportedDnDActions;
}

std::optional<DataDeviceManagerInterface::DnDAction> DataOfferInterface::preferredDragAndDropAction() const
{
    return d->preferredDnDAction;
}

void DataOfferInterface::dndAction(DataDeviceManagerInterface::DnDAction action)
{
    if (d->resource()->version() < WL_DATA_OFFER_ACTION_SINCE_VERSION) {
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

}
