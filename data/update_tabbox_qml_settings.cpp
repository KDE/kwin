/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kcomponentdata.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#include <QtDBus/QtDBus>

void updateTabBoxConfig(KConfigGroup &tabbox, bool migrate)
{
    tabbox.deleteEntry("LayoutMode");
    tabbox.deleteEntry("SelectedItem");
    tabbox.deleteEntry("MinWidth");
    tabbox.deleteEntry("MinHeight");
    tabbox.deleteEntry("SelectedLayoutName");
    if (migrate) {
        tabbox.writeEntry("LayoutName", "thumbnails");
    }
    tabbox.sync();
}

int main( int argc, char* argv[] )
{
    KAboutData about( "kwin_update_tabbox_qml_settings", "kwin", KLocalizedString(), 0 );
    KCmdLineArgs::init( argc, argv, &about );
    KComponentData inst( &about );
    Q_UNUSED( KGlobal::locale() ); // jump-start locales to get to translated descriptions
    KConfig config("kwinrc");
    KConfigGroup plugins = config.group("Plugins");
    const bool boxSwitchEnabled = plugins.readEntry<bool>("kwin4_effect_boxswitchEnabled", true);
    KConfigGroup boxswitch = config.group("Effect-BoxSwitch");
    const bool boxSwitchPrimary = boxSwitchEnabled && boxswitch.hasKey("TabBox") && boxswitch.readEntry<bool>("TabBox", true);
    const bool boxSwitchAlternative = boxSwitchEnabled && boxswitch.hasKey("TabBoxAlternative") && boxswitch.readEntry<bool>("TabBoxAlternative", false);
    boxswitch.writeEntry("TabBox", false);
    boxswitch.writeEntry("TabBoxAlternative", false);
    boxswitch.sync();
    KConfigGroup tabbox = config.group("TabBox");
    updateTabBoxConfig(tabbox, boxSwitchPrimary);
    KConfigGroup tabboxAlternative = config.group("TabBoxAlternative");
    updateTabBoxConfig(tabboxAlternative, boxSwitchAlternative);
    config.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
    QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);    
}
