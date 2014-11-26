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
#include "seat_interface.h"
#include "global_p.h"
#include "display.h"
#include "keyboard_interface.h"
#include "pointer_interface.h"
#include "surface_interface.h"
// Qt
#include <QHash>
#include <QVector>
// Wayland
#include <wayland-server.h>
#ifndef WL_SEAT_NAME_SINCE_VERSION
#define WL_SEAT_NAME_SINCE_VERSION 2
#endif

namespace KWayland
{

namespace Server
{

static const quint32 s_version = 3;

class SeatInterface::Private : public Global::Private
{
public:
    Private(SeatInterface *q, Display *d);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void sendCapabilities(wl_resource *r);
    void sendName(wl_resource *r);
    PointerInterface *pointerForSurface(SurfaceInterface *surface) const;

    QString name;
    bool pointer = false;
    bool keyboard = false;
    bool touch = false;
    QList<wl_resource*> resources;
    struct FocusedPointer {
        SurfaceInterface *surface = nullptr;
        PointerInterface *pointer = nullptr;
        QMetaObject::Connection destroyConnection;
        QPointF offset = QPointF();
        quint32 serial = 0;
    };
    FocusedPointer focusedPointer = FocusedPointer();
    KeyboardInterface *keyboardInterface = nullptr;
    quint32 timestamp = 0;
    QVector<PointerInterface*> pointers;

    // Pointer related members
    QPointF pointerPos;

    static SeatInterface *get(wl_resource *native) {
        auto s = cast(native);
        return s ? s->q : nullptr;
    }

private:
    void getPointer(wl_client *client, wl_resource *resource, uint32_t id);
    static Private *cast(wl_resource *r);
    static void unbind(wl_resource *r);

    // interface
    static void getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static void getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id);
    static const struct wl_seat_interface s_interface;

    SeatInterface *q;
};

SeatInterface::Private::Private(SeatInterface *q, Display *display)
    : Global::Private(display, &wl_seat_interface, s_version)
    , q(q)
{
}

const struct wl_seat_interface SeatInterface::Private::s_interface = {
    getPointerCallback,
    getKeyboardCallback,
    getTouchCallback
};

SeatInterface::SeatInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
    Q_D();
    d->keyboardInterface = new KeyboardInterface(this);
    connect(this, &SeatInterface::nameChanged, this,
        [this, d] {
            for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
                d->sendName(*it);
            }
        }
    );
    auto sendCapabilitiesAll = [this, d] {
        for (auto it = d->resources.constBegin(); it != d->resources.constEnd(); ++it) {
            d->sendCapabilities(*it);
        }
    };
    connect(this, &SeatInterface::hasPointerChanged,  this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasKeyboardChanged, this, sendCapabilitiesAll);
    connect(this, &SeatInterface::hasTouchChanged,    this, sendCapabilitiesAll);
}

SeatInterface::~SeatInterface()
{
    Q_D();
    while (!d->resources.isEmpty()) {
        wl_resource_destroy(d->resources.takeLast());
    }
}

void SeatInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    wl_resource *r = wl_resource_create(client, &wl_seat_interface, qMin(s_version, version), id);
    if (!r) {
        wl_client_post_no_memory(client);
        return;
    }
    resources << r;

    wl_resource_set_implementation(r, &s_interface, this, unbind);

    sendCapabilities(r);
    sendName(r);
}

void SeatInterface::Private::unbind(wl_resource *r)
{
    cast(r)->resources.removeAll(r);
}

void SeatInterface::Private::sendName(wl_resource *r)
{
    if (wl_resource_get_version(r) < WL_SEAT_NAME_SINCE_VERSION) {
        return;
    }
    wl_seat_send_name(r, name.toUtf8().constData());
}

void SeatInterface::Private::sendCapabilities(wl_resource *r)
{
    uint32_t capabilities = 0;
    if (pointer) {
        capabilities |= WL_SEAT_CAPABILITY_POINTER;
    }
    if (keyboard) {
        capabilities |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (touch) {
        capabilities |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wl_seat_send_capabilities(r, capabilities);
}

SeatInterface::Private *SeatInterface::Private::cast(wl_resource *r)
{
    return r ? reinterpret_cast<SeatInterface::Private*>(wl_resource_get_user_data(r)) : nullptr;
}

PointerInterface *SeatInterface::Private::pointerForSurface(SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }

    for (auto it = pointers.begin(); it != pointers.end(); ++it) {
        if (wl_resource_get_client((*it)->resource()) == *surface->client()) {
            return (*it);
        }
    }
    return nullptr;
}

void SeatInterface::setHasKeyboard(bool has)
{
    Q_D();
    if (d->keyboard == has) {
        return;
    }
    d->keyboard = has;
    emit hasKeyboardChanged(d->keyboard);
}

void SeatInterface::setHasPointer(bool has)
{
    Q_D();
    if (d->pointer == has) {
        return;
    }
    d->pointer = has;
    emit hasPointerChanged(d->pointer);
}

void SeatInterface::setHasTouch(bool has)
{
    Q_D();
    if (d->touch == has) {
        return;
    }
    d->touch = has;
    emit hasTouchChanged(d->touch);
}

void SeatInterface::setName(const QString &name)
{
    Q_D();
    if (d->name == name) {
        return;
    }
    d->name = name;
    emit nameChanged(d->name);
}

void SeatInterface::Private::getPointerCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->getPointer(client, resource, id);
}

