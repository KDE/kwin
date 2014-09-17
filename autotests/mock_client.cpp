/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "mock_client.h"

namespace KWin
{

Client::Client(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_screen(0)
{
}

Client::~Client() = default;

bool Client::isActive() const
{
    return m_active;
}

void Client::setActive(bool active)
{
    m_active = active;
}

void Client::setScreen(int screen)
{
    m_screen = screen;
}

bool Client::isOnScreen(int screen) const
{
    // TODO: mock checking client geometry
    return screen == m_screen;
}

int Client::screen() const
{
    return m_screen;
}

}
