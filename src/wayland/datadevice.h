/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

#include "abstract_drop_handler.h"

struct wl_client;
struct wl_resource;

namespace KWin
{
class DataDeviceManagerInterface;
class DataOfferInterface;
class DataSourceInterface;
class AbstractDataSource;
class SeatInterface;
class SurfaceInterface;
class SurfaceRole;
class DataDeviceInterfacePrivate;
class DragAndDropIconPrivate;

/**
 * The DragAndDropIcon class represents a drag-and-drop icon.
 *
 * Note that the lifetime of the drag-and-drop icon is bound to the lifetime of the underlying
 * icon surface.
 */
class KWIN_EXPORT DragAndDropIcon : public QObject
{
    Q_OBJECT

public:
    ~DragAndDropIcon() override;

    static SurfaceRole *role();

    /**
     * Returns the position of the icon relative to the cursor's hotspot.
     */
    QPoint position() const;

    /**
     * Returns the underlying icon surface. This function always returns a valid surface.
     */
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void changed();

private:
    void commit();

    explicit DragAndDropIcon(SurfaceInterface *surface);
    friend class DataDeviceInterfacePrivate;
    std::unique_ptr<DragAndDropIconPrivate> d;
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
 */
class KWIN_EXPORT DataDeviceInterface : public AbstractDropHandler
{
    Q_OBJECT
public:
    virtual ~DataDeviceInterface();

    SeatInterface *seat() const;

    DataSourceInterface *selection() const;

    void sendSelection(KWin::AbstractDataSource *other);
    /**
     * The event is sent when a drag-and-drop operation is ended because the implicit grab is removed.
     */
    void drop() override;
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
     */
    void updateDragTarget(SurfaceInterface *surface, quint32 serial) override;

    wl_client *client();

Q_SIGNALS:
    void aboutToBeDestroyed();
    void dragStarted(AbstractDataSource *source, SurfaceInterface *originSurface, quint32 serial, DragAndDropIcon *dragIcon);
    void selectionChanged(KWin::DataSourceInterface *, quint32 serial);

private:
    friend class DataDeviceManagerInterfacePrivate;
    explicit DataDeviceInterface(SeatInterface *seat, wl_resource *resource);
    std::unique_ptr<DataDeviceInterfacePrivate> d;
    friend class DataDeviceInterfacePrivate;
};

}

Q_DECLARE_METATYPE(KWin::DataDeviceInterface *)
