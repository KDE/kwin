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
#include "pointer_interface_p.h"
#include "pointerconstraints_interface.h"
#include "pointergestures_interface_p.h"
#include "resource_p.h"
#include "relativepointer_interface_p.h"
#include "seat_interface.h"
#include "display.h"
#include "subcompositor_interface.h"
#include "surface_interface.h"
// Wayland
#include <wayland-server.h>

namespace KWayland
{

namespace Server
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

void PointerInterface::Private::registerRelativePointer(RelativePointerInterface *relativePointer)
{
    relativePointers << relativePointer;
    QObject::connect(relativePointer, &QObject::destroyed, q,
        [this, relativePointer] {
            relativePointers.removeOne(relativePointer);
        }
    );
}


void PointerInterface::Private::registerSwipeGesture(PointerSwipeGestureInterface *gesture)
{
    swipeGestures << gesture;
    QObject::connect(gesture, &QObject::destroyed, q,
        [this, gesture] {
            swipeGestures.removeOne(gesture);
        }
    );
}

void PointerInterface::Private::registerPinchGesture(PointerPinchGestureInterface *gesture)
{
    pinchGestures << gesture;
    QObject::connect(gesture, &QObject::destroyed, q,
        [this, gesture] {
            pinchGestures.removeOne(gesture);
        }
    );
}

namespace {
static QPointF surfacePosition(SurfaceInterface *surface) {
    if (surface && surface->subSurface()) {
        return surface->subSurface()->position() + surfacePosition(surface->subSurface()->parentSurface().data());
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
    if (swipeGestures.isEmpty()) {
        return;
    }
    for (auto it = swipeGestures.constBegin(), end = swipeGestures.constEnd(); it != end; it++) {
        (*it)->start(serial, fingerCount);
    }
}

void PointerInterface::Private::updateSwipeGesture(const QSizeF &delta)
{
    if (swipeGestures.isEmpty()) {
        return;
    }
    for (auto it = swipeGestures.constBegin(), end = swipeGestures.constEnd(); it != end; it++) {
        (*it)->update(delta);
    }
}

void PointerInterface::Private::endSwipeGesture(quint32 serial)
{
    if (swipeGestures.isEmpty()) {
        return;
    }
    for (auto it = swipeGestures.constBegin(), end = swipeGestures.constEnd(); it != end; it++) {
        (*it)->end(serial);
    }
}

void PointerInterface::Private::cancelSwipeGesture(quint32 serial)
{
    if (swipeGestures.isEmpty()) {
        return;
    }
    for (auto it = swipeGestures.constBegin(), end = swipeGestures.constEnd(); it != end; it++) {
        (*it)->cancel(serial);
    }
}

void PointerInterface::Private::startPinchGesture(quint32 serial, quint32 fingerCount)
{
    if (pinchGestures.isEmpty()) {
        return;
    }
    for (auto it = pinchGestures.constBegin(), end = pinchGestures.constEnd(); it != end; it++) {
        (*it)->start(serial, fingerCount);
    }
}

void PointerInterface::Private::updatePinchGesture(const QSizeF &delta, qreal scale, qreal rotation)
{
    if (pinchGestures.isEmpty()) {
        return;
    }
    for (auto it = pinchGestures.constBegin(), end = pinchGestures.constEnd(); it != end; it++) {
        (*it)->update(delta, scale, rotation);
    }
}

void PointerInterface::Private::endPinchGesture(quint32 serial)
{
    if (pinchGestures.isEmpty()) {
        return;
    }
    for (auto it = pinchGestures.constBegin(), end = pinchGestures.constEnd(); it != end; it++) {
        (*it)->end(serial);
    }
}

void PointerInterface::Private::cancelPinchGesture(quint32 serial)
{
    if (pinchGestures.isEmpty()) {
        return;
    }
    for (auto it = pinchGestures.constBegin(), end = pinchGestures.constEnd(); it != end; it++) {
        (*it)->cancel(serial);
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
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
        if (d->seat->isDragPointer()) {
            // handled by DataDevice
            return;
        }
        if (d->focusedSurface && d->resource) {
            if (!d->focusedSurface->lockedPointer().isNull() && d->focusedSurface->lockedPointer()->isLocked()) {
                return;
            }
            const QPointF pos = d->seat->focusedPointerSurfaceTransformation().map(d->seat->pointerPos());
            auto targetSurface = d->focusedSurface->surfaceAt(pos);
            if (!targetSurface) {
                targetSurface = d->focusedSurface;
            }
            if (targetSurface != d->focusedChildSurface.data()) {
                const quint32 serial = d->seat->display()->nextSerial();
                d->sendLeave(d->focusedChildSurface.data(), serial);
                d->focusedChildSurface = QPointer<SurfaceInterface>(targetSurface);
                d->sendEnter(targetSurface, pos, serial);
                d->client->flush();
            } else {
                const QPointF adjustedPos = pos - surfacePosition(d->focusedChildSurface);
                wl_pointer_send_motion(d->resource, d->seat->timestamp(),
                                       wl_fixed_from_double(adjustedPos.x()), wl_fixed_from_double(adjustedPos.y()));
            }
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
    d->destroyConnection = connect(d->focusedSurface, &Resource::aboutToBeUnbound, this,
        [this] {
            Q_D();
            d->sendLeave(d->focusedChildSurface.data(), d->global->display()->nextSerial());
            d->focusedSurface = nullptr;
            d->focusedChildSurface.clear();
        }
    );

    const QPointF pos = d->seat->focusedPointerSurfaceTransformation().map(d->seat->pointerPos());
    d->focusedChildSurface = QPointer<SurfaceInterface>(d->focusedSurface->surfaceAt(pos));
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

Cursor *PointerInterface::cursor() const
{
    Q_D();
    return d->cursor;
}

void PointerInterface::relativeMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 microseconds)
{
    Q_D();
    if (d->relativePointers.isEmpty()) {
        return;
    }
    for (auto it = d->relativePointers.constBegin(), end = d->relativePointers.constEnd(); it != end; it++) {
        (*it)->relativeMotion(delta, deltaNonAccelerated, microseconds);
    }
    client()->flush();
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
}
