/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: MIT
 */

#include "evdevdevice.h"
#include <QDebug>

namespace KWin
{

EvdevDevice::EvdevDevice(const QString &path, libevdev *device)
    : m_device(device)
    , m_udi(path)
{
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
