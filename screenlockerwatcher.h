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
#ifndef KWIN_SCREENLOCKERWATCHER_H
#define KWIN_SCREENLOCKERWATCHER_H

#include <QObject>

#include <kwinglobals.h>

class OrgFreedesktopScreenSaverInterface;
class OrgKdeScreensaverInterface;
class QDBusServiceWatcher;
class QDBusPendingCallWatcher;

namespace KWin
{

class KWIN_EXPORT ScreenLockerWatcher : public QObject
{
    Q_OBJECT
public:
    ~ScreenLockerWatcher() override;
    bool isLocked() const {
        return m_locked;
    }
Q_SIGNALS:
    void locked(bool locked);
    void aboutToLock();
private Q_SLOTS:
    void setLocked(bool activated);
    void activeQueried(QDBusPendingCallWatcher *watcher);
    void serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    void serviceRegisteredQueried();
    void serviceOwnerQueried();
private:
    void initialize();
    OrgFreedesktopScreenSaverInterface *m_interface = nullptr;
    OrgKdeScreensaverInterface *m_kdeInterface = nullptr;
    QDBusServiceWatcher *m_serviceWatcher;
    bool m_locked;

    KWIN_SINGLETON(ScreenLockerWatcher)
};
}

#endif
