/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Lionel Chauvin <megabigbug@yahoo.fr>
    SPDX-FileCopyrightText: 2011, 2012 Cédric Bellegarde <gnumdk@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "appmenu.h"
#include "workspace.h"
#include "x11window.h"
#include <appmenu_interface.h>

#include <QDBusObjectPath>
#include <QDBusServiceWatcher>

#include "decorations/decorationbridge.h"
#include <KDecoration2/DecorationSettings>

namespace KWin
{

static const QString s_viewService(QStringLiteral("org.kde.kappmenuview"));

ApplicationMenu::ApplicationMenu()
    : m_appmenuInterface(new OrgKdeKappmenuInterface(QStringLiteral("org.kde.kappmenu"), QStringLiteral("/KAppMenu"), QDBusConnection::sessionBus(), this))
{
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::showRequest, this, &ApplicationMenu::slotShowRequest);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuShown, this, &ApplicationMenu::slotMenuShown);
    connect(m_appmenuInterface, &OrgKdeKappmenuInterface::menuHidden, this, &ApplicationMenu::slotMenuHidden);

    m_kappMenuWatcher = new QDBusServiceWatcher(QStringLiteral("org.kde.kappmenu"), QDBusConnection::sessionBus(),
                                                QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);

    connect(m_kappMenuWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        m_applicationMenuEnabled = true;
        Q_EMIT applicationMenuEnabledChanged(true);
    });
    connect(m_kappMenuWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        m_applicationMenuEnabled = false;
        Q_EMIT applicationMenuEnabledChanged(false);
    });

    m_applicationMenuEnabled = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.kappmenu"));
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
    // Ignore show request when user has not configured the application menu title bar button
    auto decorationSettings = Workspace::self()->decorationBridge()->settings();
    if (decorationSettings && !decorationSettings->decorationButtonsLeft().contains(KDecoration2::DecorationButtonType::ApplicationMenu)
        && !decorationSettings->decorationButtonsRight().contains(KDecoration2::DecorationButtonType::ApplicationMenu)) {
        return;
    }

    if (Window *window = findWindowWithApplicationMenu(serviceName, menuObjectPath)) {
        window->showApplicationMenu(actionId);
    }
}

void ApplicationMenu::slotMenuShown(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (Window *window = findWindowWithApplicationMenu(serviceName, menuObjectPath)) {
        window->setApplicationMenuActive(true);
    }
}

void ApplicationMenu::slotMenuHidden(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (Window *window = findWindowWithApplicationMenu(serviceName, menuObjectPath)) {
        window->setApplicationMenuActive(false);
    }
}

void ApplicationMenu::showApplicationMenu(const QPoint &p, Window *c, int actionId)
{
    if (!c->hasApplicationMenu()) {
        return;
    }
    m_appmenuInterface->showMenu(p.x(), p.y(), c->applicationMenuServiceName(), QDBusObjectPath(c->applicationMenuObjectPath()), actionId);
}

Window *ApplicationMenu::findWindowWithApplicationMenu(const QString &serviceName, const QDBusObjectPath &menuObjectPath)
{
    if (serviceName.isEmpty() || menuObjectPath.path().isEmpty()) {
        return nullptr;
    }

    return Workspace::self()->findAbstractClient([&](const Window *window) {
        return window->applicationMenuServiceName() == serviceName
            && window->applicationMenuObjectPath() == menuObjectPath.path();
    });
}

} // namespace KWin
