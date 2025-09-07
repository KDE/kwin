/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointer.h"
#include "clientconnection.h"
#include "display.h"
#include "pointer_p.h"
#include "pointergestures_v1_p.h"
#include "relativepointer_v1_p.h"
#include "seat.h"
#include "surface.h"
#include "utils/common.h"
#include "utils/resource.h"

namespace KWin
{
class PointerSurfaceCursorPrivate
{
public:
    QPointF hotspot;
    QPointer<SurfaceInterface> surface;
};

PointerInterfacePrivate *PointerInterfacePrivate::get(PointerInterface *pointer)
{
    return pointer->d.get();
}

PointerInterfacePrivate::PointerInterfacePrivate(PointerInterface *q, SeatInterface *seat)
    : q(q)
    , seat(seat)
    , relativePointersV1(new RelativePointerV1Interface(q))
    , swipeGesturesV1(new PointerSwipeGestureV1Interface(q))
    , pinchGesturesV1(new PointerPinchGestureV1Interface(q))
    , holdGesturesV1(new PointerHoldGestureV1Interface(q))
{
}

PointerInterfacePrivate::~PointerInterfacePrivate()
{
}

QList<PointerInterfacePrivate::Resource *> PointerInterfacePrivate::pointersForClient(ClientConnection *client) const
{
    return resourceMap().values(client->client());
}

void PointerInterfacePrivate::pointer_set_cursor(Resource *resource, uint32_t serial, ::wl_resource *surface_resource, int32_t hotspot_x, int32_t hotspot_y)
{
    SurfaceInterface *surface = nullptr;

    if (!focusedSurface) {
        return;
    }
    if (focusedSurface->client()->client() != resource->client()) {
        qCDebug(KWIN_CORE, "Denied set_cursor request from unfocused client");
        return;
    }
    if (focusedSerial != serial) {
        return;
    }

    if (surface_resource) {
        surface = SurfaceInterface::get(surface_resource);
        if (!surface) {
            wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid surface");
            return;
        }

        static SurfaceRole cursorRole(QByteArrayLiteral("cursor"));
        if (const SurfaceRole *role = surface->role()) {
            if (role != &cursorRole) {
                wl_resource_post_error(resource->handle, error_role, "the wl_surface already has a role assigned %s", role->name().constData());
                return;
            }
        } else {
            surface->setRole(&cursorRole);
        }
    }

    if (!cursor) {
        cursor = std::make_unique<PointerSurfaceCursor>();
    }
    cursor->d->hotspot = QPointF(hotspot_x, hotspot_y) / focusedSurface->client()->scaleOverride();
    cursor->d->surface = surface;

    Q_EMIT q->cursorChanged(cursor.get());
}

void PointerInterfacePrivate::pointer_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerInterfacePrivate::pointer_bind_resource(Resource *resource)
{
    const ClientConnection *focusedClient = focusedSurface ? focusedSurface->client() : nullptr;

    if (focusedClient && focusedClient->client() == resource->client()) {
        send_enter(resource->handle, focusedSerial, focusedSurface->resource(), wl_fixed_from_double(lastPosition.x()), wl_fixed_from_double(lastPosition.y()));
        if (resource->version() >= WL_POINTER_FRAME_SINCE_VERSION) {
            send_frame(resource->handle);
        }
    }
}

void PointerInterfacePrivate::sendLeave(quint32 serial)
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        send_leave(resource->handle, serial, focusedSurface->resource());
    }
}

void PointerInterfacePrivate::sendEnter(const QPointF &position, quint32 serial)
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        send_enter(resource->handle, serial, focusedSurface->resource(), wl_fixed_from_double(position.x()), wl_fixed_from_double(position.y()));
    }
}

void PointerInterfacePrivate::sendFrame()
{
    const QList<Resource *> pointerResources = pointersForClient(focusedSurface->client());
    for (Resource *resource : pointerResources) {
        if (resource->version() >= WL_POINTER_FRAME_SINCE_VERSION) {
            send_frame(resource->handle);
        }
    }
}

bool PointerInterfacePrivate::AxisAccumulator::Axis::shouldReset(int newDirection, std::chrono::milliseconds newTimestamp) const
{
    if (newTimestamp.count() - timestamp.count() >= 1000) {
        return true;
    }

    // Reset the accumulator if the delta has opposite sign.
    return direction && ((direction < 0) != (newDirection < 0));
}

