/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "screens.h"
#include <client.h>
#include "cursor.h"
#include "settings.h"
#include <workspace.h>
#include <config-kwin.h>
#include "screens_xrandr.h"
#if HAVE_WAYLAND
#include "screens_wayland.h"
#endif
#ifdef KWIN_UNIT_TEST
#include <mock_screens.h>
#endif

namespace KWin
{

Screens *Screens::s_self = nullptr;
Screens *Screens::create(QObject *parent)
{
    Q_ASSERT(!s_self);
#ifdef KWIN_UNIT_TEST
    s_self = new MockScreens(parent);
#else
#if HAVE_WAYLAND
    if (kwinApp()->shouldUseWaylandForCompositing()) {
        s_self = new WaylandScreens(parent);
    }
#endif
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        s_self = new XRandRScreens(parent);
    }
#endif
    s_self->init();
    return s_self;
}

Screens::Screens(QObject *parent)
    : QObject(parent)
    , m_count(0)
    , m_current(0)
    , m_currentFollowsMouse(false)
    , m_changedTimer(new QTimer(this))
{
}

Screens::~Screens()
{
    s_self = NULL;
}

void Screens::init()
{
    m_changedTimer->setSingleShot(true);
    m_changedTimer->setInterval(100);
    connect(m_changedTimer, SIGNAL(timeout()), SLOT(updateCount()));
    connect(m_changedTimer, SIGNAL(timeout()), SIGNAL(changed()));
    connect(this, &Screens::countChanged, this, &Screens::changed, Qt::QueuedConnection);
    connect(this, &Screens::changed, this, &Screens::updateSize);
    connect(this, &Screens::sizeChanged, this, &Screens::geometryChanged);

    Settings settings;
    settings.setDefaults();
    m_currentFollowsMouse = settings.activeMouseScreen();
}

void Screens::reconfigure()
{
    if (!m_config) {
        return;
    }
    Settings settings(m_config);
    settings.read();
    setCurrentFollowsMouse(settings.activeMouseScreen());
}

void Screens::updateSize()
{
    QRect bounding;
    for (int i = 0; i < count(); ++i) {
        bounding = bounding.united(geometry(i));
    }
    if (m_boundingSize != bounding.size()) {
        m_boundingSize = bounding.size();
        emit sizeChanged();
    }
}

void Screens::setCount(int count)
{
    if (m_count == count) {
        return;
    }
    const int previous = m_count;
    m_count = count;
    emit countChanged(previous, count);
}

void Screens::setCurrent(int current)
{
    if (m_current == current) {
        return;
    }
    m_current = current;
    emit currentChanged();
}

void Screens::setCurrent(const QPoint &pos)
{
    setCurrent(number(pos));
}

void Screens::setCurrent(const Client *c)
{
    if (!c->isActive()) {
        return;
    }
    if (!c->isOnScreen(m_current)) {
        setCurrent(c->screen());
    }
}

void Screens::setCurrentFollowsMouse(bool follows)
{
    if (m_currentFollowsMouse == follows) {
        return;
    }
    m_currentFollowsMouse = follows;
}

int Screens::current() const
{
    if (m_currentFollowsMouse) {
        return number(Cursor::pos());
    }
    Client *client = Workspace::self()->activeClient();
    if (client && !client->isOnScreen(m_current)) {
        return client->screen();
    }
    return m_current;
}

int Screens::intersecting(const QRect &r) const
{
    int cnt = 0;
    for (int i = 0; i < count(); ++i) {
        if (geometry(i).intersects(r)) {
            ++cnt;
        }
    }
    return cnt;
}

} // namespace
