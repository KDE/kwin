/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2005 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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

// read additional window rules and add them to kwinrulesrc

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <QtDBus/QtDBus>

int main( int argc, char* argv[] )
{
    KAboutData about( "kwin_update_tabbox_settings", "kwin", KLocalizedString(), 0 );
    KCmdLineArgs::init( argc, argv, &about );
    KComponentData inst( &about );
    Q_UNUSED( KGlobal::locale() ); // jump-start locales to get to translated descriptions
    KConfig config( "kwinrc" );
    KConfigGroup windows(&config, "Windows");
    KConfigGroup tabbox(&config, "TabBox");
    const bool traverse = tabbox.readEntry<bool>("TraverseAll", false);
    const QString style = windows.readEntry<QString>("AltTabStyle", "KDE");
    if( !tabbox.hasKey("ListMode") )
        tabbox.writeEntry("ListMode", traverse?1:0);
    if( !tabbox.hasKey("ShowTabBox") )
        tabbox.writeEntry("ShowTabBox", (style.compare("KDE", Qt::CaseInsensitive) == 0)?true:false);
    tabbox.sync();
    config.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
    QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
    
}
