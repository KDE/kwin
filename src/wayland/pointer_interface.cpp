/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "pointer_interface.h"
#include "pointer_interface_p.h"
#include "pointerconstraints_interface.h"
#include "pointergestures_v1_interface_p.h"
#include "resource_p.h"
#include "relativepointer_v1_interface_p.h"
#include "seat_interface.h"
#include "display.h"
#include "subcompositor_interface.h"
#include "surface_interface.h"
#include "datadevice_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWaylandServer
{

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
        cursor->d->update(QPointer<SurfaceInterface>(surface), serial, hotspot);
        QObject::connect(cursor, &Cursor::changed, q, &PointerInterface::cursorChanged);
        emit q->cursorChanged();
    } else {
        cursor->d->update(QPointer<SurfaceInterface>(surface), serial, hotspot);
    }
}

void PointerInterface::Private::sendLeave(SurfaceInterface *surface, quint32 serial)
{
    if (!surface) {
        return;
    }
    if (resource && surface->resource()) {
        wl_pointer_send_leave(resource, serial, surface->resource());
    }
}

void PointerInterface::Private::registerRelativePointerV1(RelativePointerV1Interface *relativePointer)
{
    Q_ASSERT(!relativePointersV1.contains(relativePointer));
    relativePointersV1.append(relativePointer);
}

void PointerInterface::Private::unregisterRelativePointerV1(RelativePointerV1Interface *relativePointer)
{
    Q_ASSERT(relativePointersV1.contains(relativePointer));
    relativePointersV1.removeOne(relativePointer);
}

void PointerInterface::Private::registerSwipeGestureV1(PointerSwipeGestureV1Interface *gesture)
{
    Q_ASSERT(!swipeGesturesV1.contains(gesture));
    swipeGesturesV1.append(gesture);
}

void PointerInterface::Private::unregisterSwipeGestureV1(PointerSwipeGestureV1Interface *gesture)
{
    Q_ASSERT(swipeGesturesV1.contains(gesture));
    swipeGesturesV1.removeOne(gesture);
}

void PointerInterface::Private::registerPinchGestureV1(PointerPinchGestureV1Interface *gesture)
{
    Q_ASSERT(!pinchGesturesV1.contains(gesture));
    pinchGesturesV1.append(gesture);
}

void PointerInterface::Private::unregisterPinchGestureV1(PointerPinchGestureV1Interface *gesture)
{
    Q_ASSERT(pinchGesturesV1.contains(gesture));
    pinchGesturesV1.removeOne(gesture);
}

namespace {
static QPointF surfacePosition(SurfaceInterface *surface) {
    if (surface && surface->subSurface()) {
        return surface->subSurface()->position() + surfacePosition(surface->subSurface()->parentSurface());
    }
    return QPointF();
}
}

void PointerInterface::Private::sendEnter(SurfaceInterface *surface, const QPointF &parentSurfacePosition, quint32 serial)
{
    if (!surface || !surface->resource()) {
        return;
    }
    const QPointF adjustedPos = parentSurfacePosition - surfacePosition(surface);
    wl_pointer_send_enter(resource, serial,
                          surface->resource(),
                          wl_fixed_from_double(adjustedPos.x()), wl_fixed_from_double(adjustedPos.y()));
}

void PointerInterface::Private::startSwipeGesture(quint32 serial, quint32 fingerCount)
{
    if (!focusedSurface) {
        return;
    }
    for (PointerSwipeGestureV1Interface *gesture : qAsConst(swipeGesturesV1)) {
        gesture->send_begin(serial, seat->timestamp(), focusedSurface->resource(), fingerCount);
    }
}

void PointerInterface::Private::updateSwipeGesture(const QSizeF &delta)
{
    for (PointerSwipeGestureV1Interface *gesture : qAsConst(swipeGesturesV1)) {
        gesture->send_update(seat->timestamp(),
                             wl_fixed_from_double(delta.width()),
                             wl_fixed_from_double(delta.height()));
    }
}

