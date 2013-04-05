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
#include "focuschain.h"
#include "client.h"
#include "screens.h"

namespace KWin
{

KWIN_SINGLETON_FACTORY_VARIABLE(FocusChain, s_manager)

FocusChain::FocusChain(QObject *parent)
    : QObject(parent)
    , m_separateScreenFocus(false)
    , m_activeClient(NULL)
    , m_currentDesktop(0)
{
}

FocusChain::~FocusChain()
{
    s_manager = NULL;
}

void FocusChain::remove(Client *client)
{
    for (DesktopChains::iterator it = m_desktopFocusChains.begin();
            it != m_desktopFocusChains.end();
            ++it) {
        it.value().removeAll(client);
    }
    m_mostRecentlyUsed.removeAll(client);
}

void FocusChain::resize(uint previousSize, uint newSize)
{
    for (uint i = previousSize + 1; i <= newSize; ++i) {
        m_desktopFocusChains.insert(i, QList<Client*>());
    }
    for (uint i = previousSize; i > newSize; --i) {
        m_desktopFocusChains.remove(i);
    }
}

Client *FocusChain::getForActivation(uint desktop) const
{
    return getForActivation(desktop, screens()->current());
}

Client *FocusChain::getForActivation(uint desktop, int screen) const
{
    DesktopChains::const_iterator it = m_desktopFocusChains.find(desktop);
    if (it == m_desktopFocusChains.constEnd()) {
        return NULL;
    }
    const QList<Client*> &chain = it.value();
    for (int i = chain.size() - 1; i >= 0; --i) {
        Client *tmp = chain.at(i);
        // TODO: move the check into Client
        if (tmp->isShown(false) && tmp->isOnCurrentActivity()
            && ( !m_separateScreenFocus || tmp->screen() == screen)) {
            return tmp;
        }
    }
    return NULL;
}

void FocusChain::update(Client *client, FocusChain::Change change)
{
    if (!client->wantsTabFocus()) {
        // Doesn't want tab focus, remove
        remove(client);
        return;
    }

    if (client->isOnAllDesktops()) {
        // Now on all desktops, add it to focus chains it is not already in
        for (DesktopChains::iterator it = m_desktopFocusChains.begin();
                it != m_desktopFocusChains.end();
                ++it) {
            QList<Client*> &chain = it.value();
            // Making first/last works only on current desktop, don't affect all desktops
            if (it.key() == m_currentDesktop
                    && (change == MakeFirst || change == MakeLast)) {
                if (change == MakeFirst) {
                    makeFirstInChain(client, chain);
                } else {
                    makeLastInChain(client, chain);
                }
            } else {
                insertClientIntoChain(client, chain);
            }
        }
    } else {
        // Now only on desktop, remove it anywhere else
        for (DesktopChains::iterator it = m_desktopFocusChains.begin();
                it != m_desktopFocusChains.end();
                ++it) {
            QList<Client*> &chain = it.value();
            if (client->isOnDesktop(it.key())) {
                updateClientInChain(client, change, chain);
            } else {
                chain.removeAll(client);
            }
        }
    }

    // add for most recently used chain
    updateClientInChain(client, change, m_mostRecentlyUsed);
}

void FocusChain::updateClientInChain(Client *client, FocusChain::Change change, QList< Client * >& chain)
{
    if (change == MakeFirst) {
        makeFirstInChain(client, chain);
    } else if (change == MakeLast) {
        makeLastInChain(client, chain);
    } else {
        insertClientIntoChain(client, chain);
    }
}

void FocusChain::insertClientIntoChain(Client *client, QList< Client * >& chain)
{
    if (chain.contains(client)) {
        return;
    }
    if (m_activeClient && m_activeClient != client &&
            !chain.empty() && chain.last() == m_activeClient) {
        // Add it after the active client
        chain.insert(chain.size() - 1, client);
    } else {
        // Otherwise add as the first one
        chain.append(client);
    }
}

void FocusChain::moveAfterClient(Client *client, Client *reference)
{
    if (!client->wantsTabFocus()) {
        return;
    }

    for (DesktopChains::iterator it = m_desktopFocusChains.begin();
            it != m_desktopFocusChains.end();
            ++it) {
        if (!client->isOnDesktop(it.key())) {
            continue;
        }
        moveAfterClientInChain(client, reference, it.value());
    }
    moveAfterClientInChain(client, reference, m_mostRecentlyUsed);
}

void FocusChain::moveAfterClientInChain(Client *client, Client *reference, QList<Client *> &chain)
{
    if (!chain.contains(reference)) {
        return;
    }
    if (Client::belongToSameApplication(reference, client)) {
        chain.removeAll(client);
        chain.insert(chain.indexOf(reference), client);
    } else {
        chain.removeAll(client);
        for (int i = chain.size() - 1; i >= 0; --i) {
            if (Client::belongToSameApplication(reference, chain.at(i))) {
                chain.insert(i, client);
                break;
            }
        }
    }
}

Client *FocusChain::firstMostRecentlyUsed() const
{
    if (m_mostRecentlyUsed.isEmpty()) {
        return NULL;
    }
    return m_mostRecentlyUsed.first();
}

Client *FocusChain::nextMostRecentlyUsed(Client *reference) const
{
    if (m_mostRecentlyUsed.isEmpty()) {
        return NULL;
    }
    const int index = m_mostRecentlyUsed.indexOf(reference);
    if (index == -1 || index == 0) {
        return m_mostRecentlyUsed.last();
    }
    return m_mostRecentlyUsed.at(index - 1);
}

// copied from activation.cpp
bool FocusChain::isUsableFocusCandidate(Client *c, Client *prev) const
{
    return c != prev &&
           c->isShown(false) && c->isOnCurrentDesktop() && c->isOnCurrentActivity() &&
           (!m_separateScreenFocus || c->isOnScreen(prev ? prev->screen() : screens()->current()));
}

Client *FocusChain::nextForDesktop(Client *reference, uint desktop) const
{
    DesktopChains::const_iterator it = m_desktopFocusChains.find(desktop);
    if (it == m_desktopFocusChains.end()) {
        return NULL;
    }
    const QList<Client*> &chain = it.value();
    for (int i = chain.size() - 1; i >= 0; --i) {
        Client* client = chain.at(i);
        if (isUsableFocusCandidate(client, reference)) {
            return client;
        }
    }
    return NULL;
}

void FocusChain::makeFirstInChain(Client *client, QList< Client * >& chain)
{
    chain.removeAll(client);
    if (client->isMinimized()) { // add it before the first minimized ...
        for (int i = chain.count()-1; i >= 0; --i) {
            if (chain.at(i)->isMinimized()) {
                chain.insert(i+1, client);
                return;
            }
        }
        chain.prepend(client); // ... or at end of chain
    } else {
        chain.append(client);
    }
}

void FocusChain::makeLastInChain(Client *client, QList< Client * >& chain)
{
    chain.removeAll(client);
    chain.prepend(client);
}

bool FocusChain::contains(Client *client, uint desktop) const
{
    DesktopChains::const_iterator it = m_desktopFocusChains.find(desktop);
    if (it == m_desktopFocusChains.end()) {
        return false;
    }
    return it.value().contains(client);
}

} // namespace
