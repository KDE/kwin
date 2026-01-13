/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Yelsin Sepulveda <yelsin.sepulveda@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "keyboard_input.h"
#include <libevdev/libevdev.h>

#include <QTimer>

namespace KWin
{

class EmulatedInputDevice : public InputDevice
{
public:
    explicit EmulatedInputDevice(libevdev *device);
    ~EmulatedInputDevice();

    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    bool isKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

    void emulateInputDevice(input_event &ev);

private:
    void evkeyMapping(input_event *ev);
    void evabsMapping(input_event *ev);
    void handleAnalogStickInput();
    void updateAnalogStick(input_event *ev);
    bool sendKeySequence(const QKeySequence &keys, KeyboardKeyState pressed, std::chrono::microseconds time);
    void toggleVirtualKeyboard();

    static constexpr qreal MOUSE_BASE_SPEED = 3.0f;
    static constexpr qreal MOUSE_MAX_SPEED = 20.0f;
    static constexpr qreal SPEED_CURVE_EXPONENT = 2.5f;

    libevdev *const m_device;
    const QPointF m_leftStickLimits;
    const QPointF m_rightStickLimits;
    QTimer m_timer;
    QPointF m_leftStick;
    QPointF m_rightStick;
    PointerButtonState m_leftClick = PointerButtonState::Released;
    PointerButtonState m_rightClick = PointerButtonState::Released;
};

} // namespace KWin
