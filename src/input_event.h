/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

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
    PointerButtonState state;
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
    PointerAxisSource source;
    Qt::MouseButtons buttons;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts;
    bool inverted;
    std::chrono::microseconds timestamp;
};

struct KeyboardKeyEvent
{
    InputDevice *device;
    KeyboardKeyState state;
    Qt::Key key;
    quint32 nativeScanCode;
    quint32 nativeVirtualKey;
    QString text;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts;
    std::chrono::microseconds timestamp;
};

struct SwitchEvent
{
    InputDevice *device;
    SwitchState state;
    std::chrono::microseconds timestamp;
};

struct TabletToolProximityEvent
{
public:
    enum Type {
        EnterProximity,
        LeaveProximity,
    };

    Type type;
    InputDevice *device;
    qreal rotation;
    QPointF position;
    qreal sliderPosition;
    qreal xTilt;
    qreal yTilt;
    qreal distance;
    std::chrono::microseconds timestamp;
    InputDeviceTabletTool *tool;
};

struct TabletToolTipEvent
{
public:
    enum Type {
        Press,
        Release,
    };

    Type type;
    InputDevice *device;
    qreal rotation;
    QPointF position;
    Qt::MouseButtons buttons;
    qreal pressure;
    qreal sliderPosition;
    qreal xTilt;
    qreal yTilt;
    qreal distance;
    std::chrono::microseconds timestamp;
    InputDeviceTabletTool *tool;
};

struct TabletToolAxisEvent
{
public:
    InputDevice *device;
    qreal rotation;
    QPointF position;
    Qt::MouseButtons buttons;
    qreal pressure;
    qreal sliderPosition;
    qreal xTilt;
    qreal yTilt;
    qreal distance;
    std::chrono::microseconds timestamp;
    InputDeviceTabletTool *tool;
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
