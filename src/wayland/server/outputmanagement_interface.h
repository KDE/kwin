/*
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef KWAYLAND_SERVER_OUTPUTMANAGEMENT_INTERFACE_H
#define KWAYLAND_SERVER_OUTPUTMANAGEMENT_INTERFACE_H

#include "global.h"

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
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
    explicit OutputManagementInterface(Display *display, QObject *parent = nullptr);
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
    void configurationChangeRequested(KWaylandServer::OutputConfigurationInterface *configurationInterface);

private:
    class Private;
};

}

#endif
