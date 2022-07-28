/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "poller.h"
#include "idledetector.h"
#include "input.h"

namespace KWin
{

namespace
{

auto nowInMs()
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
}
}

KWinIdleTimePoller::KWinIdleTimePoller(QObject *parent)
    : AbstractSystemPoller(parent)
    , m_idleTimePoller(new IdleDetector(std::chrono::milliseconds(1000), this))
{
    connect(m_idleTimePoller, &IdleDetector::resumed, this, [this]() {
        m_lastIdleTime = std::nullopt;
    });
    connect(m_idleTimePoller, &IdleDetector::idle, this, [this]() {
        m_lastIdleTime = nowInMs();
    });
}

bool KWinIdleTimePoller::isAvailable()
{
    return true;
}

bool KWinIdleTimePoller::setUpPoller()
{
    return true;
}

void KWinIdleTimePoller::unloadPoller()
{
}

void KWinIdleTimePoller::addTimeout(int nextTimeout)
{
    if (m_timeouts.contains(nextTimeout)) {
        return;
    }

    auto detector = new IdleDetector(std::chrono::milliseconds(nextTimeout), this);
    m_timeouts.insert(nextTimeout, detector);
    connect(detector, &IdleDetector::idle, this, [this, nextTimeout] {
        Q_EMIT timeoutReached(nextTimeout);
    });
    connect(detector, &IdleDetector::resumed, this, &KWinIdleTimePoller::resumingFromIdle);
}

void KWinIdleTimePoller::removeTimeout(int nextTimeout)
{
    delete m_timeouts.take(nextTimeout);
}

QList< int > KWinIdleTimePoller::timeouts() const
{
    return m_timeouts.keys();
}

void KWinIdleTimePoller::catchIdleEvent()
{
    if (m_catchResumeTimeout) {
        // already setup
        return;
    }
    m_catchResumeTimeout = new IdleDetector(std::chrono::milliseconds::zero(), this);
    connect(m_catchResumeTimeout, &IdleDetector::resumed, this, [this]() {
        m_catchResumeTimeout->deleteLater();
        m_catchResumeTimeout = nullptr;
        Q_EMIT resumingFromIdle();
    });
}

void KWinIdleTimePoller::stopCatchingIdleEvents()
{
    delete m_catchResumeTimeout;
    m_catchResumeTimeout = nullptr;
}

int KWinIdleTimePoller::forcePollRequest()
{
    if (m_idleTimePoller->isInhibited()) {
        return 0;
    }

    if (m_lastIdleTime.has_value()) {
        std::chrono::milliseconds diff = nowInMs() - *m_lastIdleTime;
        return diff.count();
    }
    return 0;
}

void KWinIdleTimePoller::simulateUserActivity()
{
    input()->simulateUserActivity();
}

} // namespace KWin
