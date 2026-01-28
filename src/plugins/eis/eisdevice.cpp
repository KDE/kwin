/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisdevice.h"

#include <libeis.h>

#include <QTimer>

namespace KWin
{

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

EisDevice::EisDevice(eis_device *device, QObject *parent)
    : InputDevice(parent)
    , m_device(device)
    , m_releaseTimer(std::make_unique<QTimer>())
{
    eis_device_set_user_data(device, this);
    eis_device_add(device);
    m_releaseTimer->setInterval(std::chrono::seconds(1));
    m_releaseTimer->setSingleShot(true);
    m_releaseTimer->callOnTimeout(this, &EisDevice::releasePressedAndTouches);
}

EisDevice::~EisDevice()
{
    releasePressedAndTouches();
    eis_device_remove(m_device);
    eis_device_unref(m_device);
}

void EisDevice::releasePressedAndTouches()
{
    for (const auto button : pressedButtons) {
        Q_EMIT pointerButtonChanged(button, PointerButtonState::Released, currentTime(), this);
    }
    pressedButtons.clear();
    for (const auto key : pressedKeys) {
        Q_EMIT keyChanged(key, KeyboardKeyState::Released, currentTime(), this);
    }
    pressedKeys.clear();
    if (!activeTouches.empty()) {
        Q_EMIT touchCanceled(this);
    }
    activeTouches.clear();
}

void EisDevice::changeDevice(eis_device *device)
{
    eis_device_set_user_data(m_device, nullptr);
    eis_device_remove(m_device);
    eis_device_unref(m_device);
    m_device = device;
    eis_device_set_user_data(device, this);
    eis_device_add(device);
    if (m_enabled) {
        eis_device_resume(device);
    }
}

void EisDevice::setEmulating(bool emulating)
{
    if (emulating) {
        m_releaseTimer->stop();
    } else {
        m_releaseTimer->start();
    }
}

QString EisDevice::name() const
{
    return QString::fromUtf8(eis_device_get_name(m_device));
}

bool EisDevice::isEnabled() const
{
    return m_enabled;
}

void EisDevice::setEnabled(bool enabled)
{
    m_enabled = enabled;
    enabled ? eis_device_resume(m_device) : eis_device_pause(m_device);
}

bool EisDevice::isKeyboard() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_KEYBOARD);
}

bool EisDevice::isPointer() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_POINTER) || eis_device_has_capability(m_device, EIS_DEVICE_CAP_POINTER_ABSOLUTE);
}

bool EisDevice::isTouchpad() const
{
    return false;
}

bool EisDevice::isTouch() const
{
    return eis_device_has_capability(m_device, EIS_DEVICE_CAP_TOUCH);
}

bool EisDevice::isTabletTool() const
{
    return false;
}

bool EisDevice::isTabletPad() const
{
    return false;
}

bool EisDevice::isTabletModeSwitch() const
{
    return false;
}

bool EisDevice::isLidSwitch() const
{
    return false;
}

}
