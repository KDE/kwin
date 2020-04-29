/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "touch_interface.h"
#include "resource_p.h"
#include "seat_interface.h"
#include "display.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
{

class TouchInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *parent, wl_resource *parentResource, TouchInterface *q);

    SeatInterface *seat;
    QMetaObject::Connection destroyConnection;

private:
    TouchInterface *q_func() {
        return reinterpret_cast<TouchInterface *>(q);
    }

    static const struct wl_touch_interface s_interface;
};

#ifndef K_DOXYGEN
const struct wl_touch_interface TouchInterface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

TouchInterface::Private::Private(SeatInterface *parent, wl_resource *parentResource, TouchInterface *q)
    : Resource::Private(q, parent, parentResource, &wl_touch_interface, &s_interface)
    , seat(parent)
{
}

TouchInterface::TouchInterface(SeatInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, parentResource, this))
{
}

TouchInterface::~TouchInterface() = default;

void TouchInterface::cancel()
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_touch_send_cancel(d->resource);
    d->client->flush();
}

void TouchInterface::frame()
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_touch_send_frame(d->resource);
    d->client->flush();
}

void TouchInterface::move(qint32 id, const QPointF &localPos)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    if (d->seat->isDragTouch()) {
        // handled by DataDevice
        return;
    }
    wl_touch_send_motion(d->resource, d->seat->timestamp(), id, wl_fixed_from_double(localPos.x()), wl_fixed_from_double(localPos.y()));
    d->client->flush();
}

void TouchInterface::up(qint32 id, quint32 serial)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_touch_send_up(d->resource, serial, d->seat->timestamp(), id);
    d->client->flush();
}

void TouchInterface::down(qint32 id, quint32 serial, const QPointF &localPos)
{
    Q_D();
    if (!d->resource) {
        return;
    }
    wl_touch_send_down(d->resource, serial, d->seat->timestamp(), d->seat->focusedTouchSurface()->resource(),
                       id, wl_fixed_from_double(localPos.x()), wl_fixed_from_double(localPos.y()));
    d->client->flush();
}

TouchInterface::Private *TouchInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
