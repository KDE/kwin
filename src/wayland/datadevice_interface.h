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
#ifndef WAYLAND_SERVER_DATA_DEVICE_INTERFACE_H
#define WAYLAND_SERVER_DATA_DEVICE_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>

#include "resource.h"

namespace KWayland
{
namespace Server
{

class DataDeviceManagerInterface;
class DataOfferInterface;
class DataSourceInterface;
class SeatInterface;
class SurfaceInterface;

/**
 * @brief Represents the Resource for the wl_data_device interface.
 *
 * @see SeatInterface
 * @see DataSourceInterface
 **/
class KWAYLANDSERVER_EXPORT DataDeviceInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~DataDeviceInterface();

    SeatInterface *seat() const;
    DataSourceInterface *dragSource() const;
    SurfaceInterface *origin() const;
    SurfaceInterface *icon() const;

    /**
     * @returns the serial of the implicit grab which started the drag
     * @since 5.6
     **/
    quint32 dragImplicitGrabSerial() const;

    DataSourceInterface *selection() const;

    void sendSelection(DataDeviceInterface *other);
    void sendClearSelection();
    /**
     * The event is sent when a drag-and-drop operation is ended because the implicit grab is removed.
     * @since 5.6
     **/
    void drop();
    /**
     * Updates the SurfaceInterface to which drag motion events are sent.
     *
     * If a SurfaceInterface was registered in this DataDeviceInterface for drag motion events, it
     * will be sent a leave event.
     *
     * If @p surface is not null it will be sent a drag enter event.
     *
     * @param surface The SurfaceInterface which gets motion events
     * @param serial The serial to be used for enter/leave
     * @since 5.6
     **/
    void updateDragTarget(SurfaceInterface *surface, quint32 serial);
    /**
     * Mark this DataDeviceInterface as being a proxy device for @p remote.
     * @since 5.56
     **/
    void updateProxy(SurfaceInterface *remote);

Q_SIGNALS:
    void dragStarted();
    void selectionChanged(KWayland::Server::DataSourceInterface*);
    void selectionCleared();

private:
    friend class DataDeviceManagerInterface;
    explicit DataDeviceInterface(SeatInterface *seat, DataDeviceManagerInterface *parent, wl_resource *parentResource);

    class Private;
    Private *d_func() const;
};

}
}

Q_DECLARE_METATYPE(KWayland::Server::DataDeviceInterface*)

#endif
