/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Lionel Chauvin <megabigbug@yahoo.fr>
    SPDX-FileCopyrightText: 2011, 2012 Cédric Bellegarde <gnumdk@gmail.com>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
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

class Window;

class ApplicationMenu : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationMenu();

    void showApplicationMenu(const QPoint &pos, Window *c, int actionId);

    bool applicationMenuEnabled() const;

    void setViewEnabled(bool enabled);

Q_SIGNALS:
    void applicationMenuEnabledChanged(bool enabled);

private Q_SLOTS:
    void slotShowRequest(const QString &serviceName, const QDBusObjectPath &menuObjectPath, int actionId);
    void slotMenuShown(const QString &serviceName, const QDBusObjectPath &menuObjectPath);
    void slotMenuHidden(const QString &serviceName, const QDBusObjectPath &menuObjectPath);

private:
    OrgKdeKappmenuInterface *m_appmenuInterface;
    QDBusServiceWatcher *m_kappMenuWatcher;

    Window *findWindowWithApplicationMenu(const QString &serviceName, const QDBusObjectPath &menuObjectPath);

    bool m_applicationMenuEnabled = false;
};

}
