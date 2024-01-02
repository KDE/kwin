/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "utils/common.h"
#include <QCoreApplication>
#include <QTimer>
#include <systemd/sd-daemon.h>

class Watchdog : public QObject
{
    Q_OBJECT
public:
    Watchdog(QObject *parent)
        : QObject(parent)
    {
        bool ok;
        const std::chrono::microseconds watchdogIntervalInUs((qgetenv("WATCHDOG_USEC").toUInt(&ok) * 3) / 4);
        if (!ok) {
            qCInfo(KWIN_CORE) << "Watchdog: disabled, not running on a systemd environment or watchdog is not set up. No WATCHDOG_USEC.";
            deleteLater();
            return;
        }
        m_onBehalf = qgetenv("WATCHDOG_PID").toUInt(&ok);
        if (!ok) {
            qCInfo(KWIN_CORE) << "Watchdog: disabled, not running on a systemd environment or watchdog is not set up. No WATCHDOG_PID.";
            deleteLater();
            return;
        }
        qunsetenv("WATCHDOG_USEC");
        qunsetenv("WATCHDOG_PID");
        auto t = new QTimer(this);
        t->setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(watchdogIntervalInUs));
        t->setSingleShot(false);
        qCInfo(KWIN_CORE) << "Watchdog: enabled. Interval:" << watchdogIntervalInUs << t->intervalAsDuration();

        sd_pid_notify(m_onBehalf, 0, "READY=1"); // If service Type=notify the service is only considered ready once we send this
        qCInfo(KWIN_CORE) << "Watchdog: Notified as ready";

        connect(t, &QTimer::timeout, this, [this] {
            sd_pid_notify(m_onBehalf, 0, "WATCHDOG=1");
        });
        t->start();
    }

private:
    pid_t m_onBehalf = 0;
};

static void setupWatchdog()
{
    new Watchdog(QCoreApplication::instance());
}

Q_COREAPP_STARTUP_FUNCTION(setupWatchdog)

#include "watchdog.moc"