void PointerInterface::Private::endSwipeGesture(quint32 serial)
{
    for (PointerSwipeGestureV1Interface *gesture : qAsConst(swipeGesturesV1)) {
        gesture->send_end(serial, seat->timestamp(), false);
    }
}

void PointerInterface::Private::cancelSwipeGesture(quint32 serial)
{
    for (PointerSwipeGestureV1Interface *gesture : qAsConst(swipeGesturesV1)) {
        gesture->send_end(serial, seat->timestamp(), true);
    }
}

void PointerInterface::Private::startPinchGesture(quint32 serial, quint32 fingerCount)
{
    if (!focusedSurface) {
        return;
    }
    for (PointerPinchGestureV1Interface *gesture : qAsConst(pinchGesturesV1)) {
        gesture->send_begin(serial, seat->timestamp(), focusedSurface->resource(), fingerCount);
    }
}

void PointerInterface::Private::updatePinchGesture(const QSizeF &delta, qreal scale, qreal rotation)
{
    for (PointerPinchGestureV1Interface *gesture : qAsConst(pinchGesturesV1)) {
        gesture->send_update(seat->timestamp(),
                             wl_fixed_from_double(delta.width()),
                             wl_fixed_from_double(delta.height()),
                             wl_fixed_from_double(scale),
                             wl_fixed_from_double(rotation));
    }
}

void PointerInterface::Private::endPinchGesture(quint32 serial)
{
    for (PointerPinchGestureV1Interface *gesture : qAsConst(pinchGesturesV1)) {
        gesture->send_end(serial, seat->timestamp(), false);
    }
}

void PointerInterface::Private::cancelPinchGesture(quint32 serial)
{
    for (PointerPinchGestureV1Interface *gesture : qAsConst(pinchGesturesV1)) {
        gesture->send_end(serial, seat->timestamp(), true);
    }
}

void PointerInterface::Private::sendFrame()
{
    if (!resource || wl_resource_get_version(resource) < WL_POINTER_FRAME_SINCE_VERSION) {
        return;
    }
    wl_pointer_send_frame(resource);
}

#ifndef K_DOXYGEN
const struct wl_pointer_interface PointerInterface::Private::s_interface = {
    setCursorCallback,
    resourceDestroyedCallback
};
#endif

PointerInterface::PointerInterface(SeatInterface *parent, wl_resource *parentResource)
    : Resource(new Private(parent, parentResource, this))
{
    // TODO: handle touch
    connect(parent, &SeatInterface::pointerPosChanged, this, [this] {
        Q_D();
        if (!d->focusedSurface || !d->resource) {
            return;
        }
        if (d->seat->isDragPointer()) {
            const auto *originSurface = d->seat->dragSource()->origin();
            const bool proxyRemoteFocused = originSurface->dataProxy() && originSurface == d->focusedSurface;
            if (!proxyRemoteFocused) {
                // handled by DataDevice
                return;
            }
        }
        if (!d->focusedSurface->lockedPointer().isNull() && d->focusedSurface->lockedPointer()->isLocked()) {
            return;
        }
        const QPointF pos = d->seat->focusedPointerSurfaceTransformation().map(d->seat->pointerPos());
        auto targetSurface = d->focusedSurface->inputSurfaceAt(pos);
        if (!targetSurface) {
            targetSurface = d->focusedSurface;
        }
        if (targetSurface != d->focusedChildSurface.data()) {
            const quint32 serial = d->seat->display()->nextSerial();
            d->sendLeave(d->focusedChildSurface.data(), serial);
            d->focusedChildSurface = QPointer<SurfaceInterface>(targetSurface);
            d->sendEnter(targetSurface, pos, serial);
            d->sendFrame();
            d->client->flush();
        } else {
            const QPointF adjustedPos = pos - surfacePosition(d->focusedChildSurface);
            wl_pointer_send_motion(d->resource, d->seat->timestamp(),
                                   wl_fixed_from_double(adjustedPos.x()), wl_fixed_from_double(adjustedPos.y()));
            d->sendFrame();
        }
    });
}

PointerInterface::~PointerInterface() = default;

