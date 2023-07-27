/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "qwayland-server-gaming-input-unstable-v2.h"
#include <linux/input.h>
#include <wayland/display.h>
#include <wayland_server.h>

namespace KWin
{

class GamepadVibratorV2Interface : public QObject, public QtWaylandServer::zcr_gamepad_vibrator_v2
{
public:
    void zcr_gamepad_vibrator_v2_vibrate(Resource *resource, wl_array *timings, wl_array *amplitudes, int32_t repeat) override
    {
    }
    void zcr_gamepad_vibrator_v2_cancel_vibration(Resource *resource) override
    {
    }

    void zcr_gamepad_vibrator_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }
};

class GamepadV2Interface : public QObject, public QtWaylandServer::zcr_gamepad_v2
{
    Q_OBJECT
public:
    GamepadV2Interface(const std::map<uint, input_absinfo> &axes, bool supportsVibration, const QString &name, uint32_t bus, uint32_t vendor_id, uint32_t product_id, uint32_t version)
        : zcr_gamepad_v2()
        , m_axes(axes)
        , m_name(name)
        , m_bus(bus)
        , m_vendor_id(vendor_id)
        , m_product_id(product_id)
        , m_version(version)
    {
        if (supportsVibration) {
            m_vibrator = std::make_unique<GamepadVibratorV2Interface>();
        }
    }
    ~GamepadV2Interface() override;

    void sendButton(uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog);
    void sendAxis(uint32_t time, uint32_t axis, wl_fixed_t value);
    void sendFrame(uint32_t time);

    void zcr_gamepad_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
        if (resourceMap().isEmpty()) {
            deleteLater();
        }
    }

    std::map<uint, input_absinfo> axes() const
    {
        return m_axes;
    }
    GamepadVibratorV2Interface *vibrator() const
    {
        return m_vibrator.get();
    }

private:
    wl_resource *focusedClientHandle() const;

    friend class GamingInputSeatV2Interface;

    std::map<uint, input_absinfo> m_axes;
    std::unique_ptr<GamepadVibratorV2Interface> m_vibrator;
    const QString m_name;
    const uint32_t m_bus;
    const uint32_t m_vendor_id;
    const uint32_t m_product_id;
    const uint32_t m_version;
};

class GamingInputSeatV2Interface : public QObject, public QtWaylandServer::zcr_gaming_seat_v2
{
public:
    GamingInputSeatV2Interface(KWaylandServer::Display *display, QObject *parent);

    void addGamepad(GamepadV2Interface *gamepad);
    void removeGamepad(GamepadV2Interface *gamepad);

private:
    void zcr_gaming_seat_v2_bind_resource(Resource *resource) override;
    void zcr_gaming_seat_v2_destroy(Resource *resource) override;

    void sendGamepadAdded(Resource *resource, GamepadV2Interface *gamepad);
    QList<GamepadV2Interface *> m_gamepads;
};

class GamingInputV2Interface : public QObject, public QtWaylandServer::zcr_gaming_input_v2
{
public:
    GamingInputV2Interface(KWaylandServer::Display *display, QObject *parent);

    void zcr_gaming_input_v2_get_gaming_seat(Resource *resource, uint32_t gaming_seat, struct ::wl_resource *seat) override;
    void zcr_gaming_input_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    GamingInputSeatV2Interface *get(KWaylandServer::SeatInterface *gaming_seat);

private:
    QHash<KWaylandServer::SeatInterface *, GamingInputSeatV2Interface *> m_seats;
    KWaylandServer::Display *const m_display;
};
}
