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
#ifndef WAYLAND_SERVER_DATA_DEVICE_MANAGER_INTERFACE_H
#define WAYLAND_SERVER_DATA_DEVICE_MANAGER_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"
#include "datadevice_interface.h"

namespace KWayland
{
namespace Server
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
    void dataSourceCreated(KWayland::Server::DataSourceInterface*);
    void dataDeviceCreated(KWayland::Server::DataDeviceInterface*);

private:
    explicit DataDeviceManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWayland::Server::DataDeviceManagerInterface::DnDActions)

#endif
