/*******************************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2009 Jorge Mata <matamax123@gmail.com>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_CLIENTGROUP_H
#define KWIN_CLIENTGROUP_H

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
 * Workspace::clientGroups is a list of all currently-existing client groups.
 *
 * A group MUST contain at least one client and MUST NOT contain multiple
 * copies of the same client. A client MUST NOT be in two groups at the same
 * time. All decorated clients SHOULD be in a group, even if it's a group of
 * one client.
 *
 * rohanp: Had to convert this object to a QObject to make it easier for adding
 * scripting interface to ClientGroup.
 *
 * If a group contains multiple clients then only one will ever be mapped at
 * any given time.
 */
class ClientGroup : public QObject
{
    Q_OBJECT
    /**
     * Currently visible client in this group.
     **/
    Q_PROPERTY(KWin::Client* visible READ visible WRITE setVisible NOTIFY visibleChanged)
    /**
     * Combined minimum size of all clients in the group.
     **/
    Q_PROPERTY(QSize minSize READ minSize NOTIFY minSizeChanged)
    /**
     * Combined maximum size of all clients in the group.
     **/
    Q_PROPERTY(QSize maxSize READ maxSize NOTIFY maxSizeChanged)
    /**
     * The index of the visible Client in this group.
     **/
    Q_PROPERTY(int visibleClientIndex READ indexOfVisibleClient NOTIFY visibleChanged)
    /**
     * The Clients in this group.
     **/
    Q_PROPERTY(QList<KWin::Client*> clients READ clients)
public:
    /**
     * Creates a new group containing \p c.
     */
    ClientGroup(Client* c);
    ~ClientGroup();

public Q_SLOTS:
    /**
     * Adds \p c to the group before \p before in the list. If \p becomeVisible is \i true then
     * the added client will become also the visible client.
     */
    void add(KWin::Client* c, int before = -1, bool becomeVisible = false);
    /**
     * Remove the client at index \p index from the group. If \p newGeom is set then the client
     * will move and resize to the specified geometry, otherwise it will stay where the group
     * is located. If \p toNullGroup is not true then the client will be added to a new group
     * of its own.
     */
    void remove(int index, const QRect& newGeom = QRect(), bool toNullGroup = false);
    /**
     * Remove \p c from the group. If \p newGeom is set then the client will move and resize to
     * the specified geometry, otherwise it will stay where the group is located. If
     * \p toNullGroup is not true then the client will be added to a new group of its own.
     */
    void remove(KWin::Client* c, const QRect& newGeom = QRect(), bool toNullGroup = false);
    /**
     * Remove all clients from this group. Results in all clients except the first being moved
       to a group of their own.
     */
    void removeAll();
    /**
     * Close all clients in this group.
     */
    void closeAll();
    /**
     * Move the client at index \p index to the position before the client at index \p before
     * in the list.
     */
    void move(int index, int before);
    /**
     * Move \p c to the position before \p before in the list.
     */
    void move(KWin::Client* c, KWin::Client* before);
    /**
     * Display the right-click client menu belonging to the client at index \p index at the
     * global coordinates specified by \p pos.
     */
    void displayClientMenu(int index, const QPoint& pos);
    /**
     * Display the right-click client menu belonging to \p c at the global coordinates
     * specified by \p pos.
     */
    void displayClientMenu(KWin::Client* c, const QPoint& pos);

public:
    /**
     * Returns the list index of \p c.
     */
    Q_SCRIPTABLE int indexOfClient(KWin::Client* c);
    /**
     * Returns the list index of the currently visible client in the group.
     */
    int indexOfVisibleClient();
    /**
     * Returns whether or not this group contains \p c.
     */
    Q_SCRIPTABLE bool contains(KWin::Client* c);
    /**
     * Returns whether or not this group contains the active client.
     */
    bool containsActiveClient();

    /**
     * Returns the list of all the clients contained in this group in their current order.
     */
    const ClientList &clients() const;
    /**
     * Returns a list of the captions and icons of all the clients contained in this group
     * in their current order.
     */
    QList< ClientGroupItem > items() const;

    /**
     * Returns the currently visible client.
     */
    Client* visible();
    /**
     * Makes the client at index \p index the visible one in the group.
     */
    void setVisible(int index);
    /**
     * Makes \p c the visible client in the group.
     */
    void setVisible(Client* c);

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
    void updateStates(Client* main, Client* only = NULL);

Q_SIGNALS:
    /**
     * Emitted when the visible Client in this group changes.
     **/
    void visibleChanged();
    /**
     * Emitted when the group's minimum size changes.
     **/
    void minSizeChanged();
    /**
     * Emitted when the group's maximum size changes.
     **/
    void maxSizeChanged();

private:
    /**
     * Regenerate the list of client captions and icons.
     */
    void updateItems();
    /**
     * Determine the combined minimum and maximum sizes of all clients in the group.
     */
    void updateMinMaxSize();

    ClientList clients_;
    QList< ClientGroupItem > items_;
    int visible_;

    QSize minSize_;
    QSize maxSize_;

    friend class Client;
};

inline int ClientGroup::indexOfClient(Client* c)
{
    return clients_.indexOf(c);
}

inline int ClientGroup::indexOfVisibleClient()
{
    return visible_;
}

inline bool ClientGroup::contains(Client* c)
{
    return clients_.contains(c);
}

inline const ClientList &ClientGroup::clients() const
{
    return clients_;
}

inline QList< ClientGroupItem > ClientGroup::items() const
{
    return items_;
}

inline Client* ClientGroup::visible()
{
    return clients_.at(visible_);
}

inline QSize ClientGroup::minSize() const
{
    return minSize_;
}

inline QSize ClientGroup::maxSize() const
{
    return maxSize_;
}

}
Q_DECLARE_METATYPE(KWin::ClientGroup*)
Q_DECLARE_METATYPE(QList<KWin::ClientGroup*>)

#endif
