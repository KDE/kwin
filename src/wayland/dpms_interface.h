/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef WAYLAND_SERVER_DPMS_INTERFACE_H
#define WAYLAND_SERVER_DPMS_INTERFACE_H

#include <QObject>

#include <KWayland/Server/kwaylandserver_export.h>
#include "global.h"

namespace KWayland
{
namespace Server
{

class Display;

/**
 * @brief Global for the org_kde_kwin_dpms_manager interface.
 *
 * If the DpmsManagerInterface is created it allows a client to
 * query the Dpms state on a given OutputInterface and request
 * changes for it. The code interaction happens only via the
 * OutputInterface.
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
 *         [] (KWayland::Server::OutputInterface::DpmsMode requestedMode) { qDebug() << "Mode change requested"; });
 * @endcode
 *
 * @see Display
 * @see OutputInterface
 * @since 5.5
 **/
class KWAYLANDSERVER_EXPORT DpmsManagerInterface : public Global
{
    Q_OBJECT
public:
    virtual ~DpmsManagerInterface();

private:
    explicit DpmsManagerInterface(Display *display, QObject *parent = nullptr);
    friend class Display;
    class Private;
};

}
}

#endif
