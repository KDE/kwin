/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"
#include "kwin_export.h"

#include <QObject>

namespace KWin
{

enum class PointerButtonState {
    Released,
    Pressed,
};

enum class PointerAxis {
    Vertical,
    Horizontal,
};

enum class PointerAxisSource {
    Unknown,
    Wheel,
    Finger,
    Continuous,
    WheelTilt,
};

enum class KeyboardKeyState {
    Released,
    Pressed,
    Repeated,
};

enum class SwitchState {
    Off,
    On,
};

class KWIN_EXPORT InputDeviceTabletTool : public QObject
{
    Q_OBJECT

public:
    enum Type {
        Pen,
        Eraser,
        Brush,
        Pencil,
        Airbrush,
        Finger,
        Mouse,
        Lens,
        Totem,
    };

    enum Capability {
        Tilt,
        Pressure,
        Distance,
        Rotation,
        Slider,
        Wheel,
    };

    explicit InputDeviceTabletTool(QObject *parent = nullptr);

    virtual quint64 serialId() const = 0;
    virtual quint64 uniqueId() const = 0;

    virtual Type type() const = 0;
    virtual QList<Capability> capabilities() const = 0;
};

/**
 * The InputDevice class represents an input device, e.g. a mouse, or a keyboard, etc.
 */
class KWIN_EXPORT InputDevice : public QObject
{
    Q_OBJECT

public:
    explicit InputDevice(QObject *parent = nullptr);

    virtual QString sysPath() const;
    virtual QString name() const = 0;
    virtual quint32 vendor() const;
    virtual quint32 product() const;

    virtual void *group() const;

    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    virtual LEDs leds() const;
    virtual void setLeds(LEDs leds);

    virtual bool isKeyboard() const = 0;
    virtual bool isPointer() const = 0;
    virtual bool isTouchpad() const = 0;
    virtual bool isTouch() const = 0;
    virtual bool isTabletTool() const = 0;
    virtual bool isTabletPad() const = 0;
    virtual bool isTabletModeSwitch() const = 0;
    virtual bool isLidSwitch() const = 0;

    virtual QString outputName() const;
    virtual void setOutputName(const QString &outputName);

    virtual int tabletPadButtonCount() const;
    virtual int tabletPadRingCount() const;
    virtual int tabletPadStripCount() const;
    virtual int tabletPadModeCount() const;
    virtual int tabletPadMode() const;

Q_SIGNALS:
    void keyChanged(quint32 key, KeyboardKeyState, std::chrono::microseconds time, InputDevice *device);
    void pointerButtonChanged(quint32 button, PointerButtonState state, std::chrono::microseconds time, InputDevice *device);
    void pointerMotionAbsolute(const QPointF &position, std::chrono::microseconds time, InputDevice *device);
    void pointerMotion(const QPointF &delta, const QPointF &deltaNonAccelerated, std::chrono::microseconds time, InputDevice *device);
    void pointerAxisChanged(PointerAxis axis, qreal delta, qint32 deltaV120, PointerAxisSource source, bool inverted, std::chrono::microseconds time, InputDevice *device);
    void pointerFrame(InputDevice *device);
    void touchFrame(InputDevice *device);
    void touchCanceled(InputDevice *device);
    void touchDown(qint32 id, const QPointF &absolutePos, std::chrono::microseconds time, InputDevice *device);
    void touchUp(qint32 id, std::chrono::microseconds time, InputDevice *device);
    void touchMotion(qint32 id, const QPointF &absolutePos, std::chrono::microseconds time, InputDevice *device);
    void swipeGestureBegin(int fingerCount, std::chrono::microseconds time, InputDevice *device);
    void swipeGestureUpdate(const QPointF &delta, std::chrono::microseconds time, InputDevice *device);
    void swipeGestureEnd(std::chrono::microseconds time, InputDevice *device);
    void swipeGestureCancelled(std::chrono::microseconds time, InputDevice *device);
    void pinchGestureBegin(int fingerCount, std::chrono::microseconds time, InputDevice *device);
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QPointF &delta, std::chrono::microseconds time, InputDevice *device);
    void pinchGestureEnd(std::chrono::microseconds time, InputDevice *device);
    void pinchGestureCancelled(std::chrono::microseconds time, InputDevice *device);
    void holdGestureBegin(int fingerCount, std::chrono::microseconds time, InputDevice *device);
    void holdGestureEnd(std::chrono::microseconds time, InputDevice *device);
    void holdGestureCancelled(std::chrono::microseconds time, InputDevice *device);
    void switchToggle(SwitchState state, std::chrono::microseconds time, InputDevice *device);
    void tabletToolAxisEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device);
    void tabletToolProximityEvent(const QPointF &pos, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipNear, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device);
    void tabletToolTipEvent(const QPointF &pos, qreal pressure, qreal xTilt, qreal yTilt, qreal rotation, qreal distance, bool tipDown, qreal sliderPosition, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device);
    void tabletToolButtonEvent(uint button, bool isPressed, InputDeviceTabletTool *tool, std::chrono::microseconds time, InputDevice *device);
    void tabletPadButtonEvent(uint button, bool isPressed, std::chrono::microseconds time, InputDevice *device);
    void tabletPadStripEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);
    void tabletPadRingEvent(int number, int position, bool isFinger, std::chrono::microseconds time, InputDevice *device);
};

} // namespace KWin
