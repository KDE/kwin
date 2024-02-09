/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fakeinputdevice.h"

namespace KWin
{
static int s_lastDeviceId = 0;

FakeInputDevice::FakeInputDevice(QObject *parent)
    : InputDevice(parent)
    , m_name(QStringLiteral("Fake Input Device %1").arg(++s_lastDeviceId))
{
}

bool FakeInputDevice::isAuthenticated() const
{
    return m_authenticated;
}

void FakeInputDevice::setAuthenticated(bool authenticated)
{
    m_authenticated = authenticated;
}

QString FakeInputDevice::sysName() const
{
    return QString();
}

QString FakeInputDevice::name() const
{
    return m_name;
}

bool FakeInputDevice::isEnabled() const
{
    return true;
}

void FakeInputDevice::setEnabled(bool enabled)
{
}

LEDs FakeInputDevice::leds() const
{
    return LEDs();
}

void FakeInputDevice::setLeds(LEDs leds)
{
}

bool FakeInputDevice::isKeyboard() const
{
    return true;
}

bool FakeInputDevice::isPointer() const
{
    return true;
}

bool FakeInputDevice::isTouchpad() const
{
    return false;
}

bool FakeInputDevice::isTouch() const
{
    return true;
}

bool FakeInputDevice::isTabletTool() const
{
    return false;
}

bool FakeInputDevice::isTabletPad() const
{
    return false;
}

bool FakeInputDevice::isTabletModeSwitch() const
{
    return false;
}

bool FakeInputDevice::isLidSwitch() const
{
    return false;
}

} // namespace KWin

#include "moc_fakeinputdevice.cpp"
