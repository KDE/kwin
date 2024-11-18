/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input_event.h"
#include "core/inputdevice.h"

namespace KWin
{

KeyEvent::KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                   const QString &text, bool autorepeat, std::chrono::microseconds timestamp, InputDevice *device)
    : QKeyEvent(type, key, modifiers, code, keysym, 0, text, autorepeat)
    , m_device(device)
    , m_timestamp(timestamp)
{
    setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(timestamp).count());
}

std::chrono::microseconds KeyEvent::timestamp() const
{
    return m_timestamp;
}

TabletEvent::TabletEvent(Type t, const QPointingDevice *dev, const QPointF &pos, const QPointF &globalPos,
                         qreal pressure, float xTilt, float yTilt,
                         float tangentialPressure, qreal rotation, float z,
                         Qt::KeyboardModifiers keyState, Qt::MouseButton button, Qt::MouseButtons buttons, InputDeviceTabletTool *tool, InputDevice *device)
    : QTabletEvent(t, dev, pos, globalPos, pressure, xTilt, yTilt, tangentialPressure, rotation, z, keyState, button, buttons)
    , m_tool(tool)
    , m_device(device)
{
}

}
