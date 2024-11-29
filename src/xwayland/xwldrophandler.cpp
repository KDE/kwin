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
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

namespace KWin::Xwl
{

XwlDropHandler::XwlDropHandler(Dnd *dnd)
    : AbstractDropHandler(dnd)
    , m_dnd(dnd)
{
}

void XwlDropHandler::drop()
{
    if (m_currentVisit) {
        m_currentVisit->drop();
    }
}

bool XwlDropHandler::handleClientMessage(xcb_client_message_event_t *event)
{
    for (auto visit : m_visits) {
        if (visit->handleClientMessage(event)) {
            return true;
        }
    }

    return false;
}

void XwlDropHandler::updateDragTarget(SurfaceInterface *surface, quint32 serial)
{
    auto window = workspace()->findClient([surface](const X11Window *c) {
        return c->surface() == surface;
    });
    if (m_currentVisit && m_currentVisit->target() == window) {
        return;
    }

    if (m_currentVisit) {
        m_currentVisit->leave();
        m_currentVisit = nullptr;
    }

    if (window) {
        auto visit = new Xvisit(window, waylandServer()->seat()->dragSource(), m_dnd, this);
        if (visit->isFinished()) {
            delete visit;
            return;
        }

        connect(visit, &Xvisit::finished, this, [this, visit]() {
            m_visits.removeOne(visit);
            if (m_currentVisit == visit) {
                m_currentVisit = nullptr;
            }

            delete visit;
        });

        m_currentVisit = visit;
        m_visits.append(visit);
    }
}
}

#include "moc_xwldrophandler.cpp"
