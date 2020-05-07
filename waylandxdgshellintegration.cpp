/*
 * Copyright (C) 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "waylandxdgshellintegration.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xdgshellclient.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/xdgshell_interface.h>

using namespace KWaylandServer;

namespace KWin
{

/**
 * The WaylandXdgShellIntegration class is a factory class for xdg-shell clients.
 *
 * The xdg-shell protocol defines two surface roles - xdg_toplevel and xdg_popup. On the
 * compositor side, those roles are represented by XdgToplevelClient and XdgPopupClient,
 * respectively.
 *
 * WaylandXdgShellIntegration monitors for new xdg_toplevel and xdg_popup objects. If it
 * detects one, it will create an XdgToplevelClient or XdgPopupClient based on the current
 * surface role of the underlying xdg_surface object.
 */

WaylandXdgShellIntegration::WaylandXdgShellIntegration(QObject *parent)
    : WaylandShellIntegration(parent)
{
    XdgShellInterface *shell = waylandServer()->display()->createXdgShell(this);

    connect(shell, &XdgShellInterface::toplevelCreated,
            this, &WaylandXdgShellIntegration::registerXdgToplevel);
    connect(shell, &XdgShellInterface::popupCreated,
            this, &WaylandXdgShellIntegration::registerXdgPopup);
}

void WaylandXdgShellIntegration::registerXdgToplevel(XdgToplevelInterface *toplevel)
{
    // Note that the client is going to be destroyed and immediately re-created when the
    // underlying surface is unmapped. XdgToplevelClient is re-created right away since
    // we don't want too loose any client requests that are allowed to be sent prior to
    // the first initial commit, e.g. set_maximized or set_fullscreen.
    connect(toplevel, &XdgToplevelInterface::resetOccurred,
            this, [this, toplevel] { createXdgToplevelClient(toplevel); });

    createXdgToplevelClient(toplevel);
}

void WaylandXdgShellIntegration::createXdgToplevelClient(XdgToplevelInterface *toplevel)
{
    if (!workspace()) {
        return; // TODO: Shouldn't we create the client when workspace is initialized?
    }

    emit clientCreated(new XdgToplevelClient(toplevel));
}

void WaylandXdgShellIntegration::registerXdgPopup(XdgPopupInterface *popup)
{
    if (!workspace()) {
        return; // TODO: Shouldn't we create the client when workspace is initialized?
    }

    emit clientCreated(new XdgPopupClient(popup));
}

} // namespace KWin
