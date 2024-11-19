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
