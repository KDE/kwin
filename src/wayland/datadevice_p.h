/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QPointer>

#include "qwayland-server-wayland.h"

namespace KWin
{
class AbstractDataSource;
class DataDeviceInterface;
class DataOfferInterface;
class DragAndDropIcon;
class SeatInterface;
class SurfaceInterface;

class DataDeviceInterfacePrivate : public QtWaylandServer::wl_data_device
{
public:
    static DataDeviceInterfacePrivate *get(DataDeviceInterface *device);

    DataDeviceInterfacePrivate(SeatInterface *seat, DataDeviceInterface *_q, wl_resource *resource);

    DataOfferInterface *createDataOffer(AbstractDataSource *source);

    SeatInterface *seat;
    DataDeviceInterface *q;

    struct Drag
    {
        SurfaceInterface *surface = nullptr;
        QPointer<DataOfferInterface> offer;
        QMetaObject::Connection destroyConnection;
        QMetaObject::Connection sourceActionConnection;
        QMetaObject::Connection targetActionConnection;
        QMetaObject::Connection keyboardModifiersConnection;
    };
    Drag drag;

protected:
    void data_device_destroy_resource(Resource *resource) override;
    void data_device_start_drag(Resource *resource, wl_resource *source, wl_resource *origin, wl_resource *icon, uint32_t serial) override;
    void data_device_set_selection(Resource *resource, wl_resource *source, uint32_t serial) override;
    void data_device_release(Resource *resource) override;
};

} // namespace KWin
