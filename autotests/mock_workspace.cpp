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
#include "mock_workspace.h"
#include "mock_client.h"

namespace KWin
{

Workspace *MockWorkspace::s_self = nullptr;

MockWorkspace::MockWorkspace(QObject *parent)
    : QObject(parent)
    , m_activeClient(nullptr)
{
    s_self = this;
}

MockWorkspace::~MockWorkspace()
{
    s_self = nullptr;
}

Client *MockWorkspace::activeClient() const
{
    return m_activeClient;
}

void MockWorkspace::setActiveClient(Client *c)
{
    m_activeClient = c;
}

}

