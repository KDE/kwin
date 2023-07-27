/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

namespace KWin
{
class GamingInputV2Interface;
class EvdevController;

class GamePadInput : public Plugin
{
    Q_OBJECT

public:
    explicit GamePadInput();

private:
    GamingInputV2Interface *const m_interface;
    EvdevController *const m_evdev;
};

} // namespace KWin
