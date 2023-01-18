/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inputdevice.h"

namespace KWin
{

InputDevice::InputDevice(QObject *parent)
    : QObject(parent)
{
}

QString InputDevice::outputName() const
{
    return {};
}

void InputDevice::setOutputName(const QString &outputName)
{
}

bool InputDevice::isNaturalScroll() const
{
    return false;
}

} // namespace KWin

#include "moc_inputdevice.cpp"
