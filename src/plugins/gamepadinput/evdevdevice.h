/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: MIT
 */

#pragma once

#include <QObject>
#include <libevdev/libevdev.h>
#include <wayland-util.h>

namespace KWin
{

class EvdevDevice : public QObject
{
    Q_OBJECT

public:
    explicit EvdevDevice(const QString &udi, libevdev *device);
    ~EvdevDevice() override;

    QString name() const
    {
        return QString::fromUtf8(libevdev_get_name(m_device));
    }
    uint32_t bus() const
    {
        return libevdev_get_id_bustype(m_device);
    }
    uint32_t vendorId() const
    {
        return libevdev_get_id_vendor(m_device);
    }
    uint32_t productId() const
    {
        return libevdev_get_id_product(m_device);
    }
    uint32_t version() const
    {
        return libevdev_get_id_version(m_device);
    }

    libevdev *device() const
    {
        return m_device;
    }
    void readNow();
    std::map<uint, input_absinfo> axes() const;
    bool supportsVibration() const;
    void deviceRemoved(const QString &udi);

Q_SIGNALS:
    void axisChange(uint32_t time, uint32_t axis, wl_fixed_t value);
    void buttonChange(uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog);
    void frame(uint32_t time);
    void remove();

private:
    void processEvent(struct input_event &ev);

    libevdev *const m_device;
    const QString m_udi;
    std::map<uint, input_absinfo> m_axes;
};

}
