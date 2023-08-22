/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "qwayland-server-inputfd-v1.h"
#include <linux/input.h>
#include <utils/filedescriptor.h>
#include <wayland/display.h>
#include <wayland_server.h>

namespace KWin
{
class Window;

class InputFdDeviceEvdevV1Interface : public QObject, public QtWaylandServer::wp_inputfd_device_evdev_v1
{
    Q_OBJECT
public:
    InputFdDeviceEvdevV1Interface(KWaylandServer::Display *display, const QString &path, const QString &name, uint32_t vendor_id, uint32_t product_id, const QString &udi);
    ~InputFdDeviceEvdevV1Interface() override;

    void wp_inputfd_device_evdev_v1_bind_resource(Resource *resource) override;
    void wp_inputfd_device_evdev_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
        if (resourceMap().isEmpty()) {
            deleteLater();
        }
    }
    QString udi() const
    {
        return m_udi;
    }

private:
    friend class InputFdSeatEvdevV1Interface;
    void setFocusedSurface(KWaylandServer::SurfaceInterface *surface);
    wl_resource *surfaceResource(KWaylandServer::SurfaceInterface *surface);
    void sendSetupData(wl_resource *handle);

    const QString m_path;
    const QString m_name;
    const uint32_t m_vendor_id;
    const uint32_t m_product_id;
    KWaylandServer::SurfaceInterface *m_focused = nullptr;
    FileDescriptor m_fd;
    KWaylandServer::Display *const m_display;
    const QString m_udi;
};

class InputFdSeatEvdevV1Interface : public QObject, public QtWaylandServer::wp_inputfd_seat_evdev_v1
{
public:
    InputFdSeatEvdevV1Interface(KWaylandServer::Display *display, QObject *parent);

    void addGamepad(InputFdDeviceEvdevV1Interface *gamepad);
    void removeGamepad(InputFdDeviceEvdevV1Interface *gamepad);

    void setFocusedSurface(KWaylandServer::SurfaceInterface *surface);

private:
    void wp_inputfd_seat_evdev_v1_bind_resource(Resource *resource) override;
    void wp_inputfd_seat_evdev_v1_destroy(Resource *resource) override;

    void sendGamepadAdded(Resource *resource, InputFdDeviceEvdevV1Interface *gamepad);
    QList<InputFdDeviceEvdevV1Interface *> m_devices;
};

class InputFdManagerV1Interface : public QObject, public QtWaylandServer::wp_inputfd_manager_v1
{
public:
    InputFdManagerV1Interface(KWaylandServer::Display *display, QObject *parent);

    void wp_inputfd_manager_v1_get_seat_evdev(Resource *resource, uint32_t inputfd_seat, struct ::wl_resource *seat) override;
    void wp_inputfd_manager_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    InputFdSeatEvdevV1Interface *get(KWaylandServer::SeatInterface *gaming_seat);

private:
    QHash<KWaylandServer::SeatInterface *, InputFdSeatEvdevV1Interface *> m_seats;
    KWaylandServer::Display *const m_display;
};

}
