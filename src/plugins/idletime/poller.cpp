/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "poller.h"

#include <KIdleTime>

#include "wayland_server.h"
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/idle_interface.h>


namespace KWin
{

KWinIdleTimePoller::KWinIdleTimePoller(QObject *parent)
    : AbstractSystemPoller(parent)
{
}

KWinIdleTimePoller::~KWinIdleTimePoller() = default;

bool KWinIdleTimePoller::isAvailable()
{
    return true;
}

bool KWinIdleTimePoller::setUpPoller()
{
    connect(waylandServer()->idle(), &KWaylandServer::IdleInterface::inhibitedChanged, this, &KWinIdleTimePoller::onInhibitedChanged);
    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::timestampChanged, this, &KWinIdleTimePoller::onTimestampChanged);

    return true;
}

void KWinIdleTimePoller::unloadPoller()
{
    if (waylandServer() && waylandServer()->idle()) {
        disconnect(waylandServer()->idle(), &KWaylandServer::IdleInterface::inhibitedChanged, this, &KWinIdleTimePoller::onInhibitedChanged);
        disconnect(waylandServer()->seat(), &KWaylandServer::SeatInterface::timestampChanged, this, &KWinIdleTimePoller::onTimestampChanged);
    }

    qDeleteAll(m_timeouts);
    m_timeouts.clear();

    m_idling = false;
}

void KWinIdleTimePoller::addTimeout(int newTimeout)
{
    if (m_timeouts.contains(newTimeout)) {
        return;
    }

    auto timer = new QTimer();
    timer->setInterval(newTimeout);
    timer->setSingleShot(true);
    timer->callOnTimeout(this, [newTimeout, this](){
        m_idling = true;
        Q_EMIT timeoutReached(newTimeout);
    });

    m_timeouts.insert(newTimeout, timer);

    if (!waylandServer()->idle()->isInhibited()) {
        timer->start();
    }
}

void KWinIdleTimePoller::processActivity()
{
    if (m_idling) {
        Q_EMIT resumingFromIdle();
        m_idling = false;
    }

    for (QTimer *timer : qAsConst(m_timeouts)) {
        timer->start();
    }
}

void KWinIdleTimePoller::onInhibitedChanged()
{
    if (waylandServer()->idle()->isInhibited()) {
        // must stop the timers
        stopCatchingIdleEvents();
    } else {
        // resume the timers
        catchIdleEvent();

        // register some activity
        Q_EMIT resumingFromIdle();
    }
}

void KWinIdleTimePoller::onTimestampChanged()
{
    if (!waylandServer()->idle()->isInhibited()) {
        processActivity();
    }
}

void KWinIdleTimePoller::catchIdleEvent()
{
    for (QTimer *timer : qAsConst(m_timeouts)) {
        timer->start();
    }
}

void KWinIdleTimePoller::stopCatchingIdleEvents()
{
    for (QTimer *timer : qAsConst(m_timeouts)) {
        timer->stop();
    }
}

void KWinIdleTimePoller::simulateUserActivity()
{
    if (waylandServer()->idle()->isInhibited()) {
        return ;
    }
    processActivity();
    waylandServer()->simulateUserActivity();
}

void KWinIdleTimePoller::removeTimeout(int nextTimeout)
{
    delete m_timeouts.take(nextTimeout);
}

QList< int > KWinIdleTimePoller::timeouts() const
{
    return m_timeouts.keys();
}

int KWinIdleTimePoller::forcePollRequest()
{
    return 0;
}

}

#include "poller.moc"
