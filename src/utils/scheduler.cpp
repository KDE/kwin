/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scheduler.h"
#include "utils.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>

#include <mutex>
#include <optional>

#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#if defined(Q_OS_LINUX)
#include <sys/syscall.h>
#elif defined(Q_OS_FREEBSD)
#include <sys/thr.h>
#endif

namespace KWin
{

static std::optional<rlim_t> rttimeUsecMax;
static std::once_flag onceFlag;

static std::optional<rlim_t> ensureRttimeUsecMax()
{
    std::call_once(onceFlag, []() {
        QDBusMessage request = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.RealtimeKit1"),
                                                              QStringLiteral("/org/freedesktop/RealtimeKit1"),
                                                              QStringLiteral("org.freedesktop.DBus.Properties"),
                                                              QStringLiteral("Get"));
        request.setArguments({QStringLiteral("org.freedesktop.RealtimeKit1"), QStringLiteral("RTTimeUSecMax")});

        QDBusMessage reply = QDBusConnection::systemBus().call(request);
        if (reply.type() == QDBusMessage::ReplyMessage) {
            rttimeUsecMax = reply.arguments().constFirst().value<QDBusVariant>().variant().toUInt();
        }
    });
    return rttimeUsecMax;
}

static quint64 currentThreadId()
{
#if defined(Q_OS_LINUX)
    return syscall(SYS_gettid);
#elif defined(Q_OS_FREEBSD)
    long tid;
    thr_self(&tid);
    return tid;
#else
#error "currentThreadId() is not implemented for this platform"
#endif
}

void Scheduler::gainRealTime()
{
    rlimit limits;
    if (getrlimit(RLIMIT_RTTIME, &limits) == -1) {
        return;
    }

    sched_param oldParams;
    if (sched_getparam(0, &oldParams) == -1) {
        return;
    }

    // Only clients with RLIMIT_RTTIME will get RT scheduling.
    const std::optional<rlim_t> rttime = ensureRttimeUsecMax();
    if (!rttime) {
        return;
    }
    limits.rlim_cur = std::min(limits.rlim_cur, *rttime);
    limits.rlim_max = std::min(limits.rlim_max, *rttime);
    if (setrlimit(RLIMIT_RTTIME, &limits) == -1) {
        return;
    }

    if (sched_setscheduler(0, SCHED_OTHER | SCHED_RESET_ON_FORK, &oldParams) == -1) {
        return;
    }

    const quint64 threadId = currentThreadId();
    const quint32 priority = sched_get_priority_min(SCHED_RR);

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.RealtimeKit1"),
                                                          QStringLiteral("/org/freedesktop/RealtimeKit1"),
                                                          QStringLiteral("org.freedesktop.RealtimeKit1"),
                                                          QStringLiteral("MakeThreadRealtime"));
    message.setArguments({threadId, priority});

    const QDBusMessage reply = QDBusConnection::systemBus().call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCDebug(KWIN_CORE) << "Failed to acquire realtime scheduling on thread" << threadId;
    }
}

} // namespace KWin
