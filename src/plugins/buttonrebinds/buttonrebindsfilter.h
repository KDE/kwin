/*
    SPDX-FileCopyrightText: 2022 David Redondo <kde@david-redono.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "plugin.h"

#include "input.h"
#include "inputdevice.h"

class InputDevice : public KWin::InputDevice
{
    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    void setLeds(KWin::LEDs leds) override;
    KWin::LEDs leds() const override;

    bool isKeyboard() const override;
    bool isAlphaNumericKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;
};

class ButtonRebindsFilter : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT
public:
    explicit ButtonRebindsFilter();
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;

private:
    void loadConfig(const KConfigGroup &group);

    InputDevice m_inputDevice;
    QMap<Qt::MouseButton, QKeySequence> m_mouseMapping;
    KConfigWatcher::Ptr m_configWatcher;
};
