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
#ifndef KWAYLAND_SERVER_OUTPUTCONFIGURATION_INTERFACE_H
#define KWAYLAND_SERVER_OUTPUTCONFIGURATION_INTERFACE_H

#include "resource.h"
#include "outputmanagement_interface.h"
#include "outputdevice_interface.h"
#include "outputchangeset.h"

#include <KWayland/Server/kwaylandserver_export.h>

namespace KWayland
{
namespace Server
{
/** @class OutputConfigurationInterface
 *
 * Holds a new configuration for the outputs.
 *
 * The overall mechanism is to get a new OutputConfiguration from the OutputManagement global and
 * apply changes through the OutputConfiguration::set* calls. When all changes are set, the client
 * calls apply, which asks the server to look at the changes and apply them. The server will then
 * signal back whether the changes have been applied successfully (@c setApplied()) or were rejected
 * or failed to apply (@c setFailed()).
 *
 * Once the client has called applied, the OutputManagementInterface send the configuration object
 * to the compositor through the OutputManagement::configurationChangeRequested(OutputConfiguration*)
 * signal, the compositor can then decide what to do with the changes.
 *
 * These KWayland classes will not apply changes to the OutputDevices, this is the compositor's
 * task. As such, the configuration set through this interface can be seen as a hint what the
 * compositor should set up, but whether or not the compositor does it (based on hardware or
 * rendering policies, for example), is up to the compositor. The mode setting is passed on to
 * the DRM subsystem through the compositor. The compositor also saves this configuration and reads
 * it on startup, this interface is not involved in that process.
 *
 * @see OutputManagementInterface
 * @see OutputConfiguration
 * @since 5.5
 */
class KWAYLANDSERVER_EXPORT OutputConfigurationInterface : public Resource
{
    Q_OBJECT
public:
    virtual ~OutputConfigurationInterface();

    /**
     * Accessor for the changes made to OutputDevices. The data returned from this call
     * will be deleted by the OutputConfigurationInterface when
     * OutputManagementInterface::setApplied() or OutputManagementInterface::setFailed()
     * is called, and on destruction of the OutputConfigurationInterface, so make sure you
     * do not keep these pointers around.
     * @returns A QHash of ChangeSets per outputdevice.
     * @see ChangeSet
     * @see OutputDeviceInterface
     * @see OutputManagement
     */
    QHash<OutputDeviceInterface*, OutputChangeSet*> changes() const;

public Q_SLOTS:
    /**
     * Called by the compositor once the changes have successfully been applied.
     * The compositor is responsible for updating the OutputDevices. After having
     * done so, calling this function sends applied() through the client.
     * @see setFailed
     * @see OutputConfiguration::applied
     */
    void setApplied();
    /**
     * Called by the compositor when the changes as a whole are rejected or
     * failed to apply. This function results in the client OutputConfiguration emitting
     * failed().
     * @see setApplied
     * @see OutputConfiguration::failed
     */
    void setFailed();

private:
    explicit OutputConfigurationInterface(OutputManagementInterface *parent, wl_resource *parentResource);
    friend class OutputManagementInterface;

    class Private;
    Private *d_func() const;
};


}
}

Q_DECLARE_METATYPE(KWayland::Server::OutputConfigurationInterface*)

#endif