void PointerInterface::setFocusedSurface(SurfaceInterface *surface, quint32 serial)
{
    Q_D();
    d->sendLeave(d->focusedChildSurface.data(), serial);
    disconnect(d->destroyConnection);
    if (!surface) {
        d->focusedSurface = nullptr;
        d->focusedChildSurface.clear();
        return;
    }
    d->focusedSurface = surface;
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this,
        [this] {
            Q_D();
            d->sendLeave(d->focusedChildSurface.data(), d->global->display()->nextSerial());
            d->sendFrame();
            d->focusedSurface = nullptr;
            d->focusedChildSurface.clear();
        }
    );

    const QPointF pos = d->seat->focusedPointerSurfaceTransformation().map(d->seat->pointerPos());
    d->focusedChildSurface = QPointer<SurfaceInterface>(d->focusedSurface->inputSurfaceAt(pos));
    if (!d->focusedChildSurface) {
        d->focusedChildSurface = QPointer<SurfaceInterface>(d->focusedSurface);
    }
    d->sendEnter(d->focusedChildSurface.data(), pos, serial);
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
    d->sendFrame();
}

void PointerInterface::buttonReleased(quint32 button, quint32 serial)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    if (!d->resource) {
        return;
    }
    wl_pointer_send_button(d->resource, serial, d->seat->timestamp(), button, WL_POINTER_BUTTON_STATE_RELEASED);
    d->sendFrame();
}

void PointerInterface::axis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source)
{
    Q_D();
    Q_ASSERT(d->focusedSurface);
    if (!d->resource) {
        return;
    }

    const quint32 version = wl_resource_get_version(d->resource);

    const auto wlOrientation = (orientation == Qt::Vertical)
        ? WL_POINTER_AXIS_VERTICAL_SCROLL
        : WL_POINTER_AXIS_HORIZONTAL_SCROLL;

    if (source != PointerAxisSource::Unknown && version >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
        wl_pointer_axis_source wlSource;
        switch (source) {
        case PointerAxisSource::Wheel:
            wlSource = WL_POINTER_AXIS_SOURCE_WHEEL;
            break;
        case PointerAxisSource::Finger:
            wlSource = WL_POINTER_AXIS_SOURCE_FINGER;
            break;
        case PointerAxisSource::Continuous:
            wlSource = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
            break;
        case PointerAxisSource::WheelTilt:
            wlSource = WL_POINTER_AXIS_SOURCE_WHEEL_TILT;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        wl_pointer_send_axis_source(d->resource, wlSource);
    }

    if (delta != 0.0) {
        if (discreteDelta && version >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION) {
            wl_pointer_send_axis_discrete(d->resource, wlOrientation, discreteDelta);
        }
        wl_pointer_send_axis(d->resource, d->seat->timestamp(), wlOrientation, wl_fixed_from_double(delta));
    } else if (version >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
        wl_pointer_send_axis_stop(d->resource, d->seat->timestamp(), wlOrientation);
    }

    d->sendFrame();
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
    d->sendFrame();
}

void PointerInterface::Private::setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                         wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y)
{
    auto p = cast<Private>(resource);
    Q_ASSERT(p->client->client() == client);
    p->setCursor(serial, SurfaceInterface::get(surface), QPoint(hotspot_x, hotspot_y));
}

Cursor *PointerInterface::cursor() const
{
    Q_D();
    return d->cursor;
}

void PointerInterface::relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    Q_D();
    if (d->relativePointersV1.isEmpty()) {
        return;
    }
    for (RelativePointerV1Interface *relativePointer : qAsConst(d->relativePointersV1)) {
        relativePointer->send_relative_motion(microseconds >> 32, microseconds & 0xffffffff,
                                              wl_fixed_from_double(delta.width()),
                                              wl_fixed_from_double(delta.height()),
                                              wl_fixed_from_double(deltaNonAccelerated.width()),
                                              wl_fixed_from_double(deltaNonAccelerated.height()));
    }
    d->sendFrame();
}

PointerInterface::Private *PointerInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

PointerInterface *PointerInterface::get(wl_resource *native)
{
    return Private::get<PointerInterface>(native);
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
