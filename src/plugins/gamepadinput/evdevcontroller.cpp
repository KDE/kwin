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
    auto evdevDevice = new EvdevDevice(device.udi(), dev, this);
    Q_EMIT deviceAdded(evdevDevice);

    auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, evdevDevice, &EvdevDevice::readNow);

    return true;
}

EvdevDevice::EvdevDevice(const QString &path, libevdev *device, EvdevController *controller)
    : m_controller(controller)
    , m_device(device)
    , m_udi(path)
{
    auto notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &EvdevDevice::deviceRemoved);

    for (uint i = ABS_X; i <= ABS_MISC; ++i) {
        auto info = libevdev_get_abs_info(m_device, i);
        if (info) {
            m_axes[i] = *info;
        }
    }
}

EvdevDevice::~EvdevDevice()
{
    libevdev_free(m_device);
}

void EvdevDevice::deviceRemoved(const QString &udi)
{
    if (m_udi == udi) {
        Q_EMIT remove();
        deleteLater();
    }
}

void EvdevDevice::readNow()
{
    const int fd = libevdev_get_fd(m_device);
    struct input_event ev;
    int ret = read(fd, &ev, sizeof(ev));
    if (ret == 0) {
        qDebug() << "nothing to read";
    } else if (ret < 0) {
        qWarning() << "Error while reading" << strerror(errno);
        if (errno == 19) {
            Q_EMIT remove();
            deleteLater();
        }
    } else {
        processEvent(ev);
    }

    uint bytes;
    ret = ::ioctl(fd, FIONREAD, &bytes);
    if (ret == 0 && bytes >= sizeof(ev))
        readNow();
}

std::map<uint, input_absinfo> EvdevDevice::axes() const
{
    return m_axes;
}

bool EvdevDevice::supportsVibration() const
{
    return libevdev_has_event_type(m_device, EV_FF) && libevdev_has_event_code(m_device, EV_FF, FF_RUMBLE);
}

uint64_t timevalToGamepadTime(timeval *tv)
{
    uint64_t ret = tv->tv_sec;
    ret *= 1000;
    ret += tv->tv_usec / 1000;
    return ret;
}

void EvdevDevice::processEvent(struct input_event &ev)
{
    if (ev.type == EV_KEY) {
        Q_EMIT buttonChange(timevalToGamepadTime(&ev.time), ev.code, ev.value, wl_fixed_from_double(ev.value));
    } else if (ev.type == EV_ABS) {
        Q_EMIT axisChange(timevalToGamepadTime(&ev.time), ev.code, wl_fixed_from_double(ev.value));
    } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
        Q_EMIT frame(timevalToGamepadTime(&ev.time));
    }
}

}
