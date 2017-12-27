/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "input_event.h"

namespace KWin
{

MouseEvent::MouseEvent(QEvent::Type type, const QPointF &pos, Qt::MouseButton button,
                       Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers,
                       quint32 timestamp, const QSizeF &delta, const QSizeF &deltaNonAccelerated,
                       quint64 timestampMicroseconds, LibInput::Device *device)
            : QMouseEvent(type, pos, pos, button, buttons, modifiers)
            , m_delta(delta)
            , m_deltaUnccelerated(deltaNonAccelerated)
            , m_timestampMicroseconds(timestampMicroseconds)
            , m_device(device)
{
    setTimestamp(timestamp);
}

WheelEvent::WheelEvent(const QPointF &pos, qreal delta, Qt::Orientation orientation, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers, quint32 timestamp, LibInput::Device *device)
            : QWheelEvent(pos, pos, QPoint(), (orientation == Qt::Horizontal) ? QPoint(delta, 0) : QPoint(0, delta), delta, orientation, buttons, modifiers)
            , m_device(device)
{
    setTimestamp(timestamp);
}

KeyEvent::KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                   const QString &text, bool autorepeat, quint32 timestamp, LibInput::Device *device)
         : QKeyEvent(type, key, modifiers, code, keysym, 0, text, autorepeat)
         , m_device(device)
{
    setTimestamp(timestamp);
}

SwitchEvent::SwitchEvent(State state, quint32 timestamp, quint64 timestampMicroseconds, LibInput::Device* device)
    : QInputEvent(QEvent::User)
    , m_state(state)
    , m_timestampMicroseconds(timestampMicroseconds)
    , m_device(device)
{
    setTimestamp(timestamp);
}

}
