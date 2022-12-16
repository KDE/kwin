/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KIdleTime/private/abstractsystempoller.h>
#include <QHash>
#include <QTimer>

namespace KWin
{

class IdleDetector;

class KWinIdleTimePoller : public AbstractSystemPoller
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AbstractSystemPoller_iid FILE "kwin.json")
    Q_INTERFACES(AbstractSystemPoller)

public:
    KWinIdleTimePoller(QObject *parent = nullptr);

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
    IdleDetector *m_catchResumeTimeout = nullptr;
    QHash<int, IdleDetector *> m_timeouts;
};

}
