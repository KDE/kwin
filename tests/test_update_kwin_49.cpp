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
#include "../data/update_kwin_49.h"
#include "../tabbox/tabboxconfig.h"

#include <KDE/KConfig>

#include <QtTest/QtTest>

class TestUpdateKWin49 : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Tests that migrating the Present Windows TabBox settings
     * does not affect an empty configuration.
     **/
    void testEmptyKConfigPW();
    /**
     * Tests that the migration of Present Windows TabBox settings for a
     * disabled Present Windows effect does not affect the configuration.
     **/
    void testPWDisabled();
    /**
     * Tests that the migration of Present Window TabBox settings for an
     * enabled Present Windows effect with disabled TabBox does not affect the configuration.
     **/
    void testPWEnabledTabBoxDisabled();
    /**
     * Tests that the migration of Present Windows TabBox settings
     * properly sets the layout in TabBox.
     **/
    void testPWTabBoxEnabled();
    /**
     * Tests that the migration of Present Windows TabBox settings
     * properly sets the layout in TabBoxAlternative.
     **/
    void testPWTabBoxAlternativeEnabled();
    /**
     * Tests that the migration of Present Windows TabBox settings
     * properly sets the layout in both TabBox and TabBoxAlternative.
     **/
    void testPWBothEnabled();
    /**
     * Test that migrating the Desktop Change OSD settings
     * does not affect an empty configuration
     **/
    void testEmptyKConfigOSD();
    /**
     * Tests that migrating the Desktop Change OSD settings
     * with a disabled PopupInfo does not affect KConfig.
     **/
    void testPopupInfoDisabled();
    /**
     * Tests that migrating the Desktop Change OSD settings
     * with a disabled PopupInfo migrates only the config parameters.
     **/
    void testPopupInfoDisabledAdditionalKeys();
    /**
     * Tests that migrating the Desktop Change OSD settings
     * with a disabled PopupInfo migrates only the config parameters which have default values.
     **/
    void testPopupInfoDisabledAdditionalKeysDefault();
    /**
     * Tests that migrating the Desktop Change OSD settings
     * enables the script.
     **/
    void testPopupInfoEnabled();
    /**
     * Tests that migrating the Desktop Change OSD settings
     * enables the script and migrates the settings.
     **/
    void testPopupInfoEnabledAdditionalKeys();
    /**
     * Tests that attempting to migrate Desktop Change OSD settings
     * again will not overwrite existing settings.
     **/
    void testPopupInfoAlreadyMigrated();
    /**
     * Tests that migrating TabBox does not change an empty KConfig.
     **/
    void testEmptyKConfigTabBox();
    /**
     * Tests the migration of TabBox setting show desktop.
     **/
    void testTabBoxShowDesktopEnabled();
    /**
     * Tests the migration of TabBox setting show desktop.
     **/
    void testTabBoxShowDesktopDisabled();
    /**
     * Tests the migration of the various TabBox ListMode settings.
     **/
    void testTabBoxCurrentDesktopClientList();
    void testTabBoxCurrentDesktopApplicationList();
    void testTabBoxAllDesktopsClientList();
    void testTabBoxAllDesktopsApplicationList();
    /**
     * Tests that attempting to migrate TabBox settings again will not
     * overwrite existing settings.
     **/
    void testTabBoxAlreadyMigrated();
};

void TestUpdateKWin49::testEmptyKConfigPW()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
}

void TestUpdateKWin49::testPWDisabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins = config.group("Plugins");
    plugins.writeEntry("kwin4_effect_presentwindowsEnabled", false);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(!plugins.readEntry("kwin4_effect_presentwindowsEnabled", true));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(!plugins.readEntry("kwin4_effect_presentwindowsEnabled", true));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
}

