/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DATA_DEVICE_MANAGER_INTERFACE_H
#define WAYLAND_SERVER_DATA_DEVICE_MANAGER_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>
#include "global.h"
#include "datadevice_interface.h"

namespace KWaylandServer
{

class Display;
class DataSourceInterface;

/**
 * @brief Represents the Global for wl_data_device_manager interface.
 *
 **/
class KWAYLANDSERVER_EXPORT DataDeviceManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~DataDeviceManagerInterface();

    /**
     * Drag and Drop actions supported by the DataSourceInterface.
     * @since 5.XX
     **/
    enum class DnDAction {
        None = 0,
        Copy = 1 << 0,
        Move = 1 << 1,
        Ask = 1 << 2
    };
    Q_DECLARE_FLAGS(DnDActions, DnDAction)

Q_SIGNALS:
    void dataSourceCreated(KWaylandServer::DataSourceInterface*);
    void dataDeviceCreated(KWaylandServer::DataDeviceInterface*);

private:
    explicit DataDeviceManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWaylandServer::DataDeviceManagerInterface::DnDActions)

#endif
