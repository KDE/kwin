/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "poller.h"
#include "../../wayland_server.h"

#include <KWayland/Client/idle.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>

Poller::Poller(QObject *parent)
    : AbstractSystemPoller(parent)
{
    connect(KWin::waylandServer(), &KWin::WaylandServer::terminatingInternalClientConnection, this,
        [this] {
            qDeleteAll(m_timeouts);
            m_timeouts.clear();
            delete m_seat;
            m_seat = nullptr;
            delete m_idle;
            m_idle = nullptr;
        }
    );
}

Poller::~Poller() = default;

bool Poller::isAvailable()
{
    return true;
}

bool Poller::setUpPoller()
{
    auto registry = KWin::waylandServer()->internalClientRegistry();
    if (!m_seat) {
        const auto iface = registry->interface(KWayland::Client::Registry::Interface::Seat);
        m_seat = registry->createSeat(iface.name, iface.version, this);
    }
    if (!m_idle) {
        const auto iface = registry->interface(KWayland::Client::Registry::Interface::Idle);
        m_idle = registry->createIdle(iface.name, iface.version, this);
    }
    return m_seat->isValid() && m_idle->isValid();
}

void Poller::unloadPoller()
{
}

void Poller::addTimeout(int nextTimeout)
{
    if (m_timeouts.contains(nextTimeout)) {
        return;
    }
    if (!m_idle) {
        return;
    }
    auto timeout = m_idle->getTimeout(nextTimeout, m_seat, this);
    m_timeouts.insert(nextTimeout, timeout);
    connect(timeout, &KWayland::Client::IdleTimeout::idle, this,
        [this, nextTimeout] {
            emit timeoutReached(nextTimeout);
        }
    );
    connect(timeout, &KWayland::Client::IdleTimeout::resumeFromIdle, this, &Poller::resumingFromIdle);
}

void Poller::removeTimeout(int nextTimeout)
{
    auto it = m_timeouts.find(nextTimeout);
    if (it == m_timeouts.end()) {
        return;
    }
    delete it.value();
    m_timeouts.erase(it);
}

QList< int > Poller::timeouts() const
{
    return QList<int>();
}

void Poller::catchIdleEvent()
{
    if (m_catchResumeTimeout) {
        // already setup
        return;
    }
    if (!m_idle) {
        return;
    }
    m_catchResumeTimeout = m_idle->getTimeout(0, m_seat, this);
    connect(m_catchResumeTimeout, &KWayland::Client::IdleTimeout::resumeFromIdle, this,
        [this] {
            stopCatchingIdleEvents();
            emit resumingFromIdle();
        }
    );
}

void Poller::stopCatchingIdleEvents()
{
    delete m_catchResumeTimeout;
    m_catchResumeTimeout = nullptr;
}

int Poller::forcePollRequest()
{
    return 0;
}

void Poller::simulateUserActivity()
{
    for (auto it = m_timeouts.constBegin(); it != m_timeouts.constEnd(); ++it) {
        it.value()->simulateUserActivity();
    }
}
