/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_tabboxhandler.h"
#include "mock_tabboxclient.h"

namespace KWin
{

MockTabBoxHandler::MockTabBoxHandler()
    : TabBoxHandler()
{
}

MockTabBoxHandler::~MockTabBoxHandler()
{
}

void MockTabBoxHandler::grabbedKeyEvent(QKeyEvent *event) const
{
    Q_UNUSED(event)
}

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::activeClient() const
{
    return m_activeClient;
}

void MockTabBoxHandler::setActiveClient(const QWeakPointer< TabBox::TabBoxClient >& client)
{
    m_activeClient = client;
}

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::clientToAddToList(TabBox::TabBoxClient *client, int desktop) const
{
    Q_UNUSED(desktop)
    QList< QSharedPointer< TabBox::TabBoxClient > >::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).data() == client) {
            return QWeakPointer< TabBox::TabBoxClient >(*it);
        }
    }
    return QWeakPointer< TabBox::TabBoxClient >();
}

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::nextClientFocusChain(TabBox::TabBoxClient *client) const
{
    QList< QSharedPointer< TabBox::TabBoxClient > >::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).data() == client) {
            ++it;
            if (it == m_windows.constEnd()) {
                return QWeakPointer< TabBox::TabBoxClient >(m_windows.first());
            } else {
                return QWeakPointer< TabBox::TabBoxClient >(*it);
            }
        }
    }
    if (!m_windows.isEmpty()) {
        return QWeakPointer< TabBox::TabBoxClient >(m_windows.last());
    }
    return QWeakPointer< TabBox::TabBoxClient >();
}

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::firstClientFocusChain() const
{
    if (m_windows.isEmpty()) {
        return QWeakPointer<TabBox::TabBoxClient>();
    }
    return m_windows.first();
}

bool MockTabBoxHandler::isInFocusChain(TabBox::TabBoxClient *client) const
{
    if (!client) {
        return false;
    }
    QList< QSharedPointer< TabBox::TabBoxClient > >::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).data() == client) {
            return true;
        }
    }
    return false;
}

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::createMockWindow(const QString &caption, WId id)
{
    QSharedPointer< TabBox::TabBoxClient > client(new MockTabBoxClient(caption, id));
    m_windows.append(client);
    m_activeClient = client;
    return QWeakPointer< TabBox::TabBoxClient >(client);
}

void MockTabBoxHandler::closeWindow(TabBox::TabBoxClient *client)
{
    QList< QSharedPointer< TabBox::TabBoxClient > >::iterator it = m_windows.begin();
    for (; it != m_windows.end(); ++it) {
        if ((*it).data() == client) {
            m_windows.erase(it);
            return;
        }
    }
}

} // namespace KWin
