/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: MIT
 */

#include "inputfd_v1.h"
#include <QSocketNotifier>

InputFdSeatV1 *InputFdManagerV1::seat(wl_seat *seat)
{
    return new InputFdSeatV1(get_seat_evdev(seat));
}

void InputFdDeviceV1::wp_inputfd_device_evdev_v1_focus_in(uint32_t serial, int32_t fd, wl_surface *surface)
{
    qDebug() << "focus in" << m_name << fd << surface;
    struct libevdev *dev = nullptr;
    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        qWarning() << "Failed to init libevdev" << strerror(-rc) << fd;
        close(fd);
        libevdev_free(dev);
        return;
    }
    if (!libevdev_has_event_type(dev, EV_ABS) || !libevdev_has_event_code(dev, EV_KEY, BTN_MODE)) {
        qDebug() << "This device does not look like a remote controller:" << libevdev_get_name(dev);
        close(fd);
        libevdev_free(dev);
        return;
    }

    auto evdevDevice = new KWin::EvdevDevice({}, dev);
    connect(evdevDevice, &KWin::EvdevDevice::axisChange, this, [](uint32_t time, uint32_t axis, wl_fixed_t value) {
        qDebug() << "axis" << axis << wl_fixed_to_double(value);
    });
    connect(evdevDevice, &KWin::EvdevDevice::buttonChange, this, [](uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog) {
        qDebug() << "button" << button << "state:" << state << wl_fixed_to_double(analog);
    });
    connect(evdevDevice, &KWin::EvdevDevice::frame, this, [](uint32_t time) {
        qDebug() << "frame!";
    });

    m_dev.reset(evdevDevice);
    auto fdNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(fdNotifier, &QSocketNotifier::activated, evdevDevice, &KWin::EvdevDevice::readNow);
}