PointerInterface::PointerInterface(SeatInterface *seat)
    : d(new PointerInterfacePrivate(this, seat))
{
}

PointerInterface::~PointerInterface()
{
}

SurfaceInterface *PointerInterface::focusedSurface() const
{
    return d->focusedSurface;
}

quint32 PointerInterface::focusedSerial() const
{
    return d->focusedSerial;
}

void PointerInterface::sendEnter(SurfaceInterface *surface, const QPointF &position, quint32 serial)
{
    if (d->focusedSurface == surface) {
        return;
    }

    if (d->focusedSurface) {
        d->sendLeave(serial);
        if (d->focusedSurface->client() != surface->client()) {
            d->sendFrame();
        }
        disconnect(d->destroyConnection);
    }

    d->focusedSurface = surface;
    d->focusedSerial = serial;
    d->destroyConnection = connect(d->focusedSurface, &SurfaceInterface::aboutToBeDestroyed, this, [this]() {
        d->sendLeave(d->seat->display()->nextSerial());
        d->sendFrame();
        d->focusedSurface = nullptr;
        Q_EMIT focusedSurfaceChanged();
    });

    d->sendEnter(d->focusedSurface->toSurfaceLocal(position), serial);
    d->sendFrame();
    d->lastPosition = position;

    Q_EMIT focusedSurfaceChanged();
}

void PointerInterface::sendLeave(quint32 serial)
{
    if (!d->focusedSurface) {
        return;
    }

    d->sendLeave(serial);
    d->sendFrame();

    d->focusedSurface = nullptr;
    disconnect(d->destroyConnection);

    Q_EMIT focusedSurfaceChanged();
}

static quint32 pointerButtonStateToWaylandState(PointerButtonState state)
{
    switch (state) {
    case PointerButtonState::Pressed:
        return WL_POINTER_BUTTON_STATE_PRESSED;
    case PointerButtonState::Released:
        return WL_POINTER_BUTTON_STATE_RELEASED;
    }

    Q_UNREACHABLE();
}

void PointerInterface::sendButton(quint32 button, PointerButtonState state, quint32 serial)
{
    if (!d->focusedSurface) {
        return;
    }

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    const quint32 waylandState = pointerButtonStateToWaylandState(state);
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        d->send_button(resource->handle, serial, d->seat->timestamp().count(), button, waylandState);
    }
}

void PointerInterface::sendButton(quint32 button, PointerButtonState state, ClientConnection *client)
{
    const auto pointerResources = d->pointersForClient(client);
    const quint32 waylandState = pointerButtonStateToWaylandState(state);
    const quint32 serial = d->seat->display()->nextSerial();
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        d->send_button(resource->handle, serial, d->seat->timestamp().count(), button, waylandState);
    }
}

static void updateAccumulators(Qt::Orientation orientation, qreal delta, qint32 deltaV120, PointerInterfacePrivate *d, qint32 &valueAxisLowRes, qint32 &valueDiscrete)
{
    const int newDirection = deltaV120 > 0 ? 1 : -1;
    auto &accum = d->axisAccumulator.axis(orientation);

    if (accum.shouldReset(newDirection, d->seat->timestamp())) {
        accum.reset();
    }

    accum.timestamp = d->seat->timestamp();
    accum.direction = newDirection;

    accum.axis += delta;
    accum.axis120 += deltaV120;

    // ±120 is a "wheel click"
    if (std::abs(accum.axis120) >= 60) {
        const int steps = accum.axis120 / 120;
        valueDiscrete += steps;
        if (steps == 0) {
            valueDiscrete += accum.direction;
        }

        accum.axis120 -= valueDiscrete * 120;
    }

    if (valueDiscrete) {
        // Accumulate the axis values to send to low-res clients
        valueAxisLowRes = accum.axis;
        accum.axis = 0;
    }
}

