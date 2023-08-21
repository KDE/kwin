/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Adrien Faveraux <ad1rie3@hotmail.fr>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "pointer_interface.h"

#include <QPointF>
#include <QPointer>
#include <QVector>

#include "qwayland-server-wayland.h"

namespace KWaylandServer
{
class ClientConnection;
class PointerPinchGestureV1Interface;
class PointerSwipeGestureV1Interface;
class PointerHoldGestureV1Interface;
class RelativePointerV1Interface;

class PointerInterfacePrivate : public QtWaylandServer::wl_pointer
{
    struct AxisAccumulator
    {
        struct Axis
        {
            bool shouldReset(int newDirection, std::chrono::milliseconds newTimestamp) const;

            void reset()
            {
                axis120 = 0;
                axis = 0;
            }

            qint32 axis120 = 0;
            qreal axis = 0;
            int direction = 0;
            std::chrono::milliseconds timestamp = std::chrono::milliseconds::zero();
        };

        Axis &axis(Qt::Orientation orientation)
        {
            const int index = orientation == Qt::Orientation::Horizontal ? 0 : 1;
            return m_axis[index];
        }

    private:
        Axis m_axis[2];
    };

public:
    static PointerInterfacePrivate *get(PointerInterface *pointer);

    PointerInterfacePrivate(PointerInterface *q, SeatInterface *seat);
    ~PointerInterfacePrivate() override;

    QList<Resource *> pointersForClient(ClientConnection *client) const;

    PointerInterface *q;
    SeatInterface *seat;
    SurfaceInterface *focusedSurface = nullptr;
    QMetaObject::Connection destroyConnection;
    Cursor *cursor = nullptr;
    std::unique_ptr<RelativePointerV1Interface> relativePointersV1;
    std::unique_ptr<PointerSwipeGestureV1Interface> swipeGesturesV1;
    std::unique_ptr<PointerPinchGestureV1Interface> pinchGesturesV1;
    std::unique_ptr<PointerHoldGestureV1Interface> holdGesturesV1;
    QPointF lastPosition;
    AxisAccumulator axisAccumulator;

    void sendLeave(quint32 serial);
    void sendEnter(const QPointF &parentSurfacePosition, quint32 serial);
    void sendFrame();

protected:
    void pointer_set_cursor(Resource *resource, uint32_t serial, ::wl_resource *surface_resource, int32_t hotspot_x, int32_t hotspot_y) override;
    void pointer_release(Resource *resource) override;
    void pointer_bind_resource(Resource *resource) override;
};

}
