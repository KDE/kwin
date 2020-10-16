/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_tabboxhandler.h"
#include "mock_tabboxclient.h"

namespace KWin
{

MockTabBoxHandler::MockTabBoxHandler(QObject *parent)
    : TabBoxHandler(parent)
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

QWeakPointer< TabBox::TabBoxClient > MockTabBoxHandler::createMockWindow(const QString &caption)
{
    QSharedPointer< TabBox::TabBoxClient > client(new MockTabBoxClient(caption));
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
