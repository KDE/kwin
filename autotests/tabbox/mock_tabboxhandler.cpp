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
}

std::weak_ptr<TabBox::TabBoxClient> MockTabBoxHandler::activeClient() const
{
    return m_activeClient;
}

void MockTabBoxHandler::setActiveClient(const std::weak_ptr<TabBox::TabBoxClient> &client)
{
    m_activeClient = client;
}

std::weak_ptr<TabBox::TabBoxClient> MockTabBoxHandler::clientToAddToList(TabBox::TabBoxClient *client, int desktop) const
{
    QList<std::shared_ptr<TabBox::TabBoxClient>>::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).get() == client) {
            return std::weak_ptr<TabBox::TabBoxClient>(*it);
        }
    }
    return std::weak_ptr<TabBox::TabBoxClient>();
}

std::weak_ptr<TabBox::TabBoxClient> MockTabBoxHandler::nextClientFocusChain(TabBox::TabBoxClient *client) const
{
    QList<std::shared_ptr<TabBox::TabBoxClient>>::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).get() == client) {
            ++it;
            if (it == m_windows.constEnd()) {
                return std::weak_ptr<TabBox::TabBoxClient>(m_windows.first());
            } else {
                return std::weak_ptr<TabBox::TabBoxClient>(*it);
            }
        }
    }
    if (!m_windows.isEmpty()) {
        return std::weak_ptr<TabBox::TabBoxClient>(m_windows.last());
    }
    return std::weak_ptr<TabBox::TabBoxClient>();
}

std::weak_ptr<TabBox::TabBoxClient> MockTabBoxHandler::firstClientFocusChain() const
{
    if (m_windows.isEmpty()) {
        return std::weak_ptr<TabBox::TabBoxClient>();
    }
    return m_windows.first();
}

bool MockTabBoxHandler::isInFocusChain(TabBox::TabBoxClient *client) const
{
    if (!client) {
        return false;
    }
    QList<std::shared_ptr<TabBox::TabBoxClient>>::const_iterator it = m_windows.constBegin();
    for (; it != m_windows.constEnd(); ++it) {
        if ((*it).get() == client) {
            return true;
        }
    }
    return false;
}

std::weak_ptr<TabBox::TabBoxClient> MockTabBoxHandler::createMockWindow(const QString &caption)
{
    std::shared_ptr<TabBox::TabBoxClient> client(new MockTabBoxClient(caption));
    m_windows.append(client);
    m_activeClient = client;
    return std::weak_ptr<TabBox::TabBoxClient>(client);
}

void MockTabBoxHandler::closeWindow(TabBox::TabBoxClient *client)
{
    QList<std::shared_ptr<TabBox::TabBoxClient>>::iterator it = m_windows.begin();
    for (; it != m_windows.end(); ++it) {
        if ((*it).get() == client) {
            m_windows.erase(it);
            return;
        }
    }
}

} // namespace KWin
