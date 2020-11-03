/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_P_H
#define WAYLAND_SERVER_POINTER_INTERFACE_P_H
#include "pointer_interface.h"
#include "resource_p.h"

#include <QPointer>
#include <QVector>

namespace KWaylandServer
{
class PointerPinchGestureV1Interface;
class PointerSwipeGestureV1Interface;
class RelativePointerV1Interface;

class PointerInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QPointer<SurfaceInterface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;
    Cursor *cursor = nullptr;
    QVector<RelativePointerV1Interface *> relativePointersV1;
    QVector<PointerSwipeGestureV1Interface *> swipeGesturesV1;
    QVector<PointerPinchGestureV1Interface *> pinchGesturesV1;

    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, const QPointF &parentSurfacePosition, quint32 serial);
    void sendFrame();

    void registerRelativePointerV1(RelativePointerV1Interface *relativePointer);
    void registerSwipeGestureV1(PointerSwipeGestureV1Interface *gesture);
    void registerPinchGestureV1(PointerPinchGestureV1Interface *gesture);

    void unregisterRelativePointerV1(RelativePointerV1Interface *relativePointer);
    void unregisterSwipeGestureV1(PointerSwipeGestureV1Interface *gesture);
    void unregisterPinchGestureV1(PointerPinchGestureV1Interface *gesture);

    void startSwipeGesture(quint32 serial, quint32 fingerCount);
    void updateSwipeGesture(const QSizeF &delta);
    void endSwipeGesture(quint32 serial);
    void cancelSwipeGesture(quint32 serial);

    void startPinchGesture(quint32 serial, quint32 fingerCount);
    void updatePinchGesture(const QSizeF &delta, qreal scale, qreal rotation);
    void endPinchGesture(quint32 serial);
    void cancelPinchGesture(quint32 serial);

private:
    PointerInterface *q_func() {
        return reinterpret_cast<PointerInterface *>(q);
    }
    void setCursor(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot);
    // interface
    static void setCursorCallback(wl_client *client, wl_resource *resource, uint32_t serial,
                                  wl_resource *surface, int32_t hotspot_x, int32_t hotspot_y);

    static const struct wl_pointer_interface s_interface;
};

}

#endif
