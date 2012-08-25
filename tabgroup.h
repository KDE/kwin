/*******************************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2011/2012 The KWin team <kwin@kde.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#ifndef KWIN_TABGROUP_H
#define KWIN_TABGROUP_H

#include <QObject>

#include "kdecoration.h"
#include "utils.h"

namespace KWin
{

class Client;

/**
 * This class represents a group of clients for use in window tabbing. All
 * clients in the group share the same geometry and state information; I.e if
 * one client changes then all others should also be changed.
 *
 * A group contains at least one client and DOES NOT contain multiple
 * copies of the same client. A client MUST NOT be in two groups at the same
 * time. All decorated clients SHOULD be in a group, even if it's a group of
 * one client.
 *
 * rohanp: Had to convert this object to a QObject to make it easier for adding
 * scripting interface to TabGroup.
 *
 * If a group contains multiple clients then only one will ever be mapped at
 * any given time.
 */
class TabGroup
{
public:
    /**
     * Creates a new group containing \p c.
     */
    TabGroup(Client* c);
    ~TabGroup();

    enum State {
        None = 0, Minimized = 1<<0, Maximized = 1<<1, Shaded = 1<<2,
        Geometry = 1<<3, Desktop = 1<<4, Activity = 1<<5,
        Layer = 1<<6, QuickTile = 1<<7, All = 0xffffffff
    };
    Q_DECLARE_FLAGS(States, State)

    /**
     * Activate next tab (flips)
     */
    void activateNext();

    /**
     * Activate previous tab (flips)
     */
    void activatePrev();

    /**
     * Allows to alter several attributes in random order and trigger a general update at the end
     * (must still be explicitly called)
     * this is to prevent side effects, mostly for geometry adjustments during maximization and QuickTiling
     */
    void blockStateUpdates(bool);

    /**
     * Close all clients in this group.
     */
    void closeAll();

    /**
     * Whether client \p c is member of this group
     */
    bool contains(Client* c) const;

    /**
     * The amount of clients in this group
     */
    int count() const;

    /**
     * Returns whether or not this group contains the active client.
     */
    bool isActive() const;

    /**
     * Returns whether this group is empty (used by workspace to remove it)
     */
    bool isEmpty() const;

    /**
     * Returns the list of all the clients contained in this group in their current order.
     */
    const ClientList &clients() const;

    /**
     * Returns the currently visible client.
     */
    Client* current() const;
    /**
     * Makes \p c the visible client in the group - force is only used when the window becomes ready for painting.
     * Any other usage just causes pointless action
     */
    void setCurrent(Client* c, bool force = false);

    /**
     * Alignes the dynamic Qt @param property of all clients to the one of @param c
     */
    void sync(const char *property, Client *c);

    /**
     * Returns combined minimum size of all clients in the group.
     */
    QSize minSize() const;
    /**
     * Returns combined maximum size of all clients in the group.
     */
    QSize maxSize() const;

    /**
     * Ensures that all the clients in the group have identical geometries and states using
     * \p main as the primary client to copy the settings off. If \p only is set then only
     * that client is updated to match \p main.
     */
    void updateStates(Client* main, States states, Client* only = NULL);

    /**
     * updates geometry restrictions of this group, basically called from Client::getWmNormalHints(), otherwise rather private
     */
    void updateMinMaxSize();

signals:
    void minSizeChanged();
    void maxSizeChanged();

private:
    friend class Client;
//     friend bool Client::tabTo(Client*, bool, bool);
    bool add(KWin::Client *c, Client *other, bool behind, bool activateC);
    void move(KWin::Client* c, KWin::Client* before, bool behind);

//     friend bool Client::untab(const QRect&);
    bool remove(KWin::Client *c);

    ClientList m_clients;
    Client *m_current;
    QSize m_minSize;
    QSize m_maxSize;
    int m_stateUpdatesBlocked;
    States m_pendingUpdates;
};

inline bool TabGroup::contains(Client* c) const
{
    return c && m_clients.contains(c);
}

inline int TabGroup::count() const
{
    return m_clients.count();
}

inline const ClientList &TabGroup::clients() const
{
    return m_clients;
}

inline bool TabGroup::isEmpty() const
{
    return m_clients.isEmpty();
}

inline Client* TabGroup::current() const
{
    return m_current;
}

inline QSize TabGroup::minSize() const
{
    return m_minSize;
}

inline QSize TabGroup::maxSize() const
{
    return m_maxSize;
}

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::TabGroup::States)

#endif
