/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "pointer_interface.h"
#include "resource_p.h"
#include "seat_interface.h"
#include "display.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{

namespace Server
{

class PointerInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QMetaObject::Connection destroyConnection;
    Cursor *cursor = nullptr;

private:
    PointerInterface *q_func() {
        return reinterpret_cast<PointerInterface *>(q);
    }
    void setCursor(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot);
    // interface
    static void setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                  wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y);
    // since version 3
    static void releaseCallback(wl_client *client, wl_resource *resource);

    static const struct wl_pointer_interface s_interface;
};

class Cursor::Private
{
public:
    Private(Cursor *q, PointerInterface *pointer);
    PointerInterface *pointer;
    quint32 enteredSerial = 0;
    QPoint hotspot;
    QPointer<SurfaceInterface> surface;

    void update(const QPointer<SurfaceInterface> &surface, quint32 serial, const QPoint &hotspot);

private:
    Cursor *q;
};

PointerInterface::Private::Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q)
    : Resource::Private(q, parent, parentResource, &wl_pointer_interface, &s_interface)
    , seat(parent)
{
}

void PointerInterface::Private::setCursor(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot)
{
    if (!cursor) {
        Q_Q(PointerInterface);
        cursor = new Cursor(q);
        cursor->d->enteredSerial = serial;
        cursor->d->hotspot = hotspot;
        cursor->d->surface = QPointer<SurfaceInterface>(surface);
        QObject::connect(cursor, &Cursor::changed, q, &PointerInterface::cursorChanged);
        emit q->cursorChanged();
    } else {
        cursor->d->update(QPointer<SurfaceInterface>(surface), serial, hotspot);
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const struct wl_pointer_interface PointerInterface::Private::s_interface = {
    setCursorCallback,
    releaseCallback
};
#endif

PointerInterface::PointerInterface(SeatInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, parentResource, this), parent)
{
    connect(parent, &SeatInterface::pointerPosChanged, this, [this] {
        Q_D();
        if (d->focusedSurface && d->resource) {
            const QPointF pos = d->seat->pointerPos() - d->seat->focusedPointerSurfacePosition();
            wl_pointer_send_motion(d->resource, d->seat->timestamp(),
                                   wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
        }
    });
}

PointerInterface::~PointerInterface() = default;

void PointerInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    Q_D();
    if (d->focusedSurface) {
        if (d->resource && d->focusedSurface->resource()) {
            wl_pointer_send_leave(d->resource, serial, d->focusedSurface->resource());
        }
        disconnect(d->destroyConnection);
    }
    if (!surface) {
        d->focusedSurface = nullptr;
        return;
    }
    d->focusedSurface = surface;
    d->destroyConnection = connect(d->focusedSurface, &QObject::destroyed, this,
        [this] {
            Q_D();
            d->focusedSurface = nullptr;
        }
    );

    const QPointF pos = d->seat->pointerPos() - d->seat->focusedPointerSurfacePosition();
    wl_pointer_send_enter(d->resource, serial,
                          d->focusedSurface->resource(),
                          wl_fixed_from_double(pos.x()), wl_fixed_from_double(pos.y()));
    d->client->flush();
}

void PointerInterface::buttonPressed(quint32 button, quint32 serial)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    if (!d->resource) {
        return;
    }
    wl_pointer_send_button(d->resource, serial, d->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_PRESSED);
}

void PointerInterface::buttonReleased(quint32 button, quint32 serial)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    if (!d->resource) {
        return;
    }
    wl_pointer_send_button(d->resource, serial, d->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
}

void PointerInterface::axis(Qt::Orientation orientation, quint32 delta)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    if (!d->resource) {
        return;
    }
    wl_pointer_send_axis(d->resource, d->seat->timestamp(),
                         (orientation == Qt::Vertical) ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                         wl_fixed_from_int(delta));
}

void PointerInterface::Private::setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                         wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(p->client->client() == client);
    p->setCursor(serial, SurfaceInterface::get(surface), QPoint(hotspot_x, hotspot_y));
}

void PointerInterface::Private::releaseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    unbind(resource);
}

Cursor *PointerInterface::cursor() const
{
    Q_D();
    return d->cursor;
}

PointerInterface::Private *PointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

Cursor::Private::Private(Cursor *q, PointerInterface *pointer)
    : pointer(pointer)
    , q(q)
{
}

void Cursor::Private::update(const QPointer< SurfaceInterface > &s, quint32 serial, const QPoint &p)
{
    bool emitChanged = false;
    if (enteredSerial != serial) {
        enteredSerial = serial;
        emitChanged = true;
        emit q->enteredSerialChanged();
    }
    if (hotspot != p) {
        hotspot = p;
        emitChanged = true;
        emit q->hotspotChanged();
    }
    if (surface != s) {
        if (!surface.isNull()) {
            QObject::disconnect(surface.data(), &SurfaceInterface::damaged, q, &Cursor::changed);
        }
        surface = s;
        if (!surface.isNull()) {
            QObject::connect(surface.data(), &SurfaceInterface::damaged, q, &Cursor::changed);
        }
        emitChanged = true;
        emit q->surfaceChanged();
    }
    if (emitChanged) {
        emit q->changed();
    }
}

Cursor::Cursor(PointerInterface *parent)
    : QObject(parent)
    , d(new Private(this, parent))
{
}

Cursor::~Cursor() = default;

quint32 Cursor::enteredSerial() const
{
    return d->enteredSerial;
}

QPoint Cursor::hotspot() const
{
    return d->hotspot;
}

PointerInterface *Cursor::pointer() const
{
    return d->pointer;
}

QPointer< SurfaceInterface > Cursor::surface() const
{
    return d->surface;
}

}
}
