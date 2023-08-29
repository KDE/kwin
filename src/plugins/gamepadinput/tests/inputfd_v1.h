/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: MIT
 */

#pragma once

#include "../evdevdevice.h"
#include "qwayland-inputfd-v1.h"
#include <QObject>
#include <qguiapplication_platform.h>

class InputFdDeviceV1 : public QObject, public QtWayland::wp_inputfd_device_evdev_v1
{
public:
    InputFdDeviceV1(struct ::wp_inputfd_device_evdev_v1 *id)
        : QtWayland::wp_inputfd_device_evdev_v1(id)
    {
    }

    ~InputFdDeviceV1()
    {
        destroy();
    }
    void wp_inputfd_device_evdev_v1_name(const QString &name) override
    {
        m_name = name;
    }
    void wp_inputfd_device_evdev_v1_usb_id(uint32_t vid, uint32_t pid) override
    {
        m_vid = vid;
        m_pid = pid;
    }
    void wp_inputfd_device_evdev_v1_done() override
    {
        qDebug() << "device done" << m_name << m_vid << m_pid;
    }
    void wp_inputfd_device_evdev_v1_removed() override
    {
        qDebug() << "removed" << m_name;
        deleteLater();
    }
    void wp_inputfd_device_evdev_v1_focus_in(uint32_t serial, int32_t fd, struct ::wl_surface *surface) override;
    void wp_inputfd_device_evdev_v1_focus_out() override
    {
        qDebug() << "focus out" << m_name;
        m_dev.reset();
    }

private:
    QString m_name;
    uint32_t m_vid = 0, m_pid = 0;
    std::unique_ptr<KWin::EvdevDevice> m_dev;
};

class InputFdSeatV1 : public QtWayland::wp_inputfd_seat_evdev_v1
{
public:
    InputFdSeatV1(struct ::wp_inputfd_seat_evdev_v1 *seat)
        : QtWayland::wp_inputfd_seat_evdev_v1(seat)
    {
    }

    void wp_inputfd_seat_evdev_v1_device_added(struct ::wp_inputfd_device_evdev_v1 *id) override
    {
        qDebug() << "device added";
        new InputFdDeviceV1(id);
    }
};

class InputFdManagerV1 : public QtWayland::wp_inputfd_manager_v1
{
public:
    InputFdSeatV1 *seat(wl_seat *seat);
};
