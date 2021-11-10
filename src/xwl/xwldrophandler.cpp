/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "databridge.h"
#include "dnd.h"
#include "drag_wl.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11client.h"
#include "xwldrophandler.h"

#include <KWaylandServer/seat_interface.h>

namespace KWin::Xwl {

XwlDropHandler::XwlDropHandler()
    : KWaylandServer::AbstractDropHandler(nullptr)
{
}

void XwlDropHandler::drop()
{
    if (m_xvisit) {
        m_xvisit->drop();
    }
}

bool XwlDropHandler::handleClientMessage(xcb_client_message_event_t *event)
{
    if (m_xvisit && m_xvisit->handleClientMessage(event)) {
        return true;
    }
    return false;
}

void XwlDropHandler::updateDragTarget(KWaylandServer::SurfaceInterface *surface, quint32 serial)
{
    Q_UNUSED(serial)
    auto client = workspace()->findClient([surface](const X11Client *c) {return c->surface() == surface;});
    if (m_xvisit && client == m_xvisit->target()) {
        return;
    }
    // leave current target
    if (m_xvisit) {
        m_xvisit->leave();
        delete m_xvisit;
        m_xvisit = nullptr;
    }
    if (client) {
        m_xvisit = new Xvisit(client, waylandServer()->seat()->dragSource(), this);
    }

}
}
