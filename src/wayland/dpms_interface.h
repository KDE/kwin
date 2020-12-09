/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DPMS_INTERFACE_H
#define WAYLAND_SERVER_DPMS_INTERFACE_H

#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

namespace KWaylandServer
{

class Display;
class DpmsManagerInterfacePrivate;

/**
 * @brief Global for server side Display Power Management Signaling interface.
 *
 * A DpmsManagerInterface allows a client to query the DPMS state
 * on a given OutputInterface and request changes to it.
 * Server-side the interaction happens only via the OutputInterface,
 * for clients the Dpms class provides the API.
 * This global implements org_kde_kwin_dpms_manager.
 *
 * To create a DpmsManagerInterface use:
 * @code
 * auto manager = display->createDpmsManager();
 * manager->create();
 * @endcode
 *
 * To interact with Dpms use one needs to mark it as enabled and set the
 * proper mode on the OutputInterface.
 * @code
 * // We have our OutputInterface called output.
 * output->setDpmsSupported(true);
 * output->setDpmsMode(OutputInterface::DpmsMode::On);
 * @endcode
 *
 * To connect to Dpms change requests use:
 * @code
 * connect(output, &OutputInterface::dpmsModeRequested,
 *         [] (KWaylandServer::OutputInterface::DpmsMode requestedMode) { qDebug() << "Mode change requested"; });
 * @endcode
 *
 * @see Display
 * @see OutputInterface
 * @since 5.5
 **/
class KWAYLANDSERVER_EXPORT DpmsManagerInterface : public QObject
{
    Q_OBJECT

public:
    explicit DpmsManagerInterface(Display *display, QObject *parent = nullptr);
    ~DpmsManagerInterface() override;

private:
    QScopedPointer<DpmsManagerInterfacePrivate> d;
};

}

#endif
