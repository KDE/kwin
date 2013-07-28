/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_DBUS_INTERFACE_H
#define KWIN_DBUS_INTERFACE_H

#include <QObject>
#include <QtDBus/QtDBus>

namespace KWin
{

/**
 * @brief This class is a wrapper for the org.kde.KWin D-Bus interface.
 *
 * The main purpose of this class is to be exported on the D-Bus as object /KWin.
 * It is a pure wrapper to provide the deprecated D-Bus methods which have been
 * removed from Workspace which used to implement the complete D-Bus interface.
 *
 * Nowadays the D-Bus interfaces are distributed, parts of it are exported on
 * /Compositor, parts on /Effects and parts on /KWin. The implementation in this
 * class just delegates the method calls to the actual implementation in one of the
 * three singletons.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 **/
class DBusInterface: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin")
public:
    explicit DBusInterface(QObject *parent);
    virtual ~DBusInterface();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    Q_NOREPLY void cascadeDesktop();
    int currentDesktop();
    Q_NOREPLY void killWindow();
    void nextDesktop();
    void previousDesktop();
    Q_NOREPLY void reconfigure();
    bool setCurrentDesktop(int desktop);
    bool startActivity(const QString &in0);
    bool stopActivity(const QString &in0);
    QString supportInformation();
    Q_NOREPLY void unclutterDesktop();

private Q_SLOTS:
    void becomeKWinService(const QString &service);
};

} // namespace

#endif // KWIN_DBUS_INTERFACE_H
