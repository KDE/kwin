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
#include <QTimer>

namespace KWin
{

class KWinIdleTimePoller : public AbstractSystemPoller
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AbstractSystemPoller_iid FILE "kwin.json")
    Q_INTERFACES(AbstractSystemPoller)

public:
    KWinIdleTimePoller(QObject *parent = nullptr);
    ~KWinIdleTimePoller() override;

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

private Q_SLOTS:
    void onInhibitedChanged();
    void onTimestampChanged();

private:
    void processActivity();
    QHash<int, QTimer *> m_timeouts;
    bool m_idling = false;
};

}

#endif
