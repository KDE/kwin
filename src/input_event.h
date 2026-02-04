/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

#include <QObject>
#include <QPointF>
#include <chrono>

namespace KWin
{

class InputDevice;
class InputDeviceTabletTool;

struct PointerMotionEvent
{
    Q_GADGET
    Q_PROPERTY(QPointF position MEMBER position CONSTANT)
    Q_PROPERTY(bool warp MEMBER warp CONSTANT)
    Q_PROPERTY(Qt::MouseButtons buttons MEMBER buttons CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers MEMBER modifiers CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiersRelevantForShortcuts MEMBER modifiersRelevantForShortcuts CONSTANT)
    Q_PROPERTY(qint64 timestamp READ timestampMicros CONSTANT)

public:
    qint64 timestampMicros() const
    {
        return timestamp.count();
    }

public:
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
    Q_GADGET
    Q_PROPERTY(QPointF position MEMBER position CONSTANT)
    Q_PROPERTY(KWin::PointerButtonState state MEMBER state CONSTANT)
    Q_PROPERTY(Qt::MouseButton button MEMBER button CONSTANT)
    Q_PROPERTY(Qt::MouseButtons buttons MEMBER buttons CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers MEMBER modifiers CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiersRelevantForShortcuts MEMBER modifiersRelevantForShortcuts CONSTANT)
    Q_PROPERTY(qint64 timestamp READ timestampMicros CONSTANT)

public:
    qint64 timestampMicros() const
    {
        return timestamp.count();
    }

public:
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

struct PointerSwipeGestureBeginEvent
{
    int fingerCount;
    std::chrono::microseconds time;
};

struct PointerSwipeGestureUpdateEvent
{
    QPointF delta;
    std::chrono::microseconds time;
};

struct PointerSwipeGestureEndEvent
{
    std::chrono::microseconds time;
};

struct PointerSwipeGestureCancelEvent
{
    std::chrono::microseconds time;
};

struct PointerPinchGestureBeginEvent
{
    int fingerCount;
    std::chrono::microseconds time;
};

struct PointerPinchGestureUpdateEvent
{
    qreal scale;
    qreal angleDelta;
    QPointF delta;
    std::chrono::microseconds time;
};

struct PointerPinchGestureEndEvent
{
    std::chrono::microseconds time;
};

struct PointerPinchGestureCancelEvent
{
    std::chrono::microseconds time;
};

struct PointerHoldGestureBeginEvent
{
    int fingerCount;
    std::chrono::microseconds time;
};

struct PointerHoldGestureEndEvent
{
    std::chrono::microseconds time;
};

struct PointerHoldGestureCancelEvent
{
    std::chrono::microseconds time;
};

struct TouchDownEvent
{
    qint32 id;
    QPointF pos;
    std::chrono::microseconds time;
};

struct TouchMotionEvent
{
    qint32 id;
    QPointF pos;
    std::chrono::microseconds time;
};

struct TouchUpEvent
{
    qint32 id;
    std::chrono::microseconds time;
};

struct KeyboardKeyEvent
{
    Q_GADGET
    Q_PROPERTY(KWin::KeyboardKeyState state MEMBER state CONSTANT)
    Q_PROPERTY(Qt::Key key MEMBER key CONSTANT)
    Q_PROPERTY(QString text MEMBER text CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiers MEMBER modifiers CONSTANT)
    Q_PROPERTY(Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts MEMBER modifiersRelevantForGlobalShortcuts CONSTANT)
    Q_PROPERTY(qint64 timestamp READ timestampMicros CONSTANT)
    Q_PROPERTY(uint32_t serial MEMBER serial CONSTANT)

public:
    qint64 timestampMicros() const
    {
        return timestamp.count();
    }

public:
    InputDevice *device;
    KeyboardKeyState state;
    Qt::Key key;
    quint32 nativeScanCode;
    quint32 nativeVirtualKey;
    QString text;
    Qt::KeyboardModifiers modifiers;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts;
    std::chrono::microseconds timestamp;
    uint32_t serial;
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
    quint32 group;
    quint32 mode;
    bool isModeSwitch;
    std::chrono::microseconds time;
};

struct TabletPadStripEvent
{
    InputDevice *device;
    int number;
    qreal position;
    bool isFinger;
    quint32 group;
    quint32 mode;
    std::chrono::microseconds time;
};

struct TabletPadRingEvent
{
    InputDevice *device;
    int number;
    qreal position;
    bool isFinger;
    quint32 group;
    quint32 mode;
    std::chrono::microseconds time;
};

struct TabletPadDialEvent
{
    InputDevice *device;
    int number;
    double delta;
    quint32 group;
    std::chrono::microseconds time;
};

} // namespace KWin

Q_DECLARE_METATYPE(KWin::PointerMotionEvent)
Q_DECLARE_METATYPE(KWin::PointerMotionEvent *)
Q_DECLARE_METATYPE(KWin::PointerButtonEvent)
Q_DECLARE_METATYPE(KWin::PointerButtonEvent *)
Q_DECLARE_METATYPE(KWin::KeyboardKeyEvent)
Q_DECLARE_METATYPE(KWin::KeyboardKeyEvent *)
