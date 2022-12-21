/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "touch_interface.h"
#include "clientconnection.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"
#include "touch_interface_p.h"

namespace KWaylandServer
{
TouchInterfacePrivate *TouchInterfacePrivate::get(TouchInterface *touch)
{
    return touch->d.get();
}

TouchInterfacePrivate::TouchInterfacePrivate(TouchInterface *q, SeatInterface *seat)
    : q(q)
    , seat(seat)
{
}

void TouchInterfacePrivate::touch_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

QList<TouchInterfacePrivate::Resource *> TouchInterfacePrivate::touchesForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

bool TouchInterfacePrivate::hasTouchesForClient(ClientConnection *client) const
{
    return resourceMap().contains(client->client());
}

TouchInterface::TouchInterface(SeatInterface *seat)
    : d(new TouchInterfacePrivate(this, seat))
{
}

TouchInterface::~TouchInterface()
{
}

SurfaceInterface *TouchInterface::focusedSurface() const
{
    return d->focusedSurface;
}

void TouchInterface::sendCancel()
{
    if (!d->focusedSurface) {
        return;
    }

    const auto touchResources = d->touchesForClient(d->focusedSurface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_cancel(resource->handle);
    }
}

void TouchInterface::sendFrame()
{
    if (!d->focusedSurface) {
        return;
    }

    const auto touchResources = d->touchesForClient(d->focusedSurface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_frame(resource->handle);
    }
}

void TouchInterface::sendMotion(qint32 id, const QPointF &localPos)
{
    if (!d->focusedSurface) {
        return;
    }

    QPointF pos = d->focusedSurface->toSurfaceLocal(localPos);

    const auto touchResources = d->touchesForClient(d->focusedSurface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_motion(resource->handle, d->seat->timestamp().count(), id, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
    }
}

void TouchInterface::sendUp(qint32 id, quint32 serial)
{
    if (!d->focusedSurface) {
        return;
    }

    const auto touchResources = d->touchesForClient(d->focusedSurface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_up(resource->handle, serial, d->seat->timestamp().count(), id);
    }
}

void TouchInterface::sendDown(qint32 id, quint32 serial, const QPointF &localPos, SurfaceInterface *surface)
{
    if (!surface) {
        return;
    }

    d->focusedSurface = surface;

    QPointF pos = d->focusedSurface->toSurfaceLocal(localPos);

    const auto touchResources = d->touchesForClient(d->focusedSurface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_down(resource->handle,
                     serial,
                     d->seat->timestamp().count(),
                     d->focusedSurface->resource(),
                     id,
                     wl_fixed_from_double(pos.x()),
                     wl_fixed_from_double(pos.y()));
    }
}

} // namespace KWaylandServer
