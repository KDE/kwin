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
#include "update_kwin_49.h"

#include <kconfig.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <QtDBus/QtDBus>

int main( int argc, char* argv[] )
{
    KAboutData about( "kwin_update_tabbox_qml_settings", "kwin", KLocalizedString(), 0 );
    KCmdLineArgs::init( argc, argv, &about );
    KComponentData inst( &about );
    Q_UNUSED( KGlobal::locale() ); // jump-start locales to get to translated descriptions
    KConfig config("kwinrc");
    migratePresentWindowsTabBox(config);
    migrateDesktopChangeOSD(config);
    migrateTabBoxConfig(config.group("TabBox"));
    migrateTabBoxConfig(config.group("TabBoxAlternative"));
    config.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
    QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}
