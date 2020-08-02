/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Lionel Chauvin <megabigbug@yahoo.fr>
    SPDX-FileCopyrightText: 2011, 2012 Cédric Bellegarde <gnumdk@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_APPMENU_H
#define KWIN_APPMENU_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QObject>
// xcb
#include <xcb/xcb.h>

class QPoint;
class OrgKdeKappmenuInterface;
class QDBusObjectPath;
class QDBusServiceWatcher;

namespace KWin
{

class AbstractClient;

class ApplicationMenu : public QObject
{
    Q_OBJECT

public:
    ~ApplicationMenu() override;

    void showApplicationMenu(const QPoint &pos, AbstractClient *c, int actionId);

    bool applicationMenuEnabled() const;

    void setViewEnabled(bool enabled);

signals:
    void applicationMenuEnabledChanged(bool enabled);

private Q_SLOTS:
    void slotShowRequest(const QString &serviceName, const QDBusObjectPath &menuObjectPath, int actionId);
    void slotMenuShown(const QString &serviceName, const QDBusObjectPath &menuObjectPath);
    void slotMenuHidden(const QString &serviceName, const QDBusObjectPath &menuObjectPath);

private:
    OrgKdeKappmenuInterface *m_appmenuInterface;
    QDBusServiceWatcher *m_kappMenuWatcher;

    AbstractClient *findAbstractClientWithApplicationMenu(const QString &serviceName, const QDBusObjectPath &menuObjectPath);

    bool m_applicationMenuEnabled = false;

    KWIN_SINGLETON(ApplicationMenu)
};

}

#endif // KWIN_APPMENU_H
