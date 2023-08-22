/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleix.pol_gonzalez@mercedes-benz.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

namespace KWin
{
class EvdevController;
class GamingInputV2Interface;
class InputFdManagerV1Interface;

class GamePadInput : public Plugin
{
    Q_OBJECT

public:
    explicit GamePadInput();

private:
    GamingInputV2Interface *const m_interface;
    InputFdManagerV1Interface *const m_interfaceInputFd;
    EvdevController *const m_evdev;
};

} // namespace KWin