void SeatInterface::Private::getPointer(wl_client *client, wl_resource *resource, uint32_t id)
{
    // TODO: only create if seat has pointer?
    PointerInterface *pointer = new PointerInterface(q, resource);
    pointer->create(display->getConnection(client), wl_resource_get_version(resource), id);
    if (!pointer->resource()) {
        wl_resource_post_no_memory(resource);
        delete pointer;
        return;
    }
    pointers << pointer;
    if (focusedPointer.surface && focusedPointer.surface->client()->client() == client) {
        // this is a pointer for the currently focused pointer surface
        if (!focusedPointer.pointer) {
            focusedPointer.pointer = pointer;
            pointer->setFocusedSurface(focusedPointer.surface, focusedPointer.serial);
        }
    }
    QObject::connect(pointer, &QObject::destroyed, q,
        [pointer,this] {
            pointers.removeAt(pointers.indexOf(pointer));
            if (focusedPointer.pointer == pointer) {
                focusedPointer.pointer = nullptr;
            }
        }
    );
    emit q->pointerCreated(pointer);
}

void SeatInterface::Private::getKeyboardCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    cast(resource)->keyboardInterface->createInterfae(client, resource, id);
}

void SeatInterface::Private::getTouchCallback(wl_client *client, wl_resource *resource, uint32_t id)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(id)
}

QString SeatInterface::name() const
{
    Q_D();
    return d->name;
}

bool SeatInterface::hasPointer() const
{
    Q_D();
    return d->pointer;
}

bool SeatInterface::hasKeyboard() const
{
    Q_D();
    return d->keyboard;
}

bool SeatInterface::hasTouch() const
{
    Q_D();
    return d->touch;
}

KeyboardInterface *SeatInterface::keyboard()
{
    Q_D();
    return d->keyboardInterface;
}

SeatInterface *SeatInterface::get(wl_resource *native)
{
    return Private::get(native);
}

SeatInterface::Private *SeatInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QPointF SeatInterface::pointerPos() const
{
    Q_D();
    return d->pointerPos;
}

void SeatInterface::setPointerPos(const QPointF &pos)
{
    Q_D();
    if (d->pointerPos == pos) {
        return;
    }
    d->pointerPos = pos;
    emit pointerPosChanged(pos);
}

quint32 SeatInterface::timestamp() const
{
    Q_D();
    return d->timestamp;
}

void SeatInterface::setTimestamp(quint32 time)
{
    Q_D();
    if (d->timestamp == time) {
        return;
    }
    d->timestamp = time;
    emit timestampChanged(time);
}

SurfaceInterface *SeatInterface::focusedPointerSurface() const
{
    Q_D();
    return d->focusedPointer.surface;
}

void SeatInterface::setFocusedPointerSurface(SurfaceInterface *surface, const QPointF &surfacePosition)
{
    Q_D();
    const quint32 serial = d->display->nextSerial();
    if (d->focusedPointer.pointer) {
        d->focusedPointer.pointer->setFocusedSurface(nullptr, serial);
    }
    if (d->focusedPointer.surface) {
        disconnect(d->focusedPointer.destroyConnection);
    }
    d->focusedPointer = Private::FocusedPointer();
    d->focusedPointer.surface = surface;
    PointerInterface *p = d->pointerForSurface(surface);
    d->focusedPointer.pointer = p;
    if (d->focusedPointer.surface) {
        d->focusedPointer.destroyConnection = connect(surface, &QObject::destroyed, this,
            [this] {
                Q_D();
                d->focusedPointer = Private::FocusedPointer();
            }
        );
        d->focusedPointer.offset = surfacePosition;
        d->focusedPointer.serial = serial;
    }
    if (!p) {
        return;
    }
    p->setFocusedSurface(surface, serial);
}

PointerInterface *SeatInterface::focusedPointer() const
{
    Q_D();
    return d->focusedPointer.pointer;
}

void SeatInterface::setFocusedPointerSurfacePosition(const QPointF &surfacePosition)
{
    Q_D();
    if (d->focusedPointer.surface) {
        d->focusedPointer.offset = surfacePosition;
    }
}

QPointF SeatInterface::focusedPointerSurfacePosition() const
{
    Q_D();
    return d->focusedPointer.offset;
}

}
}
