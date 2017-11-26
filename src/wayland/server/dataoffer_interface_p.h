/********************************************************************
Copyright 2017  Martin Flöser <mgraesslin@kde.org>

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
#ifndef KWAYLAND_SERVER_DATAOFFERINTERFACE_P_H
#define KWAYLAND_SERVER_DATAOFFERINTERFACE_P_H
#include "dataoffer_interface.h"
#include "datasource_interface.h"
#include "resource_p.h"
#include <wayland-server.h>

namespace KWayland
{
namespace Server
{

class Q_DECL_HIDDEN DataOfferInterface::Private : public Resource::Private
{
public:
    Private(DataSourceInterface *source, DataDeviceInterface *parentInterface, DataOfferInterface *q, wl_resource *parentResource);
    ~Private();
    DataSourceInterface *source;
    DataDeviceInterface *dataDevice;
    // defaults are set to sensible values for < version 3 interfaces
    DataDeviceManagerInterface::DnDActions supportedDnDActions = DataDeviceManagerInterface::DnDAction::Copy | DataDeviceManagerInterface::DnDAction::Move;
    DataDeviceManagerInterface::DnDAction preferredDnDAction = DataDeviceManagerInterface::DnDAction::Copy;

    void sendSourceActions();

private:
    DataOfferInterface *q_func() {
        return reinterpret_cast<DataOfferInterface *>(q);
    }
    void receive(const QString &mimeType, qint32 fd);
    static void acceptCallback(wl_client *client, wl_resource *resource, uint32_t serial, const char *mimeType);
    static void receiveCallback(wl_client *client, wl_resource *resource, const char *mimeType, int32_t fd);
    static void finishCallback(wl_client *client, wl_resource *resource);
    static void setActionsCallback(wl_client *client, wl_resource *resource, uint32_t dnd_actions, uint32_t preferred_action);

    static const struct wl_data_offer_interface s_interface;
};

}
}

#endif
