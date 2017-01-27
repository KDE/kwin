/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

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
#include "keyboard_repeat.h"
#include "keyboard_input.h"
#include "input_event.h"
#include "wayland_server.h"

#include <KWayland/Server/seat_interface.h>

#include <QTimer>

namespace KWin
{

KeyboardRepeat::KeyboardRepeat(Xkb *xkb)
    : QObject()
    , m_timer(new QTimer)
    , m_xkb(xkb)
{
    connect(m_timer, &QTimer::timeout, this, &KeyboardRepeat::handleKeyRepeat);
}

KeyboardRepeat::~KeyboardRepeat() = default;

void KeyboardRepeat::handleKeyRepeat()
{
    // TODO: don't depend on WaylandServer
    if (waylandServer()->seat()->keyRepeatRate() != 0) {
        m_timer->setInterval(1000 / waylandServer()->seat()->keyRepeatRate());
    }
    // TODO: better time
    emit keyRepeat(m_key, m_time);
}

void KeyboardRepeat::keyEvent(KeyEvent *event)
{
    if (event->isAutoRepeat()) {
        return;
    }
    const quint32 key = event->nativeScanCode();
    if (event->type() == QEvent::KeyPress) {
        // TODO: don't get these values from WaylandServer
        if (m_xkb->shouldKeyRepeat(key) && waylandServer()->seat()->keyRepeatDelay() != 0) {
            m_timer->setInterval(waylandServer()->seat()->keyRepeatDelay());
            m_key = key;
            m_time = event->timestamp();
            m_timer->start();
        }
    } else if (event->type() == QEvent::KeyRelease) {
        if (key == m_key) {
            m_timer->stop();
        }
    }
}

}
