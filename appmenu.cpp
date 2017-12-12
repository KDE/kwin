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
#include <appmenu_interface.h>

#include <QDBusObjectPath>
#include <QDBusServiceWatcher>

using namespace KWin;

KWIN_SINGLETON_FACTORY(ApplicationMenu)

static const QString s_viewService(QStringLiteral("org.kde.kappmenuview"));

ApplicationMenu::ApplicationMenu(QObject *parent)
    : QObject(parent)
    , m_appmenuInterface(new OrgKdeKappmenuInterface(QStringLiteral("org.kde.kappmenu"), QStringLiteral("/KAppMenu"), QDBusConnection::sessionBus(), this))
{
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::showRequest, this, &ApplicationMenu::slotShowRequest);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuShown, this, &ApplicationMenu::slotMenuShown);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuHidden, this, &ApplicationMenu::slotMenuHidden);
    
    m_kappMenuWatcher = new QDBusServiceWatcher(QStringLiteral("org.kde.kappmenu"), QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration|QDBusServiceWatcher::WatchForUnregistration, this);

    connect(m_kappMenuWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this] () {
                m_applicationMenuEnabled = true;
                emit applicationMenuEnabledChanged(true);
            });
    connect(m_kappMenuWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this] () {
                m_applicationMenuEnabled = false;
                emit applicationMenuEnabledChanged(false);
            });

    m_applicationMenuEnabled = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kappmenu"));
}

ApplicationMenu::~ApplicationMenu()
{
    s_self = nullptr;
}

bool ApplicationMenu::applicationMenuEnabled() const
{
    return m_applicationMenuEnabled;
}

void ApplicationMenu::setViewEnabled(bool enabled)
{
    if (enabled) {
        QDBusConnection::sessionBus().interface()->registerService(s_viewService,
                    QDBusConnectionInterface::QueueService,
                    QDBusConnectionInterface::DontAllowReplacement);
    } else {
        QDBusConnection::sessionBus().interface()->unregisterService(s_viewService);
    }
}

void ApplicationMenu::slotShowRequest(const QString &serviceName, const QDBusObjectPath &menuObjectPath, int actionId)
{
    if (AbstractClient *c = findAbstractClientWithApplicationMenu(serviceName, menuObjectPath)) {
        c->showApplicationMenu(actionId);
    }
}

void ApplicationMenu::slotMenuShown(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (AbstractClient *c = findAbstractClientWithApplicationMenu(serviceName, menuObjectPath)) {
        c->setApplicationMenuActive(true);
    }
}

void ApplicationMenu::slotMenuHidden(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (AbstractClient *c = findAbstractClientWithApplicationMenu(serviceName, menuObjectPath)) {
        c->setApplicationMenuActive(false);
    }
}

void ApplicationMenu::showApplicationMenu(const QPoint &p, AbstractClient *c, int actionId)
{
    m_appmenuInterface->showMenu(p.x(), p.y(), c->applicationMenuServiceName(), QDBusObjectPath(c->applicationMenuObjectPath()), actionId);
}

AbstractClient *ApplicationMenu::findAbstractClientWithApplicationMenu(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (serviceName.isEmpty() || menuObjectPath.path().isEmpty()) {
        return nullptr;
    }

    return Workspace::self()->findAbstractClient([&](const AbstractClient *c) {
        return c->applicationMenuServiceName() == serviceName
        && c->applicationMenuObjectPath() == menuObjectPath.path();
    });
}
