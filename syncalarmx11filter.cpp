/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "syncalarmx11filter.h"
#include "workspace.h"
#include "x11client.h"
#include "xcbutils.h"

namespace KWin
{

SyncAlarmX11Filter::SyncAlarmX11Filter()
    : X11EventFilter(QVector<int>{Xcb::Extensions::self()->syncAlarmNotifyEvent()})
{
}

bool SyncAlarmX11Filter::event(xcb_generic_event_t *event)
{
    auto alarmEvent = reinterpret_cast<xcb_sync_alarm_notify_event_t *>(event);
    auto client = workspace()->findClient([alarmEvent](const X11Client *client) {
        const auto syncRequest = client->syncRequest();
        return alarmEvent->alarm == syncRequest.alarm && alarmEvent->counter_value.hi == syncRequest.value.hi && alarmEvent->counter_value.lo == syncRequest.value.lo;
    });
    if (client) {
        client->handleSync();
    }
    return false;
}

} // namespace KWin
