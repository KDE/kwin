/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleixpol@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "evdevcontroller.h"
#include "kwingamepadinput_logging.h"
#include <Solid/Block>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/GenericInterface>

#include <QDirIterator>
#include <QFile>
#include <QSocketNotifier>

#include <fcntl.h>
#include <linux/input-event-codes.h>

namespace KWin
{

EvdevController::EvdevController(QObject *parent)
    : QObject(parent)
{
}

void EvdevController::init()
{
    auto notifier = Solid::DeviceNotifier::instance();
    auto refresh = [this](const QString &udi) {
        Solid::Device device(udi);
        if (device.is<Solid::Block>()) {
            qCInfo(KWIN_GAMEPAD) << "Trying device on evdev:" << device.product() << device.as<Solid::Block>()->device();
            addDevice(device);
        }
    };
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, refresh);

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Block);
    for (const Solid::Device &device : devices) {
        addDevice(device);
    }
}

bool EvdevController::addDevice(const Solid::Device &device)
{
    auto inputDevice = device.as<Solid::Block>();
    auto generic = device.as<Solid::GenericInterface>();
    if (generic && !generic->property("ID_INPUT_JOYSTICK").toBool()) {
        return false;
    }

    qDebug() << "trying" << device.udi() << device.displayName() << inputDevice->device();
    struct libevdev *dev = nullptr;
    int fd = open(QFile::encodeName(inputDevice->device()), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        qCDebug(KWIN_GAMEPAD) << "Failed to open" << inputDevice->device() << strerror(errno);
        libevdev_free(dev);
        return false;
    }

    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        qCWarning(KWIN_GAMEPAD) << "Failed to init libevdev" << inputDevice->device() << strerror(-rc) << fd;
        close(fd);
        libevdev_free(dev);
        return false;
    }
    if (!libevdev_has_event_type(dev, EV_ABS) || !libevdev_has_event_code(dev, EV_KEY, BTN_MODE)) {
        qCDebug(KWIN_GAMEPAD) << "This device does not look like a remote controller:" << libevdev_get_name(dev);
        close(fd);
        libevdev_free(dev);
        return false;
    }

    qCInfo(KWIN_GAMEPAD) << "Added evdev device:" << device.udi();
    auto evdevDevice = new EvdevDevice(device.udi(), dev);
    auto notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, evdevDevice, &EvdevDevice::deviceRemoved);

    Q_EMIT deviceAdded(evdevDevice);

    auto fdNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(fdNotifier, &QSocketNotifier::activated, evdevDevice, &EvdevDevice::readNow);

    return true;
}

}