void PointerInterface::sendAxis(Qt::Orientation orientation, qreal delta, qint32 deltaV120, PointerAxisSource source, bool inverted)
{
    if (!d->focusedSurface) {
        return;
    }

    qint32 valueAxisLowRes = 0;
    qint32 valueDiscrete = 0;

    if (deltaV120) {
        updateAccumulators(orientation, delta, deltaV120, d.get(),
                           valueAxisLowRes, valueDiscrete);
    } else {
        valueAxisLowRes = delta;
    }

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        const quint32 version = resource->version();

        // Don't send anything if the client doesn't support high-res scrolling and
        // we haven't accumulated a wheel click's worth of events.
        if (version < WL_POINTER_AXIS_VALUE120_SINCE_VERSION && deltaV120 && !valueDiscrete) {
            continue;
        }

        if (source != PointerAxisSource::Unknown && version >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
            PointerInterfacePrivate::axis_source wlSource;
            switch (source) {
            case PointerAxisSource::Wheel:
                wlSource = PointerInterfacePrivate::axis_source_wheel;
                break;
            case PointerAxisSource::Finger:
                wlSource = PointerInterfacePrivate::axis_source_finger;
                break;
            case PointerAxisSource::Continuous:
                wlSource = PointerInterfacePrivate::axis_source_continuous;
                break;
            case PointerAxisSource::WheelTilt:
                wlSource = PointerInterfacePrivate::axis_source_wheel_tilt;
                break;
            default:
                Q_UNREACHABLE();
                break;
            }
            d->send_axis_source(resource->handle, wlSource);
        }

        const auto wlOrientation =
            (orientation == Qt::Vertical) ? PointerInterfacePrivate::axis_vertical_scroll : PointerInterfacePrivate::axis_horizontal_scroll;

        if (delta) {
            if (version >= WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION) {
                auto wlRelativeDirection = inverted ? PointerInterfacePrivate::axis_relative_direction_inverted : PointerInterfacePrivate::axis_relative_direction_identical;

                d->send_axis_relative_direction(resource->handle, wlOrientation, wlRelativeDirection);
            }
            if (deltaV120) {
                if (version >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION) {
                    // Send high resolution scroll events if client supports them
                    d->send_axis_value120(resource->handle, wlOrientation, deltaV120);
                    d->send_axis(resource->handle, d->seat->timestamp().count(), wlOrientation,
                                 wl_fixed_from_double(delta));
                } else {
                    if (version >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION && valueDiscrete) {
                        // Send discrete scroll events if client supports them.
                        d->send_axis_discrete(resource->handle, wlOrientation,
                                              valueDiscrete);
                    }
                    // Send accumulated axis values
                    d->send_axis(resource->handle, d->seat->timestamp().count(), wlOrientation,
                                 wl_fixed_from_double(valueAxisLowRes));
                }
            } else {
                // Finger or continuous scroll
                d->send_axis(resource->handle, d->seat->timestamp().count(), wlOrientation,
                             wl_fixed_from_double(delta));
            }
        } else if (version >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
            d->send_axis_stop(resource->handle, d->seat->timestamp().count(), wlOrientation);
        }
    }
}

void PointerInterface::sendMotion(const QPointF &position)
{
    d->lastPosition = position;

    if (!d->focusedSurface) {
        return;
    }

    const QPointF localPos = d->focusedSurface->toSurfaceLocal(position);

    const auto pointerResources = d->pointersForClient(d->focusedSurface->client());
    for (PointerInterfacePrivate::Resource *resource : pointerResources) {
        d->send_motion(resource->handle, d->seat->timestamp().count(), wl_fixed_from_double(localPos.x()), wl_fixed_from_double(localPos.y()));
    }
}

void PointerInterface::sendFrame()
{
    if (d->focusedSurface) {
        d->sendFrame();
    }
}

SeatInterface *PointerInterface::seat() const
{
    return d->seat;
}

PointerInterface *PointerInterface::get(wl_resource *native)
{
    if (PointerInterfacePrivate *pointerPrivate = resource_cast<PointerInterfacePrivate *>(native)) {
        return pointerPrivate->q;
    }
    return nullptr;
}

PointerSurfaceCursor::PointerSurfaceCursor()
    : d(new PointerSurfaceCursorPrivate())
{
}

PointerSurfaceCursor::~PointerSurfaceCursor()
{
}

QPointF PointerSurfaceCursor::hotspot() const
{
    return d->hotspot;
}

SurfaceInterface *PointerSurfaceCursor::surface() const
{
    return d->surface;
}

} // namespace KWin

#include "moc_pointer.cpp"
