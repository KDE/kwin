/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "idledetector.h"
#include "input.h"

using namespace std::chrono_literals;

namespace KWin
{

IdleDetector::IdleDetector(std::chrono::milliseconds timeout, QObject *parent)
    : QObject(parent)
    , m_timeout(timeout)
{
    Q_ASSERT(timeout >= 0ms);
    m_timer.start(timeout, this);

    input()->addIdleDetector(this);
}

IdleDetector::~IdleDetector()
{
    if (input()) {
        input()->removeIdleDetector(this);
    }
}

void IdleDetector::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_timer.timerId()) {
        m_timer.stop();
        markAsIdle();
    }
}

bool IdleDetector::isInhibited() const
{
    return m_isInhibited;
}

void IdleDetector::setInhibited(bool inhibited)
{
    if (m_isInhibited == inhibited) {
        return;
    }
    m_isInhibited = inhibited;
    if (inhibited) {
        m_timer.stop();
    } else {
        m_timer.start(m_timeout, this);
    }
}

void IdleDetector::activity()
{
    if (!m_isInhibited) {
        m_timer.start(m_timeout, this);
        markAsResumed();
    }
}

void IdleDetector::markAsIdle()
{
    if (!m_isIdle) {
        m_isIdle = true;
        Q_EMIT idle();
    }
}

void IdleDetector::markAsResumed()
{
    if (m_isIdle) {
        m_isIdle = false;
        Q_EMIT resumed();
    }
}

} // namespace KWin

#include "moc_idledetector.cpp"
