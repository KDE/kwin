/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

#include <QInputEvent>
#include <chrono>

namespace KWin
{

class InputDevice;
class InputDeviceTabletTool;

struct PointerMotionEvent
{
    InputDevice *device;
    QPointF position;
    QPointF delta;
    QPointF deltaUnaccelerated;
    bool warp;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForShortcuts;
    std::chrono::microseconds timestamp;
};

struct PointerButtonEvent
{
    InputDevice *device;
    QPointF position;
    InputDevice::PointerButtonState state;
    Qt::MouseButton button;
    quint32 nativeButton;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForShortcuts;
    std::chrono::microseconds timestamp;
};

struct PointerAxisEvent
{
    InputDevice *device;
    QPointF position;
    qreal delta;
    qint32 deltaV120;
    Qt::Orientation orientation;
    InputDevice::PointerAxisSource source;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts;
    bool inverted;
    std::chrono::microseconds timestamp;
};

class KWIN_EXPORT KeyEvent : public QKeyEvent
{
public:
    explicit KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                      const QString &text, bool autorepeat, std::chrono::microseconds timestamp, InputDevice *device);

    InputDevice *device() const
    {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const
    {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods)
    {
        m_modifiersRelevantForShortcuts = mods;
    }

    std::chrono::microseconds timestamp() const;

private:
    InputDevice *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
    const std::chrono::microseconds m_timestamp;
};

struct SwitchEvent
{
    enum class State {
        Off,
        On
    };

    InputDevice *device;
    State state;
    std::chrono::microseconds timestamp;
};

class TabletEvent : public QTabletEvent
{
public:
    TabletEvent(Type t, const QPointingDevice *dev, const QPointF &pos, const QPointF &globalPos,
                qreal pressure, float xTilt, float yTilt,
                float tangentialPressure, qreal rotation, float z,
                Qt::KeyboardModifiers keyState, Qt::MouseButton button, Qt::MouseButtons buttons, InputDeviceTabletTool *tool, InputDevice *device);

    InputDevice *device() const
    {
        return m_device;
    }

    InputDeviceTabletTool *tool() const
    {
        return m_tool;
    }

private:
    InputDeviceTabletTool *m_tool;
    InputDevice *m_device;
};

struct TabletToolButtonEvent
{
    InputDevice *device;
    uint button;
    bool pressed;
    InputDeviceTabletTool *tool;
    std::chrono::microseconds time;
};

struct TabletPadButtonEvent
{
    InputDevice *device;
    uint button;
    bool pressed;
    std::chrono::microseconds time;
};

struct TabletPadStripEvent
{
    InputDevice *device;
    int number;
    int position;
    bool isFinger;
    std::chrono::microseconds time;
};

struct TabletPadRingEvent
{
    InputDevice *device;
    int number;
    int position;
    bool isFinger;
    std::chrono::microseconds time;
};

} // namespace KWin
