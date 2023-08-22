/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "gamepadinput.h"
#include "evdevcontroller.h"
#include "gaminginput_v2_interface.h"
#include "inputfd_v1_interface.h"
#include "workspace.h"
#include <Solid/Block>
#include <Solid/DeviceNotifier>
#include <Solid/GenericInterface>
#include <kwingamepadinput_logging.h>

namespace KWin
{

GamePadInput::GamePadInput()
    : m_interface(new GamingInputV2Interface(waylandServer()->display(), this))
    , m_interfaceInputFd(new InputFdManagerV1Interface(waylandServer()->display(), this))
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

    /////////////////

    const auto addDevice = [this](const Solid::Device &device) {
        if (!device.is<Solid::Block>()) {
            return;
        }
        qCInfo(KWIN_GAMEPAD) << "Trying device on evdev:" << device.product() << device.as<Solid::Block>()->device();
        auto inputDevice = device.as<Solid::Block>();
        auto generic = device.as<Solid::GenericInterface>();
        if (generic && !generic->property("ID_INPUT_JOYSTICK").toBool()) {
            return;
        }
        InputFdDeviceEvdevV1Interface *gamepad = new InputFdDeviceEvdevV1Interface(waylandServer()->display(), inputDevice->device(), generic->property("ID_MODEL").toString(), generic->property("ID_VENDOR_ID").toInt(), generic->property("ID_MODEL_ID").toInt(), device.udi());
        auto seat = m_interfaceInputFd->get(waylandServer()->seat());
        connect(Solid::DeviceNotifier::instance(), &Solid::DeviceNotifier::deviceRemoved, seat, [seat, gamepad](const QString &uid) {
            if (uid == gamepad->udi() && !gamepad->property("done").toBool()) {
                gamepad->setProperty("done", 1);
                seat->removeGamepad(gamepad);
            }
        });
        seat->addGamepad(gamepad);
    };
    auto notifier = Solid::DeviceNotifier::instance();
    auto addDeviceUdi = [addDevice](const QString &udi) {
        addDevice(Solid::Device(udi));
    };
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, addDeviceUdi);
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Block);
    for (const Solid::Device &device : devices) {
        addDevice(device);
    }
}

} // namespace KWin

#include "moc_gamepadinput.cpp"
