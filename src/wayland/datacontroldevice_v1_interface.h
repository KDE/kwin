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
class DataControlDeviceManagerV1Interface;
class DataControlOfferV1Interface;
class DataControlSourceV1Interface;
class SeatInterface;
class SurfaceInterface;

/**
 * @brief Represents the Resource for the wl_data_device interface.
 *
 * @see SeatInterface
 * @see DataControlSourceV1Interface
 * Lifespan is mapped to the underlying object
 **/

class DataControlDeviceV1InterfacePrivate;

class KWAYLANDSERVER_EXPORT DataControlDeviceV1Interface : public QObject
{
    Q_OBJECT

public:
    ~DataControlDeviceV1Interface() override;

    SeatInterface *seat() const;
    DataControlSourceV1Interface *selection() const;

    void sendSelection(AbstractDataSource *other);
    void sendClearSelection();

Q_SIGNALS:
    void selectionChanged(KWaylandServer::DataControlSourceV1Interface*);
    void selectionCleared();

private:
    friend class DataControlDeviceManagerV1InterfacePrivate;
    explicit DataControlDeviceV1Interface(SeatInterface *seat, wl_resource *resource);

    QScopedPointer<DataControlDeviceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::DataControlDeviceV1Interface*)

#endif
