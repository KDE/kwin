/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020, Méven Car <meven.car@enika.com>

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

#ifndef SERVICE_UTILS_H
#define SERVICE_UTILS_H

// cmake stuff
#include <config-kwin.h>
// kwin
#include <kwinglobals.h>
// Qt
#include <QFileInfo>
#include <QLoggingCategory>
//KF
#include <KApplicationTrader>

namespace KWin
{

const static QString s_waylandInterfaceName = QStringLiteral("X-KDE-Wayland-Interfaces");
const static QString s_dbusRestrictedInterfaceName = QStringLiteral("X-KDE-DBUS-Restricted-Interfaces");

static QStringList fetchProcessServiceField(const QString &executablePath, const QString &fieldName)
{
    // needed to be able to use the logging category in a header static function
    static QLoggingCategory KWIN_UTILS ("KWIN_UTILS");
    const auto servicesFound = KApplicationTrader::query([&executablePath] (const KService::Ptr &service) {

        if (service->exec().isEmpty() || service->exec() != executablePath)
            return false;

        return true;
    });

    if (servicesFound.isEmpty()) {
        qCDebug(KWIN_UTILS) << "Could not find the desktop file for" << executablePath;
        return {};
    }

    const auto fieldValues = servicesFound.first()->property(fieldName).toStringList();
    if (KWIN_UTILS().isDebugEnabled()) {
        qCDebug(KWIN_UTILS) << "Interfaces found for" << executablePath << fieldName << ":" << fieldValues;
    }
    return fieldValues;
}

static QStringList fetchRequestedInterfaces(const QString &executablePath)
{
    return fetchProcessServiceField(executablePath, s_waylandInterfaceName);
}

static QStringList fetchRestrictedDBusInterfacesFromPid(const uint pid)
{
    const auto executablePath = QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
    return fetchProcessServiceField(executablePath, s_dbusRestrictedInterfaceName);
}

}// namespace

#endif // SERVICE_UTILS_H
