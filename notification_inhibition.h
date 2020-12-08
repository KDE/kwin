/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2020 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QVector>

namespace KWin
{
class AbstractClient;

class NotificationInhibition : public QObject
{
    Q_OBJECT
public:
    ~NotificationInhibition() override;

    static NotificationInhibition &self();

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
    explicit NotificationInhibition();

    void inhibit(AbstractClient *client);
    void uninhibit(AbstractClient *client);
    void update(AbstractClient *client);

    QVector<AbstractClient *> m_idleInhibitors;
};

} // namespace KWin
