/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "qwayland-server-pointer-gestures-unstable-v1.h"

#include <QPointer>

namespace KWaylandServer
{

class Display;
class PointerInterface;

class PointerGesturesV1InterfacePrivate : public QtWaylandServer::zwp_pointer_gestures_v1
{
public:
    explicit PointerGesturesV1InterfacePrivate(Display *display);

protected:
    void zwp_pointer_gestures_v1_get_swipe_gesture(Resource *resource, uint32_t id,
                                                   struct ::wl_resource *pointer_resource) override;
    void zwp_pointer_gestures_v1_get_pinch_gesture(Resource *resource, uint32_t id,
                                                   struct ::wl_resource *pointer_resource) override;
    void zwp_pointer_gestures_v1_release(Resource *resource) override;
};

class PointerSwipeGestureV1Interface : public QtWaylandServer::zwp_pointer_gesture_swipe_v1
{
public:
    PointerSwipeGestureV1Interface(PointerInterface *pointer, ::wl_resource *resource);
    ~PointerSwipeGestureV1Interface() override;

    QPointer<PointerInterface> pointer;

protected:
    void zwp_pointer_gesture_swipe_v1_destroy_resource(Resource *resource) override;
    void zwp_pointer_gesture_swipe_v1_destroy(Resource *resource) override;
};

class PointerPinchGestureV1Interface : public QtWaylandServer::zwp_pointer_gesture_pinch_v1
{
public:
    PointerPinchGestureV1Interface(PointerInterface *pointer, ::wl_resource *resource);
    ~PointerPinchGestureV1Interface() override;

    QPointer<PointerInterface> pointer;

protected:
    void zwp_pointer_gesture_pinch_v1_destroy_resource(Resource *resource) override;
    void zwp_pointer_gesture_pinch_v1_destroy(Resource *resource) override;
};

} // namespace KWaylandServer
