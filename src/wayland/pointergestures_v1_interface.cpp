/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "pointergestures_v1_interface.h"
#include "display.h"
#include "pointer_interface_p.h"
#include "pointergestures_v1_interface_p.h"

namespace KWaylandServer
{

static const int s_version = 1;

PointerGesturesV1InterfacePrivate::PointerGesturesV1InterfacePrivate(Display *display)
    : QtWaylandServer::zwp_pointer_gestures_v1(*display, s_version)
{
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_swipe_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    wl_resource *swipeResource = wl_resource_create(resource->client(), &zwp_pointer_gesture_swipe_v1_interface,
                                                    resource->version(), id);
    if (!swipeResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    new PointerSwipeGestureV1Interface(pointer, swipeResource);
}

void PointerGesturesV1InterfacePrivate::zwp_pointer_gestures_v1_get_pinch_gesture(Resource *resource, uint32_t id, struct ::wl_resource *pointer_resource)
{
    PointerInterface *pointer = PointerInterface::get(pointer_resource);
    if (!pointer) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "invalid pointer");
        return;
    }

    wl_resource *pinchResource = wl_resource_create(resource->client(), &zwp_pointer_gesture_pinch_v1_interface,
                                                    resource->version(), id);
    if (!pinchResource) {
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    new PointerPinchGestureV1Interface(pointer, pinchResource);
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

PointerSwipeGestureV1Interface::PointerSwipeGestureV1Interface(PointerInterface *pointer,
                                                               ::wl_resource *resource)
    : QtWaylandServer::zwp_pointer_gesture_swipe_v1(resource)
    , pointer(pointer)
{
    pointer->d_func()->registerSwipeGestureV1(this);
}

PointerSwipeGestureV1Interface::~PointerSwipeGestureV1Interface()
{
    if (pointer) {
        pointer->d_func()->unregisterSwipeGestureV1(this);
    }
}

void PointerSwipeGestureV1Interface::zwp_pointer_gesture_swipe_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void PointerSwipeGestureV1Interface::zwp_pointer_gesture_swipe_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

PointerPinchGestureV1Interface::PointerPinchGestureV1Interface(PointerInterface *pointer,
                                                               ::wl_resource *resource)
    : QtWaylandServer::zwp_pointer_gesture_pinch_v1(resource)
    , pointer(pointer)
{
    pointer->d_func()->registerPinchGestureV1(this);
}

PointerPinchGestureV1Interface::~PointerPinchGestureV1Interface()
{
    if (pointer) {
        pointer->d_func()->unregisterPinchGestureV1(this);
    }
}

void PointerPinchGestureV1Interface::zwp_pointer_gesture_pinch_v1_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void PointerPinchGestureV1Interface::zwp_pointer_gesture_pinch_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

} // namespace KWaylandServer
