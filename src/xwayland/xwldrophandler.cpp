/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xwldrophandler.h"
#include "databridge.h"
#include "dnd.h"
#include "drag_wl.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

namespace KWin::Xwl
{

XwlDropHandler::XwlDropHandler(Dnd *dnd)
    : KWaylandServer::AbstractDropHandler(dnd)
    , m_dnd(dnd)
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
    for (auto visit : m_previousVisits) {
        if (visit->handleClientMessage(event)) {
            return true;
        }
    }

    if (m_xvisit && m_xvisit->handleClientMessage(event)) {
        return true;
    }
    return false;
}

void XwlDropHandler::updateDragTarget(KWaylandServer::SurfaceInterface *surface, quint32 serial)
{
    auto client = workspace()->findClient([surface](const X11Window *c) {
        return c->surface() == surface;
    });
    if (m_xvisit && client == m_xvisit->target()) {
        return;
    }
    // leave current target
    if (m_xvisit) {
        m_xvisit->leave();
        if (!m_xvisit->finished()) {
            connect(m_xvisit, &Xvisit::finish, this, [this](Xvisit *visit) {
                m_previousVisits.removeOne(visit);
                delete visit;
            });
            m_previousVisits.push_back(m_xvisit);
        } else {
            delete m_xvisit;
        }
        m_xvisit = nullptr;
    }
    if (client) {
        m_xvisit = new Xvisit(client, waylandServer()->seat()->dragSource(), m_dnd, this);
    }
}
}
