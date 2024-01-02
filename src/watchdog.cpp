/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "watchdoglogging.h"
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
        // by 1/2 to have some margin to hit the watchdog interval timely, as recommended by the docs
        const std::chrono::microseconds watchdogIntervalInUs(qEnvironmentVariableIntValue("WATCHDOG_USEC", &ok) / 2);
        if (!ok) {
            qCDebug(KWIN_WATCHDOG) << "Watchdog: disabled, not running on a systemd environment or watchdog is not set up. No WATCHDOG_USEC.";
            deleteLater();
            return;
        }
        m_onBehalf = qEnvironmentVariableIntValue("WATCHDOG_PID", &ok);
        if (!ok) {
            qCDebug(KWIN_WATCHDOG) << "Watchdog: disabled, not running on a systemd environment or watchdog is not set up. No WATCHDOG_PID.";
            deleteLater();
            return;
        }
        qunsetenv("WATCHDOG_USEC");
        qunsetenv("WATCHDOG_PID");
        auto t = new QTimer(this);
        t->setInterval(std::chrono::duration_cast<std::chrono::milliseconds>(watchdogIntervalInUs));
        t->setSingleShot(false);

        // If service Type=notify the service is only considered ready once we send this
        const int notified = sd_pid_notify(m_onBehalf, 0, "READY=1");
        if (notified < 1) {
            qCCritical(KWIN_WATCHDOG) << "Watchdog: failed to set the process as ready, systemd will probably kill the process soon. :'(" << notified << strerror(-notified);
        } else {
            qCInfo(KWIN_WATCHDOG) << "Watchdog: enabled. Interval:" << watchdogIntervalInUs << t->interval() << qgetenv("NOTIFY_SOCKET") << geteuid();
        }

        const auto bark = [this]() {
            const int ret = sd_pid_notify(m_onBehalf, 0, "WATCHDOG=1");
            if (ret < 1) {
                qCCritical(KWIN_WATCHDOG) << "Watchdog: failed to bark, systemd will probably kill the process soon. :'(" << ret << strerror(-ret);
            } else {
                qCDebug(KWIN_WATCHDOG) << "Watchdog: bark!";
            }
        };
        bark();
        connect(t, &QTimer::timeout, this, bark);
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
