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

Output *InputDevice::output() const
{
    return nullptr;
}

} // namespace KWin
