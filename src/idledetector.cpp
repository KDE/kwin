/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "idledetector.h"
#include "input.h"

namespace KWin
{

IdleDetector::IdleDetector(std::chrono::milliseconds timeout, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(timeout);
    connect(m_timer, &QTimer::timeout, this, &IdleDetector::markAsIdle);
    m_timer->start();

    input()->addIdleDetector(this);
}

IdleDetector::~IdleDetector()
{
    if (input()) {
        input()->removeIdleDetector(this);
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
        m_timer->stop();
    } else {
        m_timer->start();
    }
}

void IdleDetector::activity()
{
    if (!m_isInhibited) {
        m_timer->start();
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
