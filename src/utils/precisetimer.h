/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 Jakub Okoński <jakub@okonski.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "filedescriptor.h"

#include <QObject>
#include <QSocketNotifier>

namespace KWin
{

/**
 * Like QChronoTimer, but single-shot and targeting wakeup deadlines with 1 nanosecond granularity.
 * It doesn't provide Qt::TimerIds.
 * Used a timerfd wrapped in a QSocketNotifier internally.
 */
class PreciseTimer : public QObject
{
    Q_OBJECT

public:
    explicit PreciseTimer();

    /**
     * Starts the timer with the deadline passed in the argument.
     *
     * If the timer is already running, it will be stopped and restarted.
     */
    void start(std::chrono::nanoseconds deadline);

    /**
     * Stops the timer if it was already running, no-op otherwise.
     */
    void stop();

    bool isActive() const;

Q_SIGNALS:
    void timeout();

private:
    void timerFdReadable();
    void drain();

    FileDescriptor m_timerFd;
    std::unique_ptr<QSocketNotifier> m_notifier;
    bool m_active = false;
};

}
