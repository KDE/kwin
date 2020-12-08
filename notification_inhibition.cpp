/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "notification_inhibition.h"
#include "activities.h"
#include "abstract_client.h"
#include "deleted.h"
#include "workspace.h"

#include <algorithm>
#include <functional>

namespace KWin
{

NotificationInhibition::NotificationInhibition()
    : QObject()
{
    connect(workspace(), &Workspace::currentDesktopChanged, this, &NotificationInhibition::slotDesktopChanged);
    connect(Activities::self(), &Activities::currentChanged, this, &NotificationInhibition::slotDesktopChanged);
}

NotificationInhibition::~NotificationInhibition() = default;

NotificationInhibition &NotificationInhibition::self()
{
    static NotificationInhibition s_self;
    return s_self;
}

void NotificationInhibition::registerClient(AbstractClient *client)
{
    // FIXME check if we already track

    auto updateInhibit = [this, client] {
        update(client);
    };

    connect(client, &AbstractClient::blocksNotificationsChanged, this, updateInhibit);

    connect(client, &AbstractClient::desktopChanged, this, updateInhibit);
    connect(client, &AbstractClient::activitiesChanged, this, updateInhibit);
    connect(client, &AbstractClient::clientMinimized, this, updateInhibit);
    connect(client, &AbstractClient::clientUnminimized, this, updateInhibit);
    connect(client, &AbstractClient::windowHidden, this, updateInhibit);
    connect(client, &AbstractClient::windowShown, this, updateInhibit);
    connect(client, &AbstractClient::windowClosed, this, [this, client] {
        uninhibit(client);
    });

    updateInhibit();
}

void NotificationInhibition::inhibit(AbstractClient *client)
{
    if (isInhibited(client)) {
        // already inhibited
        return;
    }
    m_idleInhibitors << client;
    // FIXME send call to plasma
}

void NotificationInhibition::uninhibit(AbstractClient *client)
{
    auto it = std::find(m_idleInhibitors.begin(), m_idleInhibitors.end(), client);
    if (it == m_idleInhibitors.end()) {
        // not inhibited
        return;
    }

    m_idleInhibitors.erase(it);
    // FIXME send call to plasma
}

void NotificationInhibition::update(AbstractClient *client)
{
    if (client->isInternal()) {
        return;
    }

    const bool visible = client->isShown(true) && client->isOnCurrentDesktop();
    if (visible && client->isOnCurrentActivity() && client->blocksNotifications()) {
        inhibit(client);
    } else {
        uninhibit(client);
    }
}

void NotificationInhibition::slotWorkspaceCreated()
{
    //connect(workspace(), &Workspace::currentDesktopChanged, this, &NotificationInhibition::slotDesktopChanged);
}

void NotificationInhibition::slotDesktopChanged()
{
    workspace()->forEachAbstractClient([this] (AbstractClient *c) { update(c); });
}

}
