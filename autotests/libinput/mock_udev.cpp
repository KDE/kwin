/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "../../udev.h"
#include "mock_udev.h"

udev *udev::s_mockUdev = nullptr;

namespace KWin
{

Udev::Udev()
    : m_udev(udev::s_mockUdev)
{
}

Udev::~Udev()
{
}

}
