/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
