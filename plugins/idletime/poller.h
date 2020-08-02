/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef POLLER_H
#define POLLER_H

#include <KIdleTime/private/abstractsystempoller.h>

#include <QHash>

namespace KWayland
{
namespace Client
{
class Seat;
class Idle;
class IdleTimeout;
}
}

class Poller : public AbstractSystemPoller
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kidletime.AbstractSystemPoller" FILE "kwin.json")
    Q_INTERFACES(AbstractSystemPoller)

public:
    Poller(QObject *parent = nullptr);
    ~Poller() override;

    bool isAvailable() override;
    bool setUpPoller() override;
    void unloadPoller() override;

public Q_SLOTS:
    void addTimeout(int nextTimeout) override;
    void removeTimeout(int nextTimeout) override;
    QList<int> timeouts() const override;
    int forcePollRequest() override;
    void catchIdleEvent() override;
    void stopCatchingIdleEvents() override;
    void simulateUserActivity() override;

private:
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Idle *m_idle = nullptr;
    KWayland::Client::IdleTimeout *m_catchResumeTimeout = nullptr;
    QHash<int, KWayland::Client::IdleTimeout*> m_timeouts;
};

#endif
