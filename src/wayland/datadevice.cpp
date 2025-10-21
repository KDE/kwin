/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "datadevice.h"
#include "datadevice_p.h"
#include "datadevicemanager.h"
#include "dataoffer.h"
#include "datasource.h"
#include "display.h"
#include "pointer.h"
#include "seat.h"
#include "seat_p.h"
#include "surface.h"

namespace KWin
{
class DragAndDropIconPrivate
{
public:
    explicit DragAndDropIconPrivate(SurfaceInterface *surface);

    QPointer<SurfaceInterface> surface;
    QPoint position;
};

DragAndDropIconPrivate::DragAndDropIconPrivate(SurfaceInterface *surface)
    : surface(surface)
{
}

DragAndDropIcon::DragAndDropIcon(SurfaceInterface *surface)
    : QObject(surface)
    , d(new DragAndDropIconPrivate(surface))
{
    connect(surface, &SurfaceInterface::committed, this, &DragAndDropIcon::commit);
}

DragAndDropIcon::~DragAndDropIcon()
{
}

SurfaceRole *DragAndDropIcon::role()
{
    static SurfaceRole role(QByteArrayLiteral("dnd_icon"));
    return &role;
}

void DragAndDropIcon::commit()
{
    d->position += d->surface->offset();
    Q_EMIT changed();
}

QPoint DragAndDropIcon::position() const
{
    return d->position;
}

SurfaceInterface *DragAndDropIcon::surface() const
{
    return d->surface;
}

DataDeviceInterfacePrivate *DataDeviceInterfacePrivate::get(DataDeviceInterface *device)
{
    return device->d.get();
}

DataDeviceInterfacePrivate::DataDeviceInterfacePrivate(SeatInterface *seat, DataDeviceInterface *_q, wl_resource *resource)
    : QtWaylandServer::wl_data_device(resource)
    , seat(seat)
    , q(_q)
{
}

void DataDeviceInterfacePrivate::data_device_start_drag(Resource *resource,
                                                        wl_resource *sourceResource,
                                                        wl_resource *originResource,
                                                        wl_resource *iconResource,
                                                        uint32_t serial)
{
    SurfaceInterface *originSurface = SurfaceInterface::get(originResource);
    DataSourceInterface *dataSource = nullptr;
    if (sourceResource) {
        dataSource = DataSourceInterface::get(sourceResource);
    }

    DragAndDropIcon *dragIcon = nullptr;
    if (SurfaceInterface *iconSurface = SurfaceInterface::get(iconResource)) {
        if (const SurfaceRole *role = iconSurface->role()) {
            if (role != DragAndDropIcon::role()) {
                wl_resource_post_error(resource->handle, 0,
                                       "the wl_surface already has a role assigned %s", role->name().constData());
                return;
            }
        } else {
            iconSurface->setRole(DragAndDropIcon::role());
        }

        // drag icon lifespan is mapped to surface lifespan
        dragIcon = new DragAndDropIcon(iconSurface);
    }

    Q_EMIT q->dragRequested(dataSource, originSurface, serial, dragIcon);
}

void DataDeviceInterfacePrivate::data_device_set_selection(Resource *resource, wl_resource *source, uint32_t serial)
{
    DataSourceInterface *dataSource = DataSourceInterface::get(source);

    if (dataSource && dataSource->supportedDragAndDropActions() && wl_resource_get_version(dataSource->resource()) >= WL_DATA_SOURCE_ACTION_SINCE_VERSION) {
        wl_resource_post_error(dataSource->resource(), QtWaylandServer::wl_data_source::error_invalid_source, "Data source is for drag and drop");
        return;
    }

    if (dataSource && dataSource->xdgToplevelDrag()) {
        wl_resource_post_error(resource->handle, QtWaylandServer::wl_data_source::error_invalid_source, "Data source is for drag and drop");
        return;
    }

    Q_EMIT q->selectionChanged(dataSource, serial);
}

void DataDeviceInterfacePrivate::data_device_release(QtWaylandServer::wl_data_device::Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

DataOfferInterface *DataDeviceInterfacePrivate::createDataOffer(AbstractDataSource *source)
{
    if (!source) {
        // a data offer can only exist together with a source
        return nullptr;
    }

    wl_resource *data_offer_resource = wl_resource_create(resource()->client(), &wl_data_offer_interface, resource()->version(), 0);
    if (!data_offer_resource) {
        wl_resource_post_no_memory(resource()->handle);
        return nullptr;
    }

    DataOfferInterface *offer = new DataOfferInterface(source, data_offer_resource);
    send_data_offer(offer->resource());
    offer->sendAllOffers();
    return offer;
}

void DataDeviceInterfacePrivate::data_device_destroy_resource(QtWaylandServer::wl_data_device::Resource *resource)
{
    Q_EMIT q->aboutToBeDestroyed();
    delete q;
}

DataDeviceInterface::DataDeviceInterface(SeatInterface *seat, wl_resource *resource)
    : AbstractDropHandler(nullptr)
    , d(new DataDeviceInterfacePrivate(seat, this, resource))
{
    SeatInterfacePrivate *seatPrivate = SeatInterfacePrivate::get(seat);
    seatPrivate->registerDataDevice(this);
}

DataDeviceInterface::~DataDeviceInterface() = default;

SeatInterface *DataDeviceInterface::seat() const
{
    return d->seat;
}

void DataDeviceInterface::sendSelection(AbstractDataSource *other)
{
    auto r = other ? d->createDataOffer(other) : nullptr;
    d->send_selection(r ? r->resource() : nullptr);
}

void DataDeviceInterface::drop()
{
    d->send_drop();
}

static DataDeviceManagerInterface::DnDAction chooseDndAction(AbstractDataSource *source, DataOfferInterface *offer, Qt::KeyboardModifiers keyboardModifiers)
{
    // first compositor picks an action if modifiers are pressed and it's supported both sides
    if (keyboardModifiers.testFlag(Qt::ControlModifier)) {
        if (source->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Copy) && offer->supportedDragAndDropActions().has_value() && offer->supportedDragAndDropActions()->testFlag(DataDeviceManagerInterface::DnDAction::Copy)) {
            return DataDeviceManagerInterface::DnDAction::Copy;
        }
    }
    if (keyboardModifiers.testFlag(Qt::ShiftModifier)) {
        if (source->supportedDragAndDropActions().testFlag(DataDeviceManagerInterface::DnDAction::Move) && offer->supportedDragAndDropActions().has_value() && offer->supportedDragAndDropActions()->testFlag(DataDeviceManagerInterface::DnDAction::Move)) {
            return DataDeviceManagerInterface::DnDAction::Move;
        }
    }

    // otherwise we pick the preferred action from the target if the source supported it
    if (offer->preferredDragAndDropAction().has_value()) {
        if (source->supportedDragAndDropActions().testFlag(*offer->preferredDragAndDropAction())) {
            return *offer->preferredDragAndDropAction();
        }
    }

    // finally pick something everyone supports in a deterministic fashion
    if (offer->supportedDragAndDropActions().has_value()) {
        for (const DataDeviceManagerInterface::DnDAction action : {DataDeviceManagerInterface::DnDAction::Copy, DataDeviceManagerInterface::DnDAction::Move, DataDeviceManagerInterface::DnDAction::Ask}) {
            if (source->supportedDragAndDropActions().testFlag(action) && offer->supportedDragAndDropActions()->testFlag(action)) {
                return action;
            }
        }
    }

    return DataDeviceManagerInterface::DnDAction::None;
}

void DataDeviceInterface::updateDragTarget(SurfaceInterface *surface, const QPointF &position, quint32 serial)
{
    if (d->drag.surface == surface) {
        return;
    }

    auto dragSource = d->seat->dragSource();

    if (d->drag.surface) {
        d->send_leave();

        disconnect(d->drag.destroyConnection);
        d->drag.destroyConnection = QMetaObject::Connection();
        d->drag.surface = nullptr;
        // don't update serial, we need it
    }

    if (d->drag.offer) {
        // Keep the data offer alive so the target client can retrieve data after a drop. The
        // data offer will be destroyed after the target client has finished using it.
        if (dragSource && dragSource->isDropPerformed()) {
            if (dragSource->selectedDndAction() != DataDeviceManagerInterface::DnDAction::Ask) {
                disconnect(d->drag.sourceActionConnection);
                disconnect(d->drag.targetActionConnection);
                disconnect(d->drag.keyboardModifiersConnection);
            }
        } else {
            delete d->drag.offer;
        }

        d->drag.offer = nullptr;

        d->drag.sourceActionConnection = QMetaObject::Connection();
        d->drag.targetActionConnection = QMetaObject::Connection();
        d->drag.keyboardModifiersConnection = QMetaObject::Connection();
    }

    if (!surface || !dragSource) {
        if (auto s = dragSource) {
            s->dndAction(DataDeviceManagerInterface::DnDAction::None);
        }
        return;
    }

    if (dragSource) {
        dragSource->accept(QString());
    }
    d->drag.offer = d->createDataOffer(dragSource);
    d->drag.offer->sendSourceActions();

    d->drag.surface = surface;
    d->drag.destroyConnection = connect(d->drag.surface, &SurfaceInterface::aboutToBeDestroyed, this, [this] {
        d->send_leave();
        if (d->drag.offer) {
            disconnect(d->drag.sourceActionConnection);
            disconnect(d->drag.targetActionConnection);
            disconnect(d->drag.keyboardModifiersConnection);

            delete d->drag.offer;
        }
        d->drag = DataDeviceInterfacePrivate::Drag();
    });

    if (d->drag.offer) {
        auto matchOffers = [dragSource, offer = d->drag.offer] {
            Qt::KeyboardModifiers keyboardModifiers;
            if (!dragSource->isDropPerformed()) { // ignore keyboard modifiers when in "ask" negotiation
                keyboardModifiers = dragSource->keyboardModifiers();
            }

            const DataDeviceManagerInterface::DnDAction action = chooseDndAction(dragSource, offer, keyboardModifiers);
            offer->dndAction(action);
            dragSource->dndAction(action);
        };
        matchOffers();
        d->drag.targetActionConnection = connect(d->drag.offer, &DataOfferInterface::dragAndDropActionsChanged, dragSource, matchOffers);
        d->drag.sourceActionConnection = connect(dragSource, &AbstractDataSource::supportedDragAndDropActionsChanged, d->drag.offer, matchOffers);
        d->drag.keyboardModifiersConnection = connect(dragSource, &AbstractDataSource::keyboardModifiersChanged, d->drag.offer, matchOffers);
    }

    const QPointF pos = d->seat->dragSurfaceTransformation().map(position);
    d->send_enter(serial, surface->resource(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()), d->drag.offer ? d->drag.offer->resource() : nullptr);
}

void DataDeviceInterface::motion(const QPointF &position)
{
    if (!d->drag.surface) {
        return;
    }

    const QPointF pos = d->seat->dragSurfaceTransformation().map(position);
    d->send_motion(d->seat->timestamp().count(), wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
}

wl_client *DataDeviceInterface::client()
{
    return d->resource()->client();
}

}

#include "moc_datadevice.cpp"
