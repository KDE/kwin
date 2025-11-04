/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "dataoffer.h"
#include "datadevice.h"
#include "datasource.h"

// Qt
#include <QPointer>
#include <QStringList>
// Wayland
#include <qwayland-server-wayland.h>
// system
#include <unistd.h>

namespace KWin
{
class DataOfferInterfacePrivate : public QtWaylandServer::wl_data_offer
{
public:
    DataOfferInterfacePrivate(AbstractDataSource *source, DataOfferInterface *q, wl_resource *resource);
    DataOfferInterface *q;
    QPointer<AbstractDataSource> source;

    std::optional<DnDActions> supportedDnDActions = std::nullopt;
    std::optional<DnDAction> preferredDnDAction = std::nullopt;

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
        supportedDnDActions = DnDAction::Copy | DnDAction::Move;
        preferredDnDAction = DnDAction::Copy;
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
    if (!source || !source->mimeTypes().contains(mime_type)) {
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
    source.clear();
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

    DnDActions supportedActions;
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_copy) {
        supportedActions |= DnDAction::Copy;
    }
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_move) {
        supportedActions |= DnDAction::Move;
    }
    if (dnd_actions & QtWaylandServer::wl_data_device_manager::dnd_action_ask) {
        supportedActions |= DnDAction::Ask;
    }

    DnDAction preferredAction = DnDAction::None;
    if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_copy) {
        preferredAction = DnDAction::Copy;
    } else if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_move) {
        preferredAction = DnDAction::Move;
    } else if (preferred_action == QtWaylandServer::wl_data_device_manager::dnd_action_ask) {
        preferredAction = DnDAction::Ask;
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
    if (actions.testFlag(DnDAction::Copy)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_copy;
    }
    if (actions.testFlag(DnDAction::Move)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_move;
    }
    if (actions.testFlag(DnDAction::Ask)) {
        wlActions |= QtWaylandServer::wl_data_device_manager::dnd_action_ask;
    }
    d->send_source_actions(wlActions);
}

void DataOfferInterfacePrivate::data_offer_destroy_resource(QtWaylandServer::wl_data_offer::Resource *resource)
{
    Q_EMIT q->discarded();
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

DataOfferInterface::~DataOfferInterface()
{
    if (d->source && d->source->isDropPerformed()) {
        d->source->dndFinished();
        d->source.clear();
    }
}

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

std::optional<DnDActions> DataOfferInterface::supportedDragAndDropActions() const
{
    return d->supportedDnDActions;
}

std::optional<DnDAction> DataOfferInterface::preferredDragAndDropAction() const
{
    return d->preferredDnDAction;
}

void DataOfferInterface::dndAction(DnDAction action)
{
    if (d->resource()->version() < WL_DATA_OFFER_ACTION_SINCE_VERSION) {
        return;
    }
    uint32_t wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_none;
    if (action == DnDAction::Copy) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_copy;
    } else if (action == DnDAction::Move) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_move;
    } else if (action == DnDAction::Ask) {
        wlAction = QtWaylandServer::wl_data_device_manager::dnd_action_ask;
    }
    d->send_action(wlAction);
}
}

#include "moc_dataoffer.cpp"
