/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
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
 * @brief DataDeviceInterface allows clients to share data by copy-and-paste and drag-and-drop.
 *
 * The data device is per seat.
 * Copy-and-paste use the selection functions.
 *
 * Represents the Resource for the wl_data_device interface.
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
