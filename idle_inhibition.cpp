/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "idle_inhibition.h"
#include "deleted.h"
#include "shell_client.h"

#include <KWayland/Server/idle_interface.h>
#include <KWayland/Server/surface_interface.h>

#include <functional>

using KWayland::Server::SurfaceInterface;

namespace KWin
{

IdleInhibition::IdleInhibition(IdleInterface *idle)
    : QObject(idle)
    , m_idle(idle)
{
}

IdleInhibition::~IdleInhibition() = default;

void IdleInhibition::registerShellClient(ShellClient *client)
{
    auto surface = client->surface();
    m_connections.insert(client, connect(surface, &SurfaceInterface::inhibitsIdleChanged, this,
        [this, client] {
            // TODO: only inhibit if the ShellClient is visible
            if (client->surface()->inhibitsIdle()) {
                inhibit(client);
            } else {
                uninhibit(client);
            }
        }
    ));
    connect(client, &ShellClient::windowClosed, this,
        [this, client] {
            uninhibit(client);
            auto it = m_connections.find(client);
            if (it != m_connections.end()) {
                disconnect(it.value());
                m_connections.erase(it);
            }
        }
    );
}

void IdleInhibition::inhibit(ShellClient *client)
{
    if (isInhibited(client)) {
        // already inhibited
        return;
    }
    m_idleInhibitors << client;
    m_idle->inhibit();
    // TODO: notify powerdevil?
}

void IdleInhibition::uninhibit(ShellClient *client)
{
    auto it = std::find_if(m_idleInhibitors.begin(), m_idleInhibitors.end(), [client] (auto c) { return c == client; });
    if (it == m_idleInhibitors.end()) {
        // not inhibited
        return;
    }
    m_idleInhibitors.erase(it);
    m_idle->uninhibit();
}

}
