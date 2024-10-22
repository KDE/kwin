/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "touch.h"
#include "clientconnection.h"
#include "display.h"
#include "seat.h"
#include "surface.h"
#include "touch_p.h"

namespace KWin
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

TouchInterface::TouchInterface(SeatInterface *seat)
    : d(new TouchInterfacePrivate(this, seat))
{
}

TouchInterface::~TouchInterface()
{
}

void TouchInterface::sendCancel(SurfaceInterface *surface)
{
    if (!surface) {
        return;
    }

    const auto touchResources = d->touchesForClient(surface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_cancel(resource->handle);
    }
}

void TouchInterface::sendFrame()
{
    for (auto client : std::as_const(d->m_clientsInFrame)) {
        if (!client) {
            continue;
        }
        const auto touchResources = d->touchesForClient(client);
        for (TouchInterfacePrivate::Resource *resource : touchResources) {
            d->send_frame(resource->handle);
        }
    }
    d->m_clientsInFrame.clear();
}

void TouchInterface::sendMotion(SurfaceInterface *surface, qint32 id, const QPointF &localPos, std::chrono::milliseconds time)
{
    if (!surface) {
        return;
    }

    QPointF pos = surface->toSurfaceLocal(localPos);

    const auto touchResources = d->touchesForClient(surface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_motion(resource->handle, time.count(), id, wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
    }
    addToFrame(surface->client());
}

void TouchInterface::sendUp(ClientConnection *client, qint32 id, quint32 serial, std::chrono::milliseconds time)
{
    if (!client) {
        return;
    }

    const auto touchResources = d->touchesForClient(client);
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_up(resource->handle, serial, time.count(), id);
    }
    addToFrame(client);
}

void TouchInterface::sendDown(SurfaceInterface *surface, qint32 id, quint32 serial, std::chrono::milliseconds time, const QPointF &localPos)
{
    if (!surface) {
        return;
    }

    const QPointF pos = surface->toSurfaceLocal(localPos);
    const auto touchResources = d->touchesForClient(surface->client());
    for (TouchInterfacePrivate::Resource *resource : touchResources) {
        d->send_down(resource->handle,
                     serial,
                     time.count(),
                     surface->resource(),
                     id,
                     wl_fixed_from_double(pos.x()),
                     wl_fixed_from_double(pos.y()));
    }
    addToFrame(surface->client());
}

void TouchInterface::addToFrame(ClientConnection *client)
{
    if (!d->m_clientsInFrame.contains(client)) {
        d->m_clientsInFrame.append(client);
    }
}

} // namespace KWin

#include "moc_touch.cpp"
