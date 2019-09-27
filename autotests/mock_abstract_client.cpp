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
#include "mock_abstract_client.h"

namespace KWin
{

AbstractClient::AbstractClient(QObject *parent)
    : QObject(parent)
    , m_active(false)
    , m_screen(0)
    , m_fullscreen(false)
    , m_hiddenInternal(false)
    , m_keepBelow(false)
    , m_frameGeometry()
    , m_resize(false)
{
}

AbstractClient::~AbstractClient() = default;

bool AbstractClient::isActive() const
{
    return m_active;
}

void AbstractClient::setActive(bool active)
{
    m_active = active;
}

void AbstractClient::setScreen(int screen)
{
    m_screen = screen;
}

bool AbstractClient::isOnScreen(int screen) const
{
    // TODO: mock checking client geometry
    return screen == m_screen;
}

int AbstractClient::screen() const
{
    return m_screen;
}

void AbstractClient::setFullScreen(bool set)
{
    m_fullscreen = set;
}

bool AbstractClient::isFullScreen() const
{
    return m_fullscreen;
}

bool AbstractClient::isHiddenInternal() const
{
    return m_hiddenInternal;
}

void AbstractClient::setHiddenInternal(bool set)
{
    m_hiddenInternal = set;
}

void AbstractClient::setFrameGeometry(const QRect &rect)
{
    m_frameGeometry = rect;
    emit geometryChanged();
}

QRect AbstractClient::frameGeometry() const
{
    return m_frameGeometry;
}

bool AbstractClient::keepBelow() const
{
    return m_keepBelow;
}

void AbstractClient::setKeepBelow(bool keepBelow)
{
    m_keepBelow = keepBelow;
    emit keepBelowChanged();
}

bool AbstractClient::isResize() const
{
    return m_resize;
}

void AbstractClient::setResize(bool set)
{
    m_resize = set;
}

}
