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
#include "mock_abstract_client.h"

namespace KWin
{

Workspace *MockWorkspace::s_self = nullptr;

MockWorkspace::MockWorkspace(QObject *parent)
    : QObject(parent)
    , m_activeClient(nullptr)
    , m_moveResizeClient(nullptr)
    , m_showingDesktop(false)
{
    s_self = this;
}

MockWorkspace::~MockWorkspace()
{
    s_self = nullptr;
}

AbstractClient *MockWorkspace::activeClient() const
{
    return m_activeClient;
}

void MockWorkspace::setActiveClient(AbstractClient *c)
{
    m_activeClient = c;
}

AbstractClient *MockWorkspace::moveResizeClient() const
{
    return m_moveResizeClient;
}

void MockWorkspace::setMoveResizeClient(AbstractClient *c)
{
    m_moveResizeClient = c;
}

void MockWorkspace::setShowingDesktop(bool showing)
{
    m_showingDesktop = showing;
}

bool MockWorkspace::showingDesktop() const
{
    return m_showingDesktop;
}

QRect MockWorkspace::clientArea(clientAreaOption, int screen, int desktop) const
{
    Q_UNUSED(screen)
    Q_UNUSED(desktop)
    return QRect();
}

void MockWorkspace::registerEventFilter(X11EventFilter *filter)
{
    Q_UNUSED(filter)
}

void MockWorkspace::unregisterEventFilter(X11EventFilter *filter)
{
    Q_UNUSED(filter)
}

}

