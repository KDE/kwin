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

using namespace KWin;

KWIN_SINGLETON_FACTORY(ApplicationMenu)

ApplicationMenu::ApplicationMenu(QObject *parent)
    : QObject(parent)
    , m_appmenuInterface(new OrgKdeKappmenuInterface(QStringLiteral("org.kde.kappmenu"), QStringLiteral("/KAppMenu"), QDBusConnection::sessionBus(), this))
{
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::showRequest, this, &ApplicationMenu::slotShowRequest);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuShown, this, &ApplicationMenu::slotMenuShown);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuHidden, this, &ApplicationMenu::slotMenuHidden);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::reconfigured, this, &ApplicationMenu::slotReconfigured);

    updateApplicationMenuEnabled();
}

ApplicationMenu::~ApplicationMenu()
{
    s_self = nullptr;
}

void ApplicationMenu::slotReconfigured()
{
    updateApplicationMenuEnabled();
}

bool ApplicationMenu::applicationMenuEnabled() const
{
    return m_applicationMenuEnabled;
}

void ApplicationMenu::updateApplicationMenuEnabled()
{
    const bool old_enabled = m_applicationMenuEnabled;

    KConfigGroup config(KSharedConfig::openConfig(QStringLiteral("kdeglobals")), QStringLiteral("Appmenu Style"));
    const QString &menuStyle = config.readEntry(QStringLiteral("Style"));

    const bool enabled = (menuStyle == QLatin1String("Decoration"));

    if (old_enabled != enabled) {
        m_applicationMenuEnabled = enabled;
        emit applicationMenuEnabledChanged(enabled);
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
