/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#pragma once

#include <QObject>
#include <QVector>
#include <QMap>

#include <algorithm>

namespace KWayland
{
namespace Server
{
class IdleInterface;
}
}

using KWayland::Server::IdleInterface;

namespace KWin
{
class ShellClient;

class IdleInhibition : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibition(IdleInterface *idle);
    ~IdleInhibition();

    void registerShellClient(ShellClient *client);

    bool isInhibited() const {
        return !m_idleInhibitors.isEmpty();
    }
    bool isInhibited(ShellClient *client) const {
        return std::any_of(m_idleInhibitors.begin(), m_idleInhibitors.end(), [client] (auto c) { return c == client; });
    }

private:
    void inhibit(ShellClient *client);
    void uninhibit(ShellClient *client);

    IdleInterface *m_idle;
    QVector<ShellClient*> m_idleInhibitors;
    QMap<ShellClient*, QMetaObject::Connection> m_connections;
};
}
