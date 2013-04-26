/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (c) 2011 Lionel Chauvin <megabigbug@yahoo.fr>
Copyright (c) 2011,2012 Cédric Bellegarde <gnumdk@gmail.com>
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
#include "appmenu.h"
#include "client.h"
#include "workspace.h"
// Qt
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>

namespace KWin {

static const char *KDED_SERVICE = "org.kde.kded";
static const char *KDED_APPMENU_PATH = "/modules/appmenu";
static const char *KDED_INTERFACE = "org.kde.kded";

KWIN_SINGLETON_FACTORY(ApplicationMenu)

ApplicationMenu::ApplicationMenu(QObject *parent)
    : QObject(parent)
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.connect(KDED_SERVICE, KDED_APPMENU_PATH, KDED_INTERFACE, "showRequest",
                 this, SLOT(slotShowRequest(qulonglong)));
    dbus.connect(KDED_SERVICE, KDED_APPMENU_PATH, KDED_INTERFACE, "menuAvailable",
                 this, SLOT(slotMenuAvailable(qulonglong)));
    dbus.connect(KDED_SERVICE, KDED_APPMENU_PATH, KDED_INTERFACE, "menuHidden",
                 this, SLOT(slotMenuHidden(qulonglong)));
    dbus.connect(KDED_SERVICE, KDED_APPMENU_PATH, KDED_INTERFACE, "clearMenus",
                 this, SLOT(slotClearMenus()));
}

ApplicationMenu::~ApplicationMenu()
{
    s_self = NULL;
}

bool ApplicationMenu::hasMenu(xcb_window_t window)
{
    return m_windowsMenu.removeOne(window);
}

void ApplicationMenu::slotShowRequest(qulonglong wid)
{
    if (Client *c = Workspace::self()->findClient(WindowMatchPredicate(wid)))
        c->emitShowRequest();
}

void ApplicationMenu::slotMenuAvailable(qulonglong wid)
{
    if (Client *c = Workspace::self()->findClient(WindowMatchPredicate(wid)))
        c->setAppMenuAvailable();
    else
        m_windowsMenu.append(wid);
}

void ApplicationMenu::slotMenuHidden(qulonglong wid)
{
    if (Client *c = Workspace::self()->findClient(WindowMatchPredicate(wid)))
        c->emitMenuHidden();
}

void ApplicationMenu::slotClearMenus()
{
    foreach (Client *c, Workspace::self()->clientList()) {
       c->setAppMenuUnavailable();
    }
}

void ApplicationMenu::showApplicationMenu(const QPoint &p, const xcb_window_t id)
{
    QList<QVariant> args = QList<QVariant>() << p.x() << p.y() << qulonglong(id);
    QDBusMessage method = QDBusMessage::createMethodCall(KDED_SERVICE, KDED_APPMENU_PATH, KDED_INTERFACE, "showMenu");
    method.setArguments(args);
    QDBusConnection::sessionBus().asyncCall(method);
}

} // namespace
