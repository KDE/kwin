/*
    SPDX-FileCopyrightText: 2024 David Redondo <kde@david-redondo>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "eisdevice.h"
#include "keyboard_input.h"

#include <libeis.h>

namespace KWin
{

static std::chrono::microseconds currentTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

EisDevice::EisDevice(eis_device *device, QObject *parent)
    : InputDevice(parent)
    , m_device(device)
{
    eis_device_set_user_data(device, this);
    eis_device_add(device);
}

EisDevice::~EisDevice()
{
    for (const auto button : pressedButtons) {
        Q_EMIT pointerButtonChanged(button, PointerButtonState::Released, currentTime(), this);
    }
    for (const auto key : pressedKeys) {
        Q_EMIT keyChanged(key, KeyboardKeyState::Released, currentTime(), this);
    }
    if (!activeTouches.empty()) {
        Q_EMIT touchCanceled(this);
    }
    eis_device_remove(m_device);
    eis_device_unref(m_device);
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

void EisDevice::sendKeySym(xkb_keysym_t keySym, KeyboardKeyState state)
{
    std::optional<Xkb::KeyCode> keyCode = input()->keyboard()->xkb()->keycodeFromKeysym(keySym);
    bool isPress = state == KeyboardKeyState::Pressed;
    if (keyCode) {
        // TODO modifiers
        // if set, translate it into a shift key as we go through the xkb state
        // or we can force the other path
        if (isPress) {
            pressedKeys.insert(keyCode->keyCode);
            Q_EMIT keyChanged(keyCode->keyCode, KeyboardKeyState::Pressed, currentTime(), this);
        } else {
            bool keyWasHeld = pressedKeys.remove(keyCode->keyCode);
            if (keyWasHeld) {
                Q_EMIT keyChanged(keyCode->keyCode, KeyboardKeyState::Released, currentTime(), this);
            }
        }
        return;
    }
    static const uint unmappedKeyCode = 247; // DAVE
    bool keymapUpdated = input()->keyboard()->xkb()->updateToKeymapForKeySym(unmappedKeyCode, keySym);
    if (!keymapUpdated) {
        return;
    }

    if (isPress) {
        pressedKeys.insert(unmappedKeyCode);
        Q_EMIT keyChanged(unmappedKeyCode, KeyboardKeyState::Pressed, currentTime(), this);
    } else {
        bool keyWasHeld = pressedKeys.remove(unmappedKeyCode);
        if (keyWasHeld) {
            Q_EMIT keyChanged(unmappedKeyCode, KeyboardKeyState::Released, currentTime(), this);
        }
    }

    // reset keymap back, potentially this should be done a bit more lazily next time we get another event
    for (auto key : std::as_const(pressedKeys)) {
        Q_EMIT keyChanged(key, KeyboardKeyState::Released, currentTime(), this);
        pressedKeys.remove(key);
    }
    input()->keyboard()->xkb()->reconfigure();
}
}
