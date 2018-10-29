/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2018 Marco Martin <mart@kde.org>

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

// own
#include "virtualdesktopsdbustypes.h"


// Marshall the DBusDesktopDataStruct data into a D-BUS argument
const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataStruct &desk)
{
    argument.beginStructure();
    argument << desk.position;
    argument << desk.id;
    argument << desk.name;
    argument.endStructure();
    return argument;
}
// Retrieve
const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataStruct &desk)
{
    argument.beginStructure();
    argument >> desk.position;
    argument >> desk.id;
    argument >> desk.name;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator<<(QDBusArgument &argument, const KWin::DBusDesktopDataVector &deskVector)
{
    argument.beginArray(qMetaTypeId<KWin::DBusDesktopDataStruct>());
    for (int i = 0; i < deskVector.size(); ++i) {
        argument << deskVector[i];
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KWin::DBusDesktopDataVector &deskVector)
{
    argument.beginArray();
    deskVector.clear();

    while (!argument.atEnd()) {
        KWin::DBusDesktopDataStruct element;
        argument >> element;
        deskVector.append(element);
    }

    argument.endArray();

    return argument;
}
