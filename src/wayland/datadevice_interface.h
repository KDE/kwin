/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_DEVICE_INTERFACE_H
#define WAYLAND_SERVER_DATA_DEVICE_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

#include "resource.h"

namespace KWaylandServer
{

class DataDeviceManagerInterface;
class DataOfferInterface;
class DataSourceInterface;
class AbstractDataSource;
class SeatInterface;
class SurfaceInterface;
class DataDeviceInterfacePrivate;
class DragAndDropIconPrivate;

/**
 * The DragAndDropIcon class represents a drag-and-drop icon.
 *
 * Note that the lifetime of the drag-and-drop icon is bound to the lifetime of the underlying
 * icon surface.
 */
class KWAYLANDSERVER_EXPORT DragAndDropIcon : public QObject
{
    Q_OBJECT

public:
    ~DragAndDropIcon() override;

    /**
     * Returns the position of the icon relative to the cursor's hotspot.
     */
    QPoint position() const;

    /**
     * Returns the underlying icon surface. This function always returns a valid surface.
     */
    SurfaceInterface *surface() const;

private:
    explicit DragAndDropIcon(SurfaceInterface *surface, QObject *parent = nullptr);
    friend class DataDeviceInterfacePrivate;
    QScopedPointer<DragAndDropIconPrivate> d;
};

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
class KWAYLANDSERVER_EXPORT DataDeviceInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~DataDeviceInterface();

    SeatInterface *seat() const;
    DataSourceInterface *dragSource() const;
    SurfaceInterface *origin() const;
    /**
     * Returns the additional icon attached to the cursor during a drag-and-drop operation.
     * This function returns @c null if no drag-and-drop icon has been attached.
     */
    DragAndDropIcon *icon() const;

    /**
     * @returns the serial of the implicit grab which started the drag
     * @since 5.6
     **/
    quint32 dragImplicitGrabSerial() const;

    DataSourceInterface *selection() const;

    void sendSelection(KWaylandServer::AbstractDataSource *other);
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

    wl_client *client();

Q_SIGNALS:
    void aboutToBeDestroyed();
    void dragStarted();
    void selectionChanged(KWaylandServer::DataSourceInterface*);
    void selectionCleared();

private:
    friend class DataDeviceManagerInterfacePrivate;
    explicit DataDeviceInterface(SeatInterface *seat, wl_resource *resource);
    QScopedPointer<DataDeviceInterfacePrivate> d;
    friend class DataDeviceInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataDeviceInterface*)

#endif
