/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "inputfd_v1_interface.h"
#include "window.h"
#include <QFile>
#include <fcntl.h>
#include <wayland/seat_interface.h>
#include <wayland/surface_interface.h>

using namespace KWaylandServer;

namespace KWin
{
static const uint s_version = 1;

static void revoke(int fd)
{
    if (fd != -1) {
        ioctl(fd, EVIOCREVOKE, NULL);
    }
}

InputFdManagerV1Interface::InputFdManagerV1Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , wp_inputfd_manager_v1(*display, s_version)
    , m_display(display)
{
}

void InputFdManagerV1Interface::wp_inputfd_manager_v1_get_seat_evdev(Resource *resource, uint32_t gaming_seat, wl_resource *seat_resource)
{
    SeatInterface *seat = SeatInterface::get(seat_resource);
    Q_ASSERT(seat);
    auto *tsi = get(seat);
    tsi->add(resource->client(), gaming_seat, s_version);
}

InputFdSeatEvdevV1Interface::InputFdSeatEvdevV1Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , wp_inputfd_seat_evdev_v1(*display, s_version)
{
}

InputFdSeatEvdevV1Interface *InputFdManagerV1Interface::get(SeatInterface *seat)
{
    Q_ASSERT(seat);
    auto *&gamingSeat = m_seats[seat];
    if (!gamingSeat) {
        gamingSeat = new InputFdSeatEvdevV1Interface(m_display, this);
        connect(seat, &SeatInterface::focusedKeyboardSurfaceAboutToChange, this, [gamingSeat](auto surface) {
            gamingSeat->setFocusedSurface(surface);
        });
    }
    return gamingSeat;
}

void InputFdSeatEvdevV1Interface::wp_inputfd_seat_evdev_v1_bind_resource(Resource *resource)
{
    for (auto tablet : std::as_const(m_devices)) {
        sendGamepadAdded(resource, tablet);
    }
}

void InputFdSeatEvdevV1Interface::wp_inputfd_seat_evdev_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
    if (resourceMap().isEmpty()) {
        deleteLater();
    }
}

void InputFdSeatEvdevV1Interface::addGamepad(InputFdDeviceEvdevV1Interface *gamepad)
{
    Q_ASSERT(!m_devices.contains(gamepad));
    for (Resource *r : resourceMap()) {
        sendGamepadAdded(r, gamepad);
    }
    m_devices.append(gamepad);
}

void InputFdSeatEvdevV1Interface::removeGamepad(InputFdDeviceEvdevV1Interface *gamepad)
{
    Q_ASSERT(m_devices.contains(gamepad));
    const int c = m_devices.removeAll(gamepad);
    Q_ASSERT(c == 1);

    for (auto *resource : gamepad->resourceMap()) {
        gamepad->setFocusedSurface(nullptr);
        gamepad->send_removed(resource->handle);
    }
}

void InputFdSeatEvdevV1Interface::sendGamepadAdded(Resource *resource, InputFdDeviceEvdevV1Interface *gamepad)
{
    wl_resource *gamepadResource = gamepad->add(resource->client(), resource->version())->handle;
    wp_inputfd_seat_evdev_v1_send_device_added(resource->handle, gamepadResource);
    gamepad->sendSetupData(gamepadResource);
}

void InputFdSeatEvdevV1Interface::setFocusedSurface(SurfaceInterface *surface)
{
    for (auto device : std::as_const(m_devices)) {
        device->setFocusedSurface(surface);
    }
}

InputFdDeviceEvdevV1Interface::InputFdDeviceEvdevV1Interface(KWaylandServer::Display *display, const QString &path, const QString &name, uint32_t vendor_id, uint32_t product_id, const QString &udi)
    : wp_inputfd_device_evdev_v1()
    , m_path(path)
    , m_name(name)
    , m_vendor_id(vendor_id)
    , m_product_id(product_id)
    , m_display(display)
    , m_udi(udi)

{
    for (Resource *resource : resourceMap()) {
        sendSetupData(resource->handle);
    }
}

InputFdDeviceEvdevV1Interface::~InputFdDeviceEvdevV1Interface()
{
    revoke(m_fd.take());
}

wl_resource *InputFdDeviceEvdevV1Interface::surfaceResource(SurfaceInterface *surface)
{
    if (!surface) {
        return nullptr;
    }
    KWaylandServer::ClientConnection *client = surface->client();
    auto r = resourceMap().value(*client);
    return r ? r->handle : nullptr;
}

void InputFdDeviceEvdevV1Interface::setFocusedSurface(SurfaceInterface *surface)
{
    if (auto resource = surfaceResource(m_focused); resource) {
        send_focus_out(resource);
        revoke(m_fd.take());
    }

    m_focused = surface;
    if (auto resource = surfaceResource(surface); resource) {
        int oldFd = m_fd.take();
        m_fd = FileDescriptor(::open(QFile::encodeName(m_path), O_RDONLY));
        send_focus_in(resource, m_display->nextSerial(), m_fd.get(), m_focused->resource());
        revoke(oldFd);
    }
}

void InputFdDeviceEvdevV1Interface::wp_inputfd_device_evdev_v1_bind_resource(Resource *resource)
{
    sendSetupData(resource->handle);
}

void InputFdDeviceEvdevV1Interface::sendSetupData(wl_resource *handle)
{
    send_usb_id(handle, m_vendor_id, m_product_id);
    send_name(handle, m_name);
    send_done(handle);
}
}
