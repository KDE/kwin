/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "gamepadinput.h"
#include "evdevcontroller.h"
#include "gaminginput_v2_interface.h"

namespace KWin
{

GamePadInput::GamePadInput()
    : m_interface(new GamingInputV2Interface(waylandServer()->display(), this))
    , m_evdev(new EvdevController(this))
{
    connect(m_evdev, &EvdevController::deviceAdded, this, [this](EvdevDevice *device) {
        auto gamepad = new GamepadV2Interface(device->axes(), device->supportsVibration(), device->name(), device->bus(), device->vendorId(), device->productId(), device->version());
        connect(device, &EvdevDevice::buttonChange, gamepad, &GamepadV2Interface::sendButton);
        connect(device, &EvdevDevice::axisChange, gamepad, &GamepadV2Interface::sendAxis);
        connect(device, &EvdevDevice::frame, gamepad, &GamepadV2Interface::sendFrame);

        auto seat = m_interface->get(waylandServer()->seat());
        connect(device, &EvdevDevice::remove, gamepad, [seat, gamepad] {
            if (!gamepad->property("done").toBool()) {
                gamepad->setProperty("done", 1);
                seat->removeGamepad(gamepad);
            }
        });
        seat->addGamepad(gamepad);
    });
    m_evdev->init();
}

} // namespace KWin

#include "moc_gamepadinput.cpp"
