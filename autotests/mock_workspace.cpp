/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

