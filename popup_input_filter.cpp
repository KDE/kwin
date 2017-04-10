/*
 * Copyright 2017  Martin Graesslin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "popup_input_filter.h"
#include "deleted.h"
#include "shell_client.h"
#include "wayland_server.h"

#include <QMouseEvent>

namespace KWin
{

PopupInputFilter::PopupInputFilter()
    : QObject()
{
    connect(waylandServer(), &WaylandServer::shellClientAdded, this, &PopupInputFilter::handleClientAdded);
}

void PopupInputFilter::handleClientAdded(Toplevel *client)
{
    if (m_popupClients.contains(client)) {
        return;
    }
    if (client->hasPopupGrab()) {
        // TODO: verify that the Toplevel is allowed as a popup
        connect(client, &Toplevel::windowShown, this, &PopupInputFilter::handleClientAdded, Qt::UniqueConnection);
        connect(client, &Toplevel::windowClosed, this, &PopupInputFilter::handleClientRemoved, Qt::UniqueConnection);
        m_popupClients << client;
    }
}

void PopupInputFilter::handleClientRemoved(Toplevel *client)
{
    m_popupClients.removeOne(client);
}
bool PopupInputFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(nativeButton)
    if (m_popupClients.isEmpty()) {
        return false;
    }
    if (event->type() == QMouseEvent::MouseButtonPress) {
        auto pointerFocus = qobject_cast<AbstractClient*>(input()->findToplevel(event->globalPos()));
        if (!pointerFocus || !AbstractClient::belongToSameApplication(pointerFocus, qobject_cast<AbstractClient*>(m_popupClients.constLast()))) {
            // a press on a window (or no window) not belonging to the popup window
            cancelPopups();
            // filter out this press
            return true;
        }
        if (pointerFocus && pointerFocus->isDecorated()) {
            // test whether it is on the decoration
            const QRect clientRect = QRect(pointerFocus->clientPos(), pointerFocus->clientSize()).translated(pointerFocus->pos());
            if (!clientRect.contains(event->globalPos())) {
                cancelPopups();
                return true;
            }
        }
    }
    return false;
}

void PopupInputFilter::cancelPopups()
{
    while (!m_popupClients.isEmpty()) {
        auto c = m_popupClients.takeLast();
        c->popupDone();
    }
}

}
