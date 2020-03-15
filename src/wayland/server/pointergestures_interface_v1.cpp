/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointergestures_interface_p.h"
#include "display.h"
#include "pointer_interface_p.h"
#include "resource_p.h"
#include "seat_interface.h"
#include "surface_interface.h"

#include <wayland-pointer-gestures-unstable-v1-server-protocol.h>

namespace KWayland
{
namespace Server
{

class PointerGesturesUnstableV1Interface::Private : public PointerGesturesInterface::Private
{
public:
    Private(PointerGesturesUnstableV1Interface *q, Display *d);

private:
    void bind(wl_client *client, uint32_t version, uint32_t id) override;

    static void unbind(wl_resource *resource);
    static Private *cast(wl_resource *r) {
        return reinterpret_cast<Private*>(wl_resource_get_user_data(r));
    }

    static void getSwipeGestureCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * pointer);
    static void getPinchGestureCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * pointer);

    PointerGesturesUnstableV1Interface *q;
    static const struct zwp_pointer_gestures_v1_interface s_interface;
    static const quint32 s_version;
};

const quint32 PointerGesturesUnstableV1Interface::Private::s_version = 1;

#ifndef K_DOXYGEN
const struct zwp_pointer_gestures_v1_interface PointerGesturesUnstableV1Interface::Private::s_interface = {
    getSwipeGestureCallback,
    getPinchGestureCallback
};
#endif

void PointerGesturesUnstableV1Interface::Private::getSwipeGestureCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *pointer)
{
    PointerInterface *p = PointerInterface::get(pointer);
    if (!p) {
        // TODO: raise error?
        return;
    }
    auto m = cast(resource);
    auto *g = new PointerSwipeGestureUnstableV1Interface(m->q, resource, p);
    g->d->create(m->display->getConnection(client), version, id);
    p->d_func()->registerSwipeGesture(g);
}

void PointerGesturesUnstableV1Interface::Private::getPinchGestureCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource * pointer)
{
    PointerInterface *p = PointerInterface::get(pointer);
    if (!p) {
        // TODO: raise error?
        return;
    }
    auto m = cast(resource);
    auto *g = new PointerPinchGestureUnstableV1Interface(m->q, resource, p);
    g->d->create(m->display->getConnection(client), version, id);
    p->d_func()->registerPinchGesture(g);
}

PointerGesturesUnstableV1Interface::Private::Private(PointerGesturesUnstableV1Interface *q, Display *d)
    : PointerGesturesInterface::Private(PointerGesturesInterfaceVersion::UnstableV1, q, d, &zwp_pointer_gestures_v1_interface, s_version)
    , q(q)
{
}

void PointerGesturesUnstableV1Interface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *resource = c->createResource(&zwp_pointer_gestures_v1_interface, qMin(version, s_version), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &s_interface, this, unbind);
    // TODO: should we track?
}

void PointerGesturesUnstableV1Interface::Private::unbind(wl_resource *resource)
{
    Q_UNUSED(resource)
    // TODO: implement?
}

PointerGesturesUnstableV1Interface::PointerGesturesUnstableV1Interface(Display *display, QObject *parent)
    : PointerGesturesInterface(new Private(this, display), parent)
{
}

PointerGesturesUnstableV1Interface::~PointerGesturesUnstableV1Interface() = default;

class PointerSwipeGestureUnstableV1Interface::Private : public PointerSwipeGestureInterface::Private
{
public:
    Private(PointerSwipeGestureUnstableV1Interface *q, PointerGesturesUnstableV1Interface *c, wl_resource *parentResource, PointerInterface *pointer);
    ~Private();

    void end(quint32 serial, bool end);

private:

    PointerSwipeGestureUnstableV1Interface *q_func() {
        return reinterpret_cast<PointerSwipeGestureUnstableV1Interface *>(q);
    }

