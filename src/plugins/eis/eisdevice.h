/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "core/inputdevice.h"

#include <memory>

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

    void releasePressedAndTouches();

    void setEmulating(bool emulating);

    QSet<quint32> pressedButtons;
    QSet<quint32> pressedKeys;
    std::vector<int> activeTouches;

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

private:
    eis_device *m_device;
    std::unique_ptr<QTimer> m_releaseTimer;
    bool m_enabled;
};

}
