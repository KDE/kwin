/*
 *   SPDX-FileCopyrightText: 2022 Aleix Pol i Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>
 *
 *   SPDX-License-Identifier: MIT
 */

#include "inputfd_v1.h"

InputFdSeatV1 *InputFdManagerV1::seat(wl_seat *seat)
{
    return new InputFdSeatV1(get_seat_evdev(seat));
}
