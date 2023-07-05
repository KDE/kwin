/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "lidswitchtracker.h"
#include "core/inputdevice.h"
#include "input_event.h"

namespace KWin
{

LidSwitchTracker::LidSwitchTracker()
{
    input()->installInputEventSpy(this);
}

bool LidSwitchTracker::isLidClosed() const
{
    return m_isLidClosed;
}

void LidSwitchTracker::switchEvent(KWin::SwitchEvent *event)
{
    if (event->device()->isLidSwitch()) {
        const bool state = event->state() == SwitchEvent::State::On;
        if (state != m_isLidClosed) {
            m_isLidClosed = state;
            Q_EMIT lidStateChanged();
        }
    }
}

}

#include "moc_lidswitchtracker.cpp"
