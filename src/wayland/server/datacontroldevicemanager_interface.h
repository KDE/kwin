/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_CONTROL_DEVICE_MANAGER_INTERFACE_H
#define WAYLAND_SERVER_DATA_CONTROL_DEVICE_MANAGER_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class Display;
class DataControlSourceInterface;

class DataControlDeviceManagerInterfacePrivate;
class DataControlDeviceInterface;

/**
 * @brief Represents the Global for zwlr_data_control_manager_v1 interface.
 *
 **/
class KWAYLANDSERVER_EXPORT DataControlDeviceManagerInterface : public QObject
{
    Q_OBJECT
public:
    ~DataControlDeviceManagerInterface() override;

Q_SIGNALS:
    void dataSourceCreated(KWaylandServer::DataControlSourceInterface *dataSource);
    void dataDeviceCreated(KWaylandServer::DataControlDeviceInterface *dataDevice);

private:
    explicit DataControlDeviceManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    QScopedPointer<DataControlDeviceManagerInterfacePrivate> d;
};

}

#endif
