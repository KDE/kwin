/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "idle_inhibition.h"
#include "abstract_client.h"
#include "deleted.h"
#include "workspace.h"

#include <KWaylandServer/idle_interface.h>
#include <KWaylandServer/surface_interface.h>

#include <algorithm>
#include <functional>

using KWaylandServer::SurfaceInterface;

namespace KWin
{

IdleInhibition::IdleInhibition(IdleInterface *idle)
    : QObject(idle)
    , m_idle(idle)
{
    // Workspace is created after the wayland server is initialized.
    connect(kwinApp(), &Application::workspaceCreated, this, &IdleInhibition::slotWorkspaceCreated);
}

IdleInhibition::~IdleInhibition() = default;

void IdleInhibition::registerClient(AbstractClient *client)
{
    auto updateInhibit = [this, client] {
        update(client);
    };

    m_connections[client] = connect(client->surface(), &SurfaceInterface::inhibitsIdleChanged, this, updateInhibit);
    connect(client, &AbstractClient::desktopChanged, this, updateInhibit);
    connect(client, &AbstractClient::clientMinimized, this, updateInhibit);
    connect(client, &AbstractClient::clientUnminimized, this, updateInhibit);
    connect(client, &AbstractClient::windowHidden, this, updateInhibit);
    connect(client, &AbstractClient::windowShown, this, updateInhibit);
    connect(client, &AbstractClient::windowClosed, this,
        [this, client] {
            uninhibit(client);
            auto it = m_connections.find(client);
            if (it != m_connections.end()) {
                disconnect(it.value());
                m_connections.erase(it);
            }
        }
    );

    updateInhibit();
}

void IdleInhibition::inhibit(AbstractClient *client)
{
    if (isInhibited(client)) {
        // already inhibited
        return;
    }
    m_idleInhibitors << client;
    m_idle->inhibit();
    // TODO: notify powerdevil?
}

void IdleInhibition::uninhibit(AbstractClient *client)
{
    auto it = std::find(m_idleInhibitors.begin(), m_idleInhibitors.end(), client);
    if (it == m_idleInhibitors.end()) {
        // not inhibited
        return;
    }
    m_idleInhibitors.erase(it);
    m_idle->uninhibit();
}

void IdleInhibition::update(AbstractClient *client)
{
    if (client->isInternal()) {
        return;
    }

    // TODO: Don't honor the idle inhibitor object if the shell client is not
    // on the current activity (currently, activities are not supported).
    const bool visible = client->isShown(true) && client->isOnCurrentDesktop();
    if (visible && client->surface() && client->surface()->inhibitsIdle()) {
        inhibit(client);
    } else {
        uninhibit(client);
    }
}

void IdleInhibition::slotWorkspaceCreated()
{
    connect(workspace(), &Workspace::currentDesktopChanged, this, &IdleInhibition::slotDesktopChanged);
}

void IdleInhibition::slotDesktopChanged()
{
    workspace()->forEachAbstractClient([this] (AbstractClient *c) { update(c); });
}

}
