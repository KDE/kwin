/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_VIRTUALDESKTOPS_DBUS_TYPES_H
#define KWIN_VIRTUALDESKTOPS_DBUS_TYPES_H

#include <QtDBus>

namespace KWin
{

struct DBusDesktopDataStruct {
    uint position;
    QString id;
    QString name;
};
typedef QVector<DBusDesktopDataStruct> DBusDesktopDataVector;
}

const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataStruct &desk);
const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataStruct &desk);

Q_DECLARE_METATYPE(KWin::DBusDesktopDataStruct)

const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataVector &deskVector);
const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataVector &deskVector);

Q_DECLARE_METATYPE(KWin::DBusDesktopDataVector)

#endif // KWIN_VIRTUALDESKTOPS_DBUS_TYPES_H
