/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_CONTROL_DEVICE_INTERFACE_H
#define WAYLAND_SERVER_DATA_CONTROL_DEVICE_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

struct wl_resource;

namespace KWaylandServer
{

class AbstractDataSource;
class DataControlDeviceManagerInterface;
class DataControlOfferInterface;
class DataControlSourceInterface;
class SeatInterface;
class SurfaceInterface;

/**
 * @brief Represents the Resource for the wl_data_device interface.
 *
 * @see SeatInterface
 * @see DataControlSourceInterface
 * Lifespan is mapped to the underlying object
 **/

class DataControlDeviceInterfacePrivate;

class KWAYLANDSERVER_EXPORT DataControlDeviceInterface : public QObject
{
    Q_OBJECT
public:
    ~DataControlDeviceInterface() override;

    SeatInterface *seat() const;

    DataControlSourceInterface *selection() const;

    void sendSelection(AbstractDataSource *other);
    void sendClearSelection();

Q_SIGNALS:
    void selectionChanged(KWaylandServer::DataControlSourceInterface*);
    void selectionCleared();

private:
    friend class DataControlDeviceManagerInterfacePrivate;
    explicit DataControlDeviceInterface(SeatInterface *seat, wl_resource *resource);

    QScopedPointer<DataControlDeviceInterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlDeviceInterface*)

#endif