void TestUpdateKWin49::testPWEnabledTabBoxDisabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins = config.group("Plugins");
    plugins.writeEntry("kwin4_effect_presentwindowsEnabled", true);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(plugins.readEntry("kwin4_effect_presentwindowsEnabled", true));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(plugins.readEntry("kwin4_effect_presentwindowsEnabled", true));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    // same with TabBox explicitly disabled
    KConfigGroup pw = config.group("Effect-PresentWindows");
    pw.writeEntry("TabBox", false);
    pw.writeEntry("TabBoxAlternative", false);
    QVERIFY(pw.hasKey("TabBox"));
    QVERIFY(pw.hasKey("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(plugins.readEntry("kwin4_effect_presentwindowsEnabled", true));
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
}

void TestUpdateKWin49::testPWTabBoxEnabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup pw = config.group("Effect-PresentWindows");
    pw.writeEntry("TabBox", true);
    QVERIFY(pw.hasKey("TabBox"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(config.hasGroup("TabBox"));
    KConfigGroup tabBox = config.group("TabBox");
    QVERIFY(tabBox.hasKey("LayoutName"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    // test same with an explicit layout set
    tabBox.writeEntry("LayoutName", "informative");
    pw.writeEntry("TabBox", true);
    migratePresentWindowsTabBox(config);
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
}

void TestUpdateKWin49::testPWTabBoxAlternativeEnabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup pw = config.group("Effect-PresentWindows");
    pw.writeEntry("TabBoxAlternative", true);
    QVERIFY(pw.hasKey("TabBoxAlternative"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migratePresentWindowsTabBox(config);
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(config.hasGroup("TabBoxAlternative"));
    KConfigGroup tabBox = config.group("TabBoxAlternative");
    QVERIFY(tabBox.hasKey("LayoutName"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
    QVERIFY(!config.hasGroup("TabBox"));
    // test same with an explicit layout set
    tabBox.writeEntry("LayoutName", "informative");
    pw.writeEntry("TabBoxAlternative", true);
    migratePresentWindowsTabBox(config);
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
}

void TestUpdateKWin49::testPWBothEnabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup pw = config.group("Effect-PresentWindows");
    pw.writeEntry("TabBox", true);
    pw.writeEntry("TabBoxAlternative", true);
    QVERIFY(pw.hasKey("TabBoxAlternative"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    QVERIFY(pw.hasKey("TabBox"));
    migratePresentWindowsTabBox(config);
    QVERIFY(!config.hasGroup("Effect-PresentWindows"));
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(config.hasGroup("TabBox"));
    QVERIFY(config.hasGroup("TabBoxAlternative"));
    KConfigGroup tabBox = config.group("TabBox");
    QVERIFY(tabBox.hasKey("LayoutName"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    QVERIFY(tabBoxAlternative.hasKey("LayoutName"));
    QVERIFY(tabBoxAlternative.readEntry("LayoutName", "thumbnails") == "present_windows");
    // test same with an explicit layout set
    tabBox.writeEntry("LayoutName", "informative");
    tabBoxAlternative.writeEntry("LayoutName", "informative");
    pw.writeEntry("TabBox", true);
    pw.writeEntry("TabBoxAlternative", true);
    migratePresentWindowsTabBox(config);
    QVERIFY(!pw.hasKey("TabBox"));
    QVERIFY(!pw.hasKey("TabBoxAlternative"));
    QVERIFY(tabBox.readEntry("LayoutName", "thumbnails") == "present_windows");
    QVERIFY(tabBoxAlternative.readEntry("LayoutName", "thumbnails") == "present_windows");
}

void TestUpdateKWin49::testEmptyKConfigOSD()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
}

void TestUpdateKWin49::testPopupInfoDisabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("ShowPopup", false);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(popupInfo.hasKey("ShowPopup"));
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
}

void TestUpdateKWin49::testPopupInfoDisabledAdditionalKeys()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("TextOnly", false);
    popupInfo.writeEntry("PopupHideDelay", 1000);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(popupInfo.hasKey("TextOnly"));
    QVERIFY(popupInfo.hasKey("PopupHideDelay"));
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(!popupInfo.hasKey("TextOnly"));
    QVERIFY(!popupInfo.hasKey("PopupHideDelay"));
    KConfigGroup osd = config.group("Script-desktopchangeosd");
    QVERIFY(osd.hasKey("TextOnly"));
    QVERIFY(osd.hasKey("PopupHideDelay"));
    QVERIFY(!osd.readEntry("TextOnly", false));
    QVERIFY(osd.readEntry("PopupHideDelay", 1000) == 1000);
}

void TestUpdateKWin49::testPopupInfoDisabledAdditionalKeysDefault()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("TextOnly", true);
    popupInfo.writeEntry("PopupHideDelay", 200);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(popupInfo.hasKey("TextOnly"));
    QVERIFY(popupInfo.hasKey("PopupHideDelay"));
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(!popupInfo.hasKey("TextOnly"));
    QVERIFY(!popupInfo.hasKey("PopupHideDelay"));
    KConfigGroup osd = config.group("Script-desktopchangeosd");
    QVERIFY(osd.hasKey("TextOnly"));
    QVERIFY(osd.hasKey("PopupHideDelay"));
    QVERIFY(osd.readEntry("TextOnly", false));
    QVERIFY(osd.readEntry("PopupHideDelay", 1000) == 200);
}

void TestUpdateKWin49::testPopupInfoEnabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("ShowPopup", true);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(popupInfo.hasKey("ShowPopup"));
    migrateDesktopChangeOSD(config);
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    KConfigGroup plugins = config.group("Plugins");
    QVERIFY(plugins.hasKey("desktopchangeosdEnabled"));
    QVERIFY(plugins.readEntry("desktopchangeosdEnabled", false));
}

void TestUpdateKWin49::testPopupInfoEnabledAdditionalKeys()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("ShowPopup", true);
    popupInfo.writeEntry("TextOnly", true);
    popupInfo.writeEntry("PopupHideDelay", 2000);
    QVERIFY(!config.hasGroup("Plugins"));
    QVERIFY(!config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(popupInfo.hasKey("ShowPopup"));
    QVERIFY(popupInfo.hasKey("TextOnly"));
    QVERIFY(popupInfo.hasKey("PopupHideDelay"));
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(config.hasGroup("Plugins"));
    QVERIFY(config.hasGroup("Script-desktopchangeosd"));
    QVERIFY(!popupInfo.hasKey("ShowPopup"));
    QVERIFY(!popupInfo.hasKey("TextOnly"));
    QVERIFY(!popupInfo.hasKey("PopupHideDelay"));
    KConfigGroup osd = config.group("Script-desktopchangeosd");
    QVERIFY(osd.hasKey("TextOnly"));
    QVERIFY(osd.hasKey("PopupHideDelay"));
    QVERIFY(osd.readEntry("TextOnly", false));
    QVERIFY(osd.readEntry("PopupHideDelay", 1000) == 2000);
    KConfigGroup plugins = config.group("Plugins");
    QVERIFY(plugins.hasKey("desktopchangeosdEnabled"));
    QVERIFY(plugins.readEntry("desktopchangeosdEnabled", false));
}

void TestUpdateKWin49::testPopupInfoAlreadyMigrated()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup popupInfo = config.group("PopupInfo");
    popupInfo.writeEntry("ShowPopup", true);
    popupInfo.writeEntry("TextOnly", false);
    popupInfo.writeEntry("PopupHideDelay", 2000);
    KConfigGroup plugins = config.group("Plugins");
    plugins.writeEntry("desktopchangeosdEnabled", false);
    KConfigGroup osd = config.group("Script-desktopchangeosd");
    osd.writeEntry("TextOnly", true);
    osd.writeEntry("PopupHideDelay", 200);
    migrateDesktopChangeOSD(config);
    QVERIFY(!config.hasGroup("PopupInfo"));
    QVERIFY(!plugins.readEntry("desktopchangeosdEnabled", false));
    QVERIFY(osd.readEntry("TextOnly", false));
    QVERIFY(osd.readEntry("PopupHideDelay", 1000) == 200);
}

void TestUpdateKWin49::testEmptyKConfigTabBox()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    QVERIFY(!config.hasGroup("TabBox"));
    migrateTabBoxConfig(config.group("TabBox"));
    QVERIFY(!config.hasGroup("TabBox"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
    migrateTabBoxConfig(config.group("TabBoxAlternative"));
    QVERIFY(!config.hasGroup("TabBoxAlternative"));
}

void TestUpdateKWin49::testTabBoxShowDesktopDisabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ShowDesktop", false);
    tabBoxAlternative.writeEntry("ShowDesktop", false);
    QVERIFY(!tabBox.hasKey("ShowDesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("ShowDesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ShowDesktop"));
    QVERIFY(tabBox.hasKey("ShowDesktopMode"));
    QVERIFY(tabBox.readEntry("ShowDesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultShowDesktopMode())) == KWin::TabBox::TabBoxConfig::DoNotShowDesktopClient);
    QVERIFY(tabBoxAlternative.hasKey("ShowDesktop"));
    QVERIFY(!tabBoxAlternative.hasKey("ShowDesktopMode"));
}

void TestUpdateKWin49::testTabBoxShowDesktopEnabled()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ShowDesktop", true);
    tabBoxAlternative.writeEntry("ShowDesktop", true);
    QVERIFY(!tabBox.hasKey("ShowDesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("ShowDesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ShowDesktop"));
    QVERIFY(tabBox.hasKey("ShowDesktopMode"));
    QVERIFY(tabBox.readEntry("ShowDesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultShowDesktopMode())) == KWin::TabBox::TabBoxConfig::ShowDesktopClient);
    QVERIFY(tabBoxAlternative.hasKey("ShowDesktop"));
    QVERIFY(!tabBoxAlternative.hasKey("ShowDesktopMode"));
}

void TestUpdateKWin49::testTabBoxAllDesktopsApplicationList()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ListMode", 3);
    tabBoxAlternative.writeEntry("ListMode", 3);
    QVERIFY(!tabBox.hasKey("DesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ListMode"));
    QVERIFY(tabBox.hasKey("DesktopMode"));
    QVERIFY(tabBox.readEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultDesktopMode())) == KWin::TabBox::TabBoxConfig::AllDesktopsClients);
    QVERIFY(tabBoxAlternative.hasKey("ListMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
}

void TestUpdateKWin49::testTabBoxAllDesktopsClientList()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ListMode", 1);
    tabBoxAlternative.writeEntry("ListMode", 1);
    QVERIFY(!tabBox.hasKey("DesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ListMode"));
    QVERIFY(tabBox.hasKey("DesktopMode"));
    QVERIFY(tabBox.readEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultDesktopMode())) == KWin::TabBox::TabBoxConfig::AllDesktopsClients);
    QVERIFY(tabBoxAlternative.hasKey("ListMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
}

void TestUpdateKWin49::testTabBoxCurrentDesktopApplicationList()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ListMode", 2);
    tabBoxAlternative.writeEntry("ListMode", 2);
    QVERIFY(!tabBox.hasKey("DesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ListMode"));
    QVERIFY(tabBox.hasKey("DesktopMode"));
    QVERIFY(tabBox.readEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultDesktopMode())) == KWin::TabBox::TabBoxConfig::OnlyCurrentDesktopClients);
    QVERIFY(tabBoxAlternative.hasKey("ListMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
}

void TestUpdateKWin49::testTabBoxCurrentDesktopClientList()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    KConfigGroup tabBoxAlternative = config.group("TabBoxAlternative");
    tabBox.writeEntry("ListMode", 0);
    tabBoxAlternative.writeEntry("ListMode", 0);
    QVERIFY(!tabBox.hasKey("DesktopMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ListMode"));
    QVERIFY(tabBox.hasKey("DesktopMode"));
    QVERIFY(tabBox.readEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultDesktopMode())) == KWin::TabBox::TabBoxConfig::OnlyCurrentDesktopClients);
    QVERIFY(tabBoxAlternative.hasKey("ListMode"));
    QVERIFY(!tabBoxAlternative.hasKey("DesktopMode"));
}

void TestUpdateKWin49::testTabBoxAlreadyMigrated()
{
    KConfig config(QString(), KConfig::SimpleConfig);
    KConfigGroup tabBox = config.group("TabBox");
    tabBox.writeEntry("ListMode", 0);
    tabBox.writeEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::AllDesktopsClients));
    tabBox.writeEntry("ShowDesktop", true);
    tabBox.writeEntry("ShowDesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::DoNotShowDesktopClient));
    migrateTabBoxConfig(tabBox);
    QVERIFY(!tabBox.hasKey("ListMode"));
    QVERIFY(!tabBox.hasKey("ShowDesktop"));
    QVERIFY(tabBox.readEntry("DesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultDesktopMode())) == KWin::TabBox::TabBoxConfig::AllDesktopsClients);
    QVERIFY(tabBox.readEntry("ShowDesktopMode", static_cast<int>(KWin::TabBox::TabBoxConfig::defaultShowDesktopMode())) == KWin::TabBox::TabBoxConfig::DoNotShowDesktopClient);
}

QTEST_MAIN(TestUpdateKWin49)
#include "test_update_kwin_49.moc"
