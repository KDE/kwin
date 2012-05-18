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
#include <KDE/KConfig>

void migratePresentWindowsTabBox(KConfig &config)
{
    KConfigGroup plugins = config.group("Plugins");
    const bool presentWindowsEnabled = plugins.readEntry("kwin4_effect_presentwindowsEnabled", true);
    if (!presentWindowsEnabled) {
        // nothing to migrate
        return;
    }
    KConfigGroup presentWindows = config.group("Effect-PresentWindows");
    const bool presentWindowsPrimary = presentWindows.readEntry("TabBox", false);
    const bool presentWindowsAlternative = presentWindows.readEntry("TabBoxAlternative", false);
    if (presentWindowsPrimary) {
        KConfigGroup tabbox = config.group("TabBox");
        tabbox.writeEntry("LayoutName", "present_windows");
        tabbox.sync();
    }
    if (presentWindowsAlternative) {
        KConfigGroup tabbox = config.group("TabBoxAlternative");
        tabbox.writeEntry("LayoutName", "present_windows");
        tabbox.sync();
    }
    presentWindows.deleteEntry("TabBox");
    presentWindows.deleteEntry("TabBoxAlternative");
    presentWindows.sync();
}

void migrateDesktopChangeOSD(KConfig &config)
{
    if (!config.hasGroup("PopupInfo")) {
        return;
    }
    KConfigGroup popupInfo = config.group("PopupInfo");
    const bool shown = popupInfo.readEntry("ShowPopup", false);
    const bool textOnly = popupInfo.readEntry("TextOnly", false);
    const int delayTime = popupInfo.readEntry("PopupHideDelay", 1000);

    KConfigGroup plugins = config.group("Plugins");
    if (shown && !plugins.hasKey("desktopchangeosdEnabled")) {
        plugins.writeEntry("desktopchangeosdEnabled", true);
        plugins.sync();
    }
    KConfigGroup osd = config.group("Script-desktopchangeosd");
    if (popupInfo.hasKey("TextOnly") && !osd.hasKey("TextOnly")) {
        osd.writeEntry("TextOnly", textOnly);
    }
    if (popupInfo.hasKey("PopupHideDelay") && !osd.hasKey("PopupHideDelay")) {
        osd.writeEntry("PopupHideDelay", delayTime);
    }
    osd.sync();
    config.deleteGroup("PopupInfo");
}

void migrateTabBoxConfig(KConfigGroup tabbox)
{
    if (tabbox.hasKey("ListMode") && !tabbox.hasKey("DesktopMode")) {
        const int oldValue = tabbox.readEntry("ListMode", 0);
        switch (oldValue) {
        case 0: // Current Desktop Client List
        case 2: // Current Desktop Application List
            tabbox.writeEntry("DesktopMode", 1);
            break;
        case 1: // All Desktops Client List
        case 3: // All Desktops Application List
            tabbox.writeEntry("DesktopMode", 0);
            break;
        }
    }
    tabbox.deleteEntry("ListMode");
    if (tabbox.hasKey("ShowDesktop") && !tabbox.hasKey("ShowDesktopMode")) {
        const bool showDesktop = tabbox.readEntry("ShowDesktop", false);
        tabbox.writeEntry("ShowDesktopMode", showDesktop ? 1 : 0);
    }
    tabbox.deleteEntry("ShowDesktop");
    tabbox.sync();
}
