/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
Copyright (C) 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

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
class AbstractClient;

class IdleInhibition : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibition(IdleInterface *idle);
    ~IdleInhibition() override;

    void registerClient(AbstractClient *client);

    bool isInhibited() const {
        return !m_idleInhibitors.isEmpty();
    }
    bool isInhibited(AbstractClient *client) const {
        return m_idleInhibitors.contains(client);
    }

private Q_SLOTS:
    void slotWorkspaceCreated();
    void slotDesktopChanged();

private:
    void inhibit(AbstractClient *client);
    void uninhibit(AbstractClient *client);
    void update(AbstractClient *client);

    IdleInterface *m_idle;
    QVector<AbstractClient *> m_idleInhibitors;
    QMap<AbstractClient *, QMetaObject::Connection> m_connections;
};
}
