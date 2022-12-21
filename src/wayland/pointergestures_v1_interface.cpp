/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointergestures_v1_interface.h"
#include "clientconnection.h"
#include "display.h"
#include "pointer_interface_p.h"
#include "pointergestures_v1_interface_p.h"
#include "seat_interface.h"
#include "surface_interface.h"

namespace KWaylandServer
{
static const int s_version = 3;

PointerGesturesV1InterfacePrivate::PointerGesturesV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_gestures_v1(*display, s_version)
{
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_swipe_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid pointer");
        return;
    }

    PointerSwipeGestureV1Interface *swipeGesture = PointerSwipeGestureV1Interface::get(pointer);
    swipeGesture->add(resource->client(), id, resource->version());
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_pinch_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "invalid pointer");
        return;
    }

    PointerPinchGestureV1Interface *pinchGesture = PointerPinchGestureV1Interface::get(pointer);
    pinchGesture->add(resource->client(), id, resource->version());
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_hold_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    PointerHoldGestureV1Interface *holdGesture = PointerHoldGestureV1Interface::get(pointer);
    holdGesture->add(resource->client(), id, resource->version());
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PointerGesturesV1Interface::PointerGesturesV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new PointerGesturesV1InterfacePrivate(display))
{
}

PointerGesturesV1Interface::~PointerGesturesV1Interface()
{
}

PointerSwipeGestureV1Interface::PointerSwipeGestureV1Interface(PointerInterface *pointer)
    : pointer(pointer)
{
}

PointerSwipeGestureV1Interface *PointerSwipeGestureV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->swipeGesturesV1.get();
    }
    return nullptr;
}

void PointerSwipeGestureV1Interface::zwp_pointer_gesture_swipe_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerSwipeGestureV1Interface::sendBegin(quint32 serial, quint32 fingerCount)
{
    if (focusedClient) {
        return;
    }
    if (!pointer->focusedSurface()) {
        return;
    }

    const SurfaceInterface *focusedSurface = pointer->focusedSurface();
    focusedClient = focusedSurface->client();
    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        send_begin(swipeResource->handle, serial, seat->timestamp().count(), focusedSurface->resource(), fingerCount);
    }
}

void PointerSwipeGestureV1Interface::sendUpdate(const QPointF &delta)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        send_update(swipeResource->handle, seat->timestamp().count(), wl_fixed_from_double(delta.x()), wl_fixed_from_double(delta.y()));
    }
}

void PointerSwipeGestureV1Interface::sendEnd(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        send_end(swipeResource->handle, serial, seat->timestamp().count(), false);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

void PointerSwipeGestureV1Interface::sendCancel(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> swipeResources = resourceMap().values(focusedClient->client());
    for (Resource *swipeResource : swipeResources) {
        send_end(swipeResource->handle, serial, seat->timestamp().count(), true);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

PointerPinchGestureV1Interface::PointerPinchGestureV1Interface(PointerInterface *pointer)
    : pointer(pointer)
{
}

PointerPinchGestureV1Interface *PointerPinchGestureV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->pinchGesturesV1.get();
    }
    return nullptr;
}

void PointerPinchGestureV1Interface::zwp_pointer_gesture_pinch_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerPinchGestureV1Interface::sendBegin(quint32 serial, quint32 fingerCount)
{
    if (focusedClient) {
        return; // gesture is already active
    }
    if (!pointer->focusedSurface()) {
        return;
    }

    const SurfaceInterface *focusedSurface = pointer->focusedSurface();
    focusedClient = focusedSurface->client();
    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        send_begin(pinchResource->handle, serial, seat->timestamp().count(), focusedSurface->resource(), fingerCount);
    }
}

void PointerPinchGestureV1Interface::sendUpdate(const QPointF &delta, qreal scale, qreal rotation)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        send_update(pinchResource->handle,
                    seat->timestamp().count(),
                    wl_fixed_from_double(delta.x()),
                    wl_fixed_from_double(delta.y()),
                    wl_fixed_from_double(scale),
                    wl_fixed_from_double(rotation));
    }
}

void PointerPinchGestureV1Interface::sendEnd(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        send_end(pinchResource->handle, serial, seat->timestamp().count(), false);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

void PointerPinchGestureV1Interface::sendCancel(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> pinchResources = resourceMap().values(*focusedClient);
    for (Resource *pinchResource : pinchResources) {
        send_end(pinchResource->handle, serial, seat->timestamp().count(), true);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

PointerHoldGestureV1Interface::PointerHoldGestureV1Interface(PointerInterface *pointer)
    : pointer(pointer)
{
}

PointerHoldGestureV1Interface *PointerHoldGestureV1Interface::get(PointerInterface *pointer)
{
    if (pointer) {
        PointerInterfacePrivate *pointerPrivate = PointerInterfacePrivate::get(pointer);
        return pointerPrivate->holdGesturesV1.get();
    }
    return nullptr;
}

void PointerHoldGestureV1Interface::zwp_pointer_gesture_hold_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerHoldGestureV1Interface::sendBegin(quint32 serial, quint32 fingerCount)
{
    if (focusedClient) {
        return; // gesture is already active
    }
    if (!pointer->focusedSurface()) {
        return;
    }

    const SurfaceInterface *focusedSurface = pointer->focusedSurface();
    focusedClient = focusedSurface->client();
    SeatInterface *seat = pointer->seat();

    const QList<Resource *> holdResources = resourceMap().values(*focusedClient);
    for (Resource *holdResource : holdResources) {
        send_begin(holdResource->handle, serial, seat->timestamp().count(), focusedSurface->resource(), fingerCount);
    }
}

void PointerHoldGestureV1Interface::sendEnd(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> holdResources = resourceMap().values(*focusedClient);
    for (Resource *holdResource : holdResources) {
        send_end(holdResource->handle, serial, seat->timestamp().count(), false);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

void PointerHoldGestureV1Interface::sendCancel(quint32 serial)
{
    if (!focusedClient) {
        return;
    }

    SeatInterface *seat = pointer->seat();

    const QList<Resource *> holdResources = resourceMap().values(*focusedClient);
    for (Resource *holdResource : holdResources) {
        send_end(holdResource->handle, serial, seat->timestamp().count(), true);
    }

    // The gesture session has been just finished, reset the cached focused client.
    focusedClient = nullptr;
}

} // namespace KWaylandServer