    static const struct zwp_pointer_gesture_swipe_v1_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zwp_pointer_gesture_swipe_v1_interface PointerSwipeGestureUnstableV1Interface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

PointerSwipeGestureUnstableV1Interface::Private::Private(PointerSwipeGestureUnstableV1Interface *q, PointerGesturesUnstableV1Interface *c, wl_resource *parentResource, PointerInterface *pointer)
    : PointerSwipeGestureInterface::Private(q, c, parentResource, &zwp_pointer_gesture_swipe_v1_interface, &s_interface, pointer)
{
}

PointerSwipeGestureUnstableV1Interface::Private::~Private() = default;

PointerSwipeGestureUnstableV1Interface::PointerSwipeGestureUnstableV1Interface(PointerGesturesUnstableV1Interface *parent, wl_resource *parentResource, PointerInterface *pointer)
    : PointerSwipeGestureInterface(new Private(this, parent, parentResource, pointer))
{
}

PointerSwipeGestureUnstableV1Interface::~PointerSwipeGestureUnstableV1Interface() = default;

void PointerSwipeGestureUnstableV1Interface::start(quint32 serial, quint32 fingerCount)
{
    Q_D();
    SeatInterface *seat = qobject_cast<SeatInterface*>(d->pointer->global());
    if (!seat) {
        return;
    }
    if (!seat->focusedPointerSurface()) {
        return;
    }
    zwp_pointer_gesture_swipe_v1_send_begin(resource(), serial, seat->timestamp(), seat->focusedPointerSurface()->resource(), fingerCount);
}

void PointerSwipeGestureUnstableV1Interface::update(const QSizeF &delta)
{
    Q_D();
    SeatInterface *seat = qobject_cast<SeatInterface*>(d->pointer->global());
    if (!seat) {
        return;
    }
    zwp_pointer_gesture_swipe_v1_send_update(resource(), seat->timestamp(),
                                             wl_fixed_from_double(delta.width()), wl_fixed_from_double(delta.height()));
}

void PointerSwipeGestureUnstableV1Interface::Private::end(quint32 serial, bool cancel)
{
    SeatInterface *seat = qobject_cast<SeatInterface*>(pointer->global());
    if (!seat) {
        return;
    }
    zwp_pointer_gesture_swipe_v1_send_end(resource, serial, seat->timestamp(), uint32_t(cancel));
}

void PointerSwipeGestureUnstableV1Interface::end(quint32 serial)
{
    Q_D();
    d->end(serial, false);
}

void PointerSwipeGestureUnstableV1Interface::cancel(quint32 serial)
{
    Q_D();
    d->end(serial, true);
}

PointerSwipeGestureUnstableV1Interface::Private *PointerSwipeGestureUnstableV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

class PointerPinchGestureUnstableV1Interface::Private : public PointerPinchGestureInterface::Private
{
public:
    Private(PointerPinchGestureUnstableV1Interface *q, PointerGesturesUnstableV1Interface *c, wl_resource *parentResource, PointerInterface *pointer);
    ~Private();

    void end(quint32 serial, bool end);

private:

    PointerPinchGestureUnstableV1Interface *q_func() {
        return reinterpret_cast<PointerPinchGestureUnstableV1Interface *>(q);
    }

    static const struct zwp_pointer_gesture_pinch_v1_interface s_interface;
};

#ifndef K_DOXYGEN
const struct zwp_pointer_gesture_pinch_v1_interface PointerPinchGestureUnstableV1Interface::Private::s_interface = {
    resourceDestroyedCallback
};
#endif

PointerPinchGestureUnstableV1Interface::Private::Private(PointerPinchGestureUnstableV1Interface *q, PointerGesturesUnstableV1Interface *c, wl_resource *parentResource, PointerInterface *pointer)
    : PointerPinchGestureInterface::Private(q, c, parentResource, &zwp_pointer_gesture_pinch_v1_interface, &s_interface, pointer)
{
}

PointerPinchGestureUnstableV1Interface::Private::~Private() = default;

PointerPinchGestureUnstableV1Interface::PointerPinchGestureUnstableV1Interface(PointerGesturesUnstableV1Interface *parent, wl_resource *parentResource, PointerInterface *pointer)
    : PointerPinchGestureInterface(new Private(this, parent, parentResource, pointer))
{
}

PointerPinchGestureUnstableV1Interface::~PointerPinchGestureUnstableV1Interface() = default;

void PointerPinchGestureUnstableV1Interface::start(quint32 serial, quint32 fingerCount)
{
    Q_D();
    SeatInterface *seat = qobject_cast<SeatInterface*>(d->pointer->global());
    if (!seat) {
        return;
    }
    if (!seat->focusedPointerSurface()) {
        return;
    }
    zwp_pointer_gesture_pinch_v1_send_begin(resource(), serial, seat->timestamp(), seat->focusedPointerSurface()->resource(), fingerCount);
}

void PointerPinchGestureUnstableV1Interface::update(const QSizeF &delta, qreal scale, qreal rotation)
{
    Q_D();
    SeatInterface *seat = qobject_cast<SeatInterface*>(d->pointer->global());
    if (!seat) {
        return;
    }
    zwp_pointer_gesture_pinch_v1_send_update(resource(), seat->timestamp(),
                                             wl_fixed_from_double(delta.width()), wl_fixed_from_double(delta.height()),
                                             wl_fixed_from_double(scale), wl_fixed_from_double(rotation));
}

void PointerPinchGestureUnstableV1Interface::Private::end(quint32 serial, bool cancel)
{
    SeatInterface *seat = qobject_cast<SeatInterface*>(pointer->global());
    if (!seat) {
        return;
    }
    zwp_pointer_gesture_pinch_v1_send_end(resource, serial, seat->timestamp(), uint32_t(cancel));
}

void PointerPinchGestureUnstableV1Interface::end(quint32 serial)
{
    Q_D();
    d->end(serial, false);
}

void PointerPinchGestureUnstableV1Interface::cancel(quint32 serial)
{
    Q_D();
    d->end(serial, true);
}

PointerPinchGestureUnstableV1Interface::Private *PointerPinchGestureUnstableV1Interface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

}
}
