/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputdevice.h"

struct eis_device;

namespace KWin
{

class EisDevice : public InputDevice
{
    Q_OBJECT
public:
    explicit EisDevice(eis_device *device, QObject *parent = nullptr);
    ~EisDevice() override;

    eis_device *handle() const
    {
        return m_device;
    }
    void changeDevice(eis_device *device);

    std::vector<quint32> pressedButtons;
    std::vector<quint32> pressedKeys;
    std::vector<int> activeTouches;

    QString sysName() const override;
    QString name() const override;

    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    LEDs leds() const override;
    void setLeds(LEDs leds) override;

    bool isKeyboard() const override;
    bool isPointer() const override;
    bool isTouchpad() const override;
    bool isTouch() const override;
    bool isTabletTool() const override;
    bool isTabletPad() const override;
    bool isTabletModeSwitch() const override;
    bool isLidSwitch() const override;

private:
    eis_device *m_device;
    bool m_enabled;
};

}
