/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "brightnessdevice.h"
#include "output.h"

namespace KWin
{

BrightnessDevice::BrightnessDevice()
{
}

BrightnessDevice::~BrightnessDevice()
{
}

void BrightnessDevice::setOutput(Output *output)
{
    m_output = output;
}

Output *BrightnessDevice::output() const
{
    return m_output;
}

}
