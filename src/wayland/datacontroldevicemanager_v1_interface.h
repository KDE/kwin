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
class DataControlSourceV1Interface;
class DataControlDeviceManagerV1InterfacePrivate;
class DataControlDeviceV1Interface;

/**
 * The DataControlDeviceManagerV1Interface provides a way for privileged clients such as clipboard
 * managers to manage the current selection.
 *
 * DataControlDeviceManagerV1Interface corresponds to the Wayland interface @c zwlr_data_control_manager_v1.
 */
class KWAYLANDSERVER_EXPORT DataControlDeviceManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit DataControlDeviceManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~DataControlDeviceManagerV1Interface() override;

Q_SIGNALS:
    void dataSourceCreated(KWaylandServer::DataControlSourceV1Interface *dataSource);
    void dataDeviceCreated(KWaylandServer::DataControlDeviceV1Interface *dataDevice);

private:
    QScopedPointer<DataControlDeviceManagerV1InterfacePrivate> d;
};

}

#endif
