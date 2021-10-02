/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"
#include "input.h"

#include <QObject>

namespace KWin
{

/**
 * The InputDevice class represents an input device, e.g. a mouse, or a keyboard, etc.
 */
class KWIN_EXPORT InputDevice : public QObject
{
    Q_OBJECT

public:
    explicit InputDevice(QObject *parent = nullptr);

    virtual QString sysName() const = 0;
    virtual QString name() const = 0;

    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;

    virtual LEDs leds() const = 0;
    virtual void setLeds(LEDs leds) = 0;

    virtual bool isKeyboard() const = 0;
    virtual bool isAlphaNumericKeyboard() const = 0;
    virtual bool isPointer() const = 0;
    virtual bool isTouchpad() const = 0;
    virtual bool isTouch() const = 0;
    virtual bool isTabletTool() const = 0;
    virtual bool isTabletPad() const = 0;
    virtual bool isTabletModeSwitch() const = 0;
    virtual bool isLidSwitch() const = 0;

Q_SIGNALS:
    void keyChanged(quint32 key, InputRedirection::KeyboardKeyState, quint32 time, InputDevice *device);
    void pointerButtonChanged(quint32 button, InputRedirection::PointerButtonState state, quint32 time, InputDevice *device);
    void pointerMotionAbsolute(const QPointF &position, quint32 time, InputDevice *device);
    void pointerMotion(const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint32 time, quint64 timeMicroseconds, InputDevice *device);
    void pointerAxisChanged(InputRedirection::PointerAxis axis, qreal delta, qint32 discreteDelta,
                            InputRedirection::PointerAxisSource source, quint32 time, InputDevice *device);
    void touchFrame(InputDevice *device);
    void touchCanceled(InputDevice *device);
    void touchDown(qint32 id, const QPointF &absolutePos, quint32 time, InputDevice *device);
    void touchUp(qint32 id, quint32 time, InputDevice *device);
    void touchMotion(qint32 id, const QPointF &absolutePos, quint32 time, InputDevice *device);
    void swipeGestureBegin(int fingerCount, quint32 time, InputDevice *device);
    void swipeGestureUpdate(const QSizeF &delta, quint32 time, InputDevice *device);
    void swipeGestureEnd(quint32 time, InputDevice *device);
    void swipeGestureCancelled(quint32 time, InputDevice *device);
    void pinchGestureBegin(int fingerCount, quint32 time, InputDevice *device);
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, InputDevice *device);
    void pinchGestureEnd(quint32 time, InputDevice *device);
    void pinchGestureCancelled(quint32 time, InputDevice *device);
    void holdGestureBegin(int fingerCount, quint32 time, InputDevice *device);
    void holdGestureEnd(quint32 time, InputDevice *device);
    void holdGestureCancelled(quint32 time, InputDevice *device);
    void switchToggledOn(quint32 time, quint64 timeMicroseconds, InputDevice *device);
    void switchToggledOff(quint32 time, quint64 timeMicroseconds, InputDevice *device);

    void tabletToolEvent(InputRedirection::TabletEventType type, const QPointF &pos,
                         qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                         bool tipNear, const TabletToolId &tabletToolId, quint32 time);
    void tabletToolButtonEvent(uint button, bool isPressed, const TabletToolId &tabletToolId);

    void tabletPadButtonEvent(uint button, bool isPressed, const TabletPadId &tabletPadId);
    void tabletPadStripEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId);
    void tabletPadRingEvent(int number, int position, bool isFinger, const TabletPadId &tabletPadId);
};

} // namespace KWin
