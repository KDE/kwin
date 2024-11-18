/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "input.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>

namespace KWin
{
class Cursor;
class InputDeviceTabletTool;
class TabletToolV2Interface;
class TabletV2Interface;
class Window;

namespace Decoration
{
class DecoratedWindowImpl;
}

namespace LibInput
{
class Device;
}

class TabletInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit TabletInputRedirection(InputRedirection *parent);
    ~TabletInputRedirection() override;

    void tabletPad();
    bool focusUpdatesBlocked() override;

    void tabletToolEvent(KWin::InputDevice::TabletEventType type, const QPointF &pos,
                         qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                         bool tipNear, InputDeviceTabletTool *tool,
                         std::chrono::microseconds time,
                         InputDevice *device);
    void tabletToolButtonEvent(uint button, bool isPressed, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device);
    void tabletPadButtonEvent(uint button, bool isPressed, std::chrono::microseconds time, InputDevice *device);
    void tabletPadStripEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);
    void tabletPadRingEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);

    bool positionValid() const override
    {
        return !m_lastPosition.isNull();
    }
    void init() override;

    QPointF position() const override
    {
        return m_lastPosition;
    }

private:
    void cleanupDecoration(Decoration::DecoratedWindowImpl *old,
                           Decoration::DecoratedWindowImpl *now) override;
    void focusUpdate(Window *focusOld, Window *focusNow) override;
    void integrateDevice(InputDevice *device);
    void removeDevice(InputDevice *device);
    void trackNextOutput();
    void ensureTabletTool(InputDeviceTabletTool *tool);

    bool m_tipDown = false;
    bool m_tipNear = false;

    QPointF m_lastPosition;
    QMetaObject::Connection m_decorationGeometryConnection;
    QMetaObject::Connection m_decorationDestroyedConnection;
    QHash<InputDeviceTabletTool *, Cursor *> m_cursorByTool;
};

}
