/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "gaminginput_v2_interface.h"
#include <input.h>
#include <wayland/seat_interface.h>
#include <wayland/surface_interface.h>
#include <window.h>
#include <workspace.h>

using namespace KWaylandServer;

namespace KWin
{
static const int s_version = 2;
static const int s_seatVersion = 1; // For some reason it's different...

GamingInputV2Interface::GamingInputV2Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , zcr_gaming_input_v2(*display, s_version)
    , m_display(display)
{
}

void GamingInputV2Interface::zcr_gaming_input_v2_get_gaming_seat(Resource *resource, uint32_t gaming_seat, wl_resource *seat_resource)
{
    SeatInterface *seat = SeatInterface::get(seat_resource);
    Q_ASSERT(seat);
    auto *tsi = get(seat);
    tsi->add(resource->client(), gaming_seat, s_version);
}

GamingInputSeatV2Interface *GamingInputV2Interface::get(SeatInterface *seat)
{
    Q_ASSERT(seat);
    GamingInputSeatV2Interface *&gamingSeat = m_seats[seat];
    if (!gamingSeat) {
        gamingSeat = new GamingInputSeatV2Interface(m_display, this);
    }
    return gamingSeat;
}

void GamingInputSeatV2Interface::zcr_gaming_seat_v2_bind_resource(Resource *resource)
{
    for (auto tablet : std::as_const(m_gamepads)) {
        sendGamepadAdded(resource, tablet);
    }
}

void GamingInputSeatV2Interface::zcr_gaming_seat_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
    if (resourceMap().isEmpty()) {
        deleteLater();
    }
}

void GamingInputSeatV2Interface::addGamepad(GamepadV2Interface *gamepad)
{
    Q_ASSERT(!m_gamepads.contains(gamepad));
    qDebug() << "adding gamepad" << gamepad << resourceMap().count();
    for (Resource *r : resourceMap()) {
        sendGamepadAdded(r, gamepad);
    }
    m_gamepads.append(gamepad);
}

void GamingInputSeatV2Interface::removeGamepad(GamepadV2Interface *gamepad)
{
    Q_ASSERT(m_gamepads.contains(gamepad));
    const int c = m_gamepads.removeAll(gamepad);
    Q_ASSERT(c == 1);

    for (auto *resource : gamepad->resourceMap()) {
        gamepad->send_removed(resource->handle);
    }
}

void GamingInputSeatV2Interface::sendGamepadAdded(Resource *resource, GamepadV2Interface *gamepad)
{
    wl_resource *gamepadResource = gamepad->add(resource->client(), resource->version())->handle;
    send_gamepad_added_with_device_info(resource->handle, gamepadResource, gamepad->m_name, gamepad->m_bus, gamepad->m_vendor_id, gamepad->m_product_id, gamepad->m_version);

    for (const auto &[code, axis] : gamepad->axes()) {
        gamepad->send_axis_added(gamepadResource, code, axis.minimum, axis.maximum, axis.flat, axis.fuzz, axis.resolution);
    }
    if (auto vibrator = gamepad->vibrator()) {
        auto resource = vibrator->resource();
        if (resource) {
            wl_resource *vibratorHandle = gamepad->add(resource->client(), resource->version())->handle;
            gamepad->send_vibrator_added(gamepadResource, vibratorHandle);
        }
    }
}

wl_resource *GamepadV2Interface::focusedClientHandle() const
{
    Window *window = workspace()->activeWindow();
    if (!window || !window->surface()) {
        return nullptr;
    }

    KWaylandServer::SurfaceInterface *surface = window->surface();
    KWaylandServer::ClientConnection *client = surface->client();
    Resource *r = resourceMap().value(*client);
    return r ? r->handle : nullptr;
}

void GamepadV2Interface::sendButton(uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog)
{
    auto focus = focusedClientHandle();
    if (focus) {
        send_button(focus, time, button, state, analog);
    }
}

void GamepadV2Interface::sendAxis(uint32_t time, uint32_t axis, wl_fixed_t value)
{
    auto focus = focusedClientHandle();
    if (focus) {
        send_axis(focus, time, axis, value);
    }
}

void GamepadV2Interface::sendFrame(uint32_t time)
{
    auto focus = focusedClientHandle();
    if (focus) {
        send_frame(focus, time);
    }
}

GamepadV2Interface::~GamepadV2Interface()
{
}

GamingInputSeatV2Interface::GamingInputSeatV2Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , zcr_gaming_seat_v2(*display, s_seatVersion)
{
}
}
