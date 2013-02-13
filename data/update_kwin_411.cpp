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
#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>
#include <KDE/KComponentData>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocalizedString>
// Qt
#include <QDBusConnection>
#include <QDBusMessage>

bool migrateRulesShortcut()
{
    const QString KEY = "shortcut";
    const QString ALT = "Alt";
    const QString CTRL = "Ctrl";
    const QString META = "Meta";
    const QString SHIFT = "Shift";

    KConfig config("kwinrulesrc");
    if (config.groupList().isEmpty()) {
        return false;
    }
    bool updated = false;
    Q_FOREACH (const QString &groupName, config.groupList()) {
        KConfigGroup group = config.group(groupName);
        if (!group.hasKey(KEY)) {
            continue;
        }
        const QString value = group.readEntry(KEY, QString());
        if (value.contains(" - ")) {
            // already migrated
            continue;
        }
        if (!value.contains(' ')) {
            // nothing to migrate
            continue;
        }
        // space might be either the shortcut separator or part of the shortcut
        // let's try to detect it properly
        const QStringList shortcuts = value.split(' ');
        // let's take the first part as it is
        QString newValue = shortcuts.first();
        for (int i=1; i<shortcuts.length(); ++i) {
            const QString &cs = shortcuts.at(i);
            if (cs.contains('+') && (cs.contains(ALT) || cs.contains(CTRL) || cs.contains(META) || cs.contains(SHIFT))) {
                // our shortcuts consist of at least one modifier and a key, so having a plus and a modifier means it's a shortcut
                newValue.append(" - ");
            } else {
                // otherwise it's part of a key like "Volume Up"
                newValue.append(' ');
            }
            newValue.append(cs);
        }
        group.writeEntry(KEY, newValue);
        group.sync();
        updated = true;
    }
    if (updated) {
        config.sync();
    }
    return updated;
}

int main( int argc, char* argv[] )
{
    KAboutData about( "kwin_update_settings_4_11", "kwin", KLocalizedString(), 0 );
    KCmdLineArgs::init( argc, argv, &about );
    KComponentData inst( &about );
    Q_UNUSED( KGlobal::locale() ); // jump-start locales to get to translated descriptions
    bool reload = migrateRulesShortcut();
    // Send signal to all kwin instances
    if (reload) {
        QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
        QDBusConnection::sessionBus().send(message);
    }
}
