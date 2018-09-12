/****************************************************************************
 * Copyright 2015 Sebastian Kügler <sebas@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef KWAYLAND_SERVER_OUTPUTMANAGEMENT_INTERFACE_H
#define KWAYLAND_SERVER_OUTPUTMANAGEMENT_INTERFACE_H

#include "global.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{

class OutputConfigurationInterface;
/**
 * @class OutputManagementInterface
 *
 * This class is used to change the configuration of the Wayland server's outputs.
 * The client requests an OutputConfiguration, changes its OutputDevices and then
 * calls OutputConfiguration::apply, which makes this class emit a signal, carrying
 * the new configuration.
 * The server is then expected to make the requested changes by applying the settings
 * of the OutputDevices to the Outputs.
 *
 * @see OutputConfiguration
 * @see OutputConfigurationInterface
 * @since 5.5
 */
class KWAYLANDSERVER_EXPORT OutputManagementInterface : public Global
{
    Q_OBJECT
public:
    virtual ~OutputManagementInterface();

Q_SIGNALS:
    /**
     * Emitted after the client has requested an OutputConfiguration to be applied.
     * through OutputConfiguration::apply. The compositor can use this object to get
     * notified when the new configuration is set up, and it should be applied to the
     * Wayland server's OutputInterfaces.
     *
     * @param config The OutputConfigurationInterface corresponding to the client that
     * called apply().
     * @see OutputConfiguration::apply
     * @see OutputConfigurationInterface
     * @see OutputDeviceInterface
     * @see OutputInterface
     */
    void configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *configurationInterface);

private:
    explicit OutputManagementInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}

#endif
