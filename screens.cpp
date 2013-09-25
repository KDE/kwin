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
#include "client.h"
#include "cursor.h"
#include "settings.h"
#include "workspace.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>

namespace KWin
{

KWIN_SINGLETON_FACTORY_FACTORED(Screens, DesktopWidgetScreens)

Screens::Screens(QObject *parent)
    : QObject(parent)
    , m_count(0)
    , m_current(0)
    , m_currentFollowsMouse(false)
    , m_changedTimer(new QTimer(this))
{
    m_changedTimer->setSingleShot(true);
    m_changedTimer->setInterval(100);
    connect(m_changedTimer, SIGNAL(timeout()), SLOT(updateCount()));
    connect(m_changedTimer, SIGNAL(timeout()), SIGNAL(changed()));

    Settings settings;
    settings.setDefaults();
    m_currentFollowsMouse = settings.activeMouseScreen();
}

Screens::~Screens()
{
    s_self = NULL;
}

void Screens::reconfigure()
{
    if (!m_config) {
        return;
    }
    Settings settings(m_config);
    settings.readConfig();
    setCurrentFollowsMouse(settings.activeMouseScreen());
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

DesktopWidgetScreens::DesktopWidgetScreens(QObject *parent)
    : Screens(parent)
    , m_desktop(QApplication::desktop())
{
    connect(m_desktop, SIGNAL(screenCountChanged(int)), SLOT(startChangedTimer()));
    connect(m_desktop, SIGNAL(resized(int)), SLOT(startChangedTimer()));
    updateCount();
}

DesktopWidgetScreens::~DesktopWidgetScreens()
{
}

QRect DesktopWidgetScreens::geometry(int screen) const
{
    if (Screens::self()->isChanging())
        const_cast<DesktopWidgetScreens*>(this)->updateCount();
    return m_desktop->screenGeometry(screen);
}

int DesktopWidgetScreens::number(const QPoint &pos) const
{
    if (Screens::self()->isChanging())
        const_cast<DesktopWidgetScreens*>(this)->updateCount();
    return m_desktop->screenNumber(pos);
}

void DesktopWidgetScreens::updateCount()
{
    setCount(m_desktop->screenCount());
}

} // namespace
