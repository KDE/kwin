/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "qwayland-gaming-input-unstable-v2.h"
#include <qguiapplication_platform.h>

class GamePad : public QObject, public QtWayland::zcr_gamepad_v2
{
    Q_OBJECT
public:
    GamePad(struct ::zcr_gamepad_v2 *object)
        : QtWayland::zcr_gamepad_v2(object)
    {
    }

    ~GamePad()
    {
        destroy();
    }

    void zcr_gamepad_v2_button(uint32_t time, uint32_t button, uint32_t state, wl_fixed_t analog) override
    {
        qDebug() << "button! button" << button << "state:" << state << "analog:" << wl_fixed_to_double(analog) << "time:" << time;
        Q_EMIT buttonEvent(button, state);
    }

    void zcr_gamepad_v2_axis(uint32_t time, uint32_t axis, wl_fixed_t value) override
    {
        qDebug() << "axis!" << axis << wl_fixed_to_double(value) << "time:" << time;
    }

    void zcr_gamepad_v2_axis_added(uint32_t index, int32_t min_value, int32_t max_value, int32_t flat, int32_t fuzz, int32_t resolution) override
    {
        qDebug() << "axis added." << min_value << " < " << max_value << "flat:" << flat << "fuzz" << fuzz << "resolution:" << resolution;
    }

    void zcr_gamepad_v2_removed() override
    {
        qDebug() << "removed";
        Q_EMIT removed();
    }

Q_SIGNALS:
    void buttonEvent(uint32_t button, uint32_t state);
    void removed();
};

class GamingInputSeat : public QObject, public QtWayland::zcr_gaming_seat_v2
{
    Q_OBJECT
public:
    GamingInputSeat(struct ::zcr_gaming_seat_v2 *object)
        : QtWayland::zcr_gaming_seat_v2(object)
    {
    }

    GamingInputSeat()
    {
        qDebug() << "seat destroyed!";
    }

    void zcr_gaming_seat_v2_gamepad_added_with_device_info(struct ::zcr_gamepad_v2 *gamepad, const QString &name, uint32_t bus, uint32_t vendor_id, uint32_t product_id, uint32_t version) override
    {
        auto g = new GamePad(gamepad);
        connect(g, &GamePad::removed, this, [this, g] {
            m_gamepads.removeAll(g);
        });
        m_gamepads << g;
        Q_EMIT gamepadAdded(g);
    }

Q_SIGNALS:
    void gamepadAdded(GamePad *gamepad);

public:
    QList<GamePad *> m_gamepads;
};

class GamingInput : public QtWayland::zcr_gaming_input_v2
{
public:
    GamingInput()
    {
    }

    GamingInputSeat *seat(wl_seat *seat)
    {
        return new GamingInputSeat(get_gaming_seat(seat));
    }
};
