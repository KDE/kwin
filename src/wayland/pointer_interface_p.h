/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_P_H
#define WAYLAND_SERVER_POINTER_INTERFACE_P_H
#include "pointer_interface.h"
#include "resource_p.h"

#include <QPointer>
#include <QVector>

namespace KWayland
{
namespace Server
{
class PointerPinchGestureInterface;
class PointerSwipeGestureInterface;
class RelativePointerInterface;

class PointerInterface::Private : public Resource::Private
{
public:
    Private(SeatInterface *parent, wl_resource *parentResource, PointerInterface *q);

    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QPointer<SurfaceInterface> focusedChildSurface;
    QMetaObject::Connection destroyConnection;
    Cursor *cursor = nullptr;
    QVector<RelativePointerInterface*> relativePointers;
    QVector<PointerSwipeGestureInterface*> swipeGestures;
    QVector<PointerPinchGestureInterface*> pinchGestures;

    void sendLeave(SurfaceInterface *surface, quint32 serial);
    void sendEnter(SurfaceInterface *surface, const QPointF &parentSurfacePosition, quint32 serial);

    void registerRelativePointer(RelativePointerInterface *relativePointer);
    void registerSwipeGesture(PointerSwipeGestureInterface *gesture);
    void registerPinchGesture(PointerPinchGestureInterface *gesture);

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
}

#endif
