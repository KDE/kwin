/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputdevice.h"

namespace KWin
{

InputDeviceTabletTool::InputDeviceTabletTool(QObject *parent)
    : QObject(parent)
{
}

InputDevice::InputDevice(QObject *parent)
    : QObject(parent)
{
}

QString InputDevice::sysPath() const
{
    return QString();
}

quint32 InputDevice::vendor() const
{
    return 0;
}

quint32 InputDevice::product() const
{
    return 0;
}

void *InputDevice::group() const
{
    return nullptr;
}

LEDs InputDevice::leds() const
{
    return LEDs();
}

void InputDevice::setLeds(LEDs leds)
{
}

QString InputDevice::outputName() const
{
    return {};
}

void InputDevice::setOutputName(const QString &outputName)
{
}

int InputDevice::tabletPadButtonCount() const
{
    return 0;
}

int InputDevice::tabletPadRingCount() const
{
    return 0;
}

int InputDevice::tabletPadStripCount() const
{
    return 0;
}

int InputDevice::tabletPadModeCount() const
{
    return 0;
}

int InputDevice::tabletPadMode() const
{
    return 0;
}

} // namespace KWin

#include "moc_inputdevice.cpp"
