/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QVector>
#include <QMap>

namespace KWaylandServer
{
class IdleInterface;
}

using KWaylandServer::IdleInterface;

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
