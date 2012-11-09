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
#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>
#include <KDE/KComponentData>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <QtDBus/QtDBus>

void migratePlastikToAurorae(KConfig &config)
{
    // migrates selected window decoration from Plastik to Aurorae theme of Plastik
    if (!config.hasGroup("Style")) {
        return;
    }
    KConfigGroup style = config.group("Style");
    if (!style.hasKey("PluginLib")) {
        return;
    }
    if (style.readEntry("PluginLib", "kwin3_oxygen") != "kwin3_plastik") {
        return;
    }
    style.writeEntry("PluginLib", "kwin3_aurorae");
    style.sync();

    // And the Aurorae config
    KConfig aurorae("auroraerc");
    KConfigGroup engine = aurorae.group("Engine");
    engine.writeEntry("EngineType", "qml");
    engine.writeEntry("ThemeName", "kwin4_decoration_qml_plastik");
    engine.sync();
    aurorae.sync();
}

void migratePlastikSettings()
{
    KConfig aurorae("auroraerc");
    if (aurorae.hasGroup("kwin4_decoration_qml_plastik")) {
        // already migrated
        return;
    }

    // read the Plastik configuration
    KConfig plastik("kwinplastikrc");
    if (!plastik.hasGroup("General")) {
        // nothing to migrate
        return;
    }
    KConfigGroup cg = plastik.group("General");
    KConfigGroup auroraePlastik = aurorae.group("kwin4_decoration_qml_plastik");
    if (cg.hasKey("CloseOnMenuDoubleClick")) {
        auroraePlastik.writeEntry("CloseOnDoubleClickMenuButton", cg.readEntry("CloseOnMenuDoubleClick", true));
    }
    // everything else is in a general group
    KConfigGroup general = auroraePlastik.group("General");
    if (cg.hasKey("AnimateButtons")) {
        general.writeEntry("animateButtons", cg.readEntry("AnimateButtons", true));
    }
    if (cg.hasKey("TitleShadow")) {
        general.writeEntry("titleShadow", cg.readEntry("TitleShadow", true));
    }
    if (cg.hasKey("ColoredBorder")) {
        general.writeEntry("coloredBorder", cg.readEntry("ColoredBorder", true));
    }
    if (cg.hasKey("TitleAlignment")) {
        bool left, center, right;
        left = center = right = false;
        const QString titleAlignment = cg.readEntry("TitleAlignment", "AlignLeft");
        if (titleAlignment == "AlignLeft") {
            left = true;
        } else if (titleAlignment == "AlignHCenter") {
            center = true;
        } else if (titleAlignment == "AlignRight") {
            right = true;
        }
        general.writeEntry("titleAlignLeft", left);
        general.writeEntry("titleAlignCenter", center);
        general.writeEntry("titleAlignRight", right);
    }

    general.sync();
    auroraePlastik.sync();
    aurorae.sync();
}

void migrateTranslucencySetting(KConfigGroup &group, const QString &key, qreal oldDefault)
{
    if (!group.hasKey(key)) {
        return;
    }
    const qreal value = group.readEntry(key, oldDefault);
    if (value == oldDefault) {
        group.deleteEntry(key);
        return;
    }
    if (value > 1.0) {
        // already migrated to new settings
        return;
    }
    const int newValue = qBound<int>(0, qRound(value * 100), 100);
    group.writeEntry(key, newValue);
}

void migrateTranslucencySettings(KConfig &config)
{
    if (!config.hasGroup("Effect-Translucency")) {
        return;
    }
    KConfigGroup cg = config.group("Effect-Translucency");
    migrateTranslucencySetting(cg, "Decoration", 1.0);
    migrateTranslucencySetting(cg, "MoveResize", 0.8);
    migrateTranslucencySetting(cg, "Dialogs", 1.0);
    migrateTranslucencySetting(cg, "Inactive", 1.0);
    migrateTranslucencySetting(cg, "ComboboxPopups", 1.0);
    migrateTranslucencySetting(cg, "Menus", 1.0);
    migrateTranslucencySetting(cg, "DropdownMenus", 1.0);
    migrateTranslucencySetting(cg, "PopupMenus", 1.0);
    migrateTranslucencySetting(cg, "TornOffMenus", 1.0);
    cg.sync();
}

int main( int argc, char* argv[] )
{
    KAboutData about( "kwin_update_settings_4_10", "kwin", KLocalizedString(), 0 );
    KCmdLineArgs::init( argc, argv, &about );
    KComponentData inst( &about );
    Q_UNUSED( KGlobal::locale() ); // jump-start locales to get to translated descriptions
    KConfig config("kwinrc");
    migratePlastikToAurorae(config);
    migratePlastikSettings();
    migrateTranslucencySettings(config);
    config.sync();
    // Send signal to all kwin instances
    QDBusMessage message =
    QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
}
