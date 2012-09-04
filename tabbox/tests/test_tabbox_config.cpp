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
#include "../tabboxconfig.h"
#include <QtTest/QtTest>
using namespace KWin;
using namespace KWin::TabBox;

class TestTabBoxConfig : public QObject
{
    Q_OBJECT
private slots:
    void testDefaultCtor();
    void testAssignmentOperator();
};

void TestTabBoxConfig::testDefaultCtor()
{
    TabBoxConfig config;
    QCOMPARE(config.isShowTabBox(), TabBoxConfig::defaultShowTabBox());
    QCOMPARE(config.isHighlightWindows(), TabBoxConfig::defaultHighlightWindow());
    QCOMPARE(config.isShowOutline(), TabBoxConfig::defaultShowOutline());
    QCOMPARE(config.tabBoxMode(), TabBoxConfig::ClientTabBox);
    QCOMPARE(config.clientDesktopMode(), TabBoxConfig::defaultDesktopMode());
    QCOMPARE(config.clientActivitiesMode(), TabBoxConfig::defaultActivitiesMode());
    QCOMPARE(config.clientApplicationsMode(), TabBoxConfig::defaultApplicationsMode());
    QCOMPARE(config.clientMinimizedMode(), TabBoxConfig::defaultMinimizedMode());
    QCOMPARE(config.showDesktopMode(), TabBoxConfig::defaultShowDesktopMode());
    QCOMPARE(config.clientMultiScreenMode(), TabBoxConfig::defaultMultiScreenMode());
    QCOMPARE(config.clientSwitchingMode(), TabBoxConfig::defaultSwitchingMode());
    QCOMPARE(config.desktopSwitchingMode(), TabBoxConfig::MostRecentlyUsedDesktopSwitching);
    QCOMPARE(config.layoutName(), TabBoxConfig::defaultLayoutName());
}

void TestTabBoxConfig::testAssignmentOperator()
{
    TabBoxConfig config;
    // changing all values of the config object
    config.setShowTabBox(!TabBoxConfig::defaultShowTabBox());
    config.setHighlightWindows(!TabBoxConfig::defaultHighlightWindow());
    config.setShowOutline(!TabBoxConfig::defaultShowOutline());
    config.setTabBoxMode(TabBoxConfig::DesktopTabBox);
    config.setClientDesktopMode(TabBoxConfig::AllDesktopsClients);
    config.setClientActivitiesMode(TabBoxConfig::AllActivitiesClients);
    config.setClientApplicationsMode(TabBoxConfig::OneWindowPerApplication);
    config.setClientMinimizedMode(TabBoxConfig::ExcludeMinimizedClients);
    config.setShowDesktopMode(TabBoxConfig::ShowDesktopClient);
    config.setClientMultiScreenMode(TabBoxConfig::ExcludeCurrentScreenClients);
    config.setClientSwitchingMode(TabBoxConfig::StackingOrderSwitching);
    config.setDesktopSwitchingMode(TabBoxConfig::StaticDesktopSwitching);
    config.setLayoutName(QString("grid"));
    TabBoxConfig config2;
    config2 = config;
    // verify the config2 values
    QCOMPARE(config2.isShowTabBox(), !TabBoxConfig::defaultShowTabBox());
    QCOMPARE(config2.isHighlightWindows(), !TabBoxConfig::defaultHighlightWindow());
    QCOMPARE(config2.isShowOutline(), !TabBoxConfig::defaultShowOutline());
    QCOMPARE(config2.tabBoxMode(), TabBoxConfig::DesktopTabBox);
    QCOMPARE(config2.clientDesktopMode(), TabBoxConfig::AllDesktopsClients);
    QCOMPARE(config2.clientActivitiesMode(), TabBoxConfig::AllActivitiesClients);
    QCOMPARE(config2.clientApplicationsMode(), TabBoxConfig::OneWindowPerApplication);
    QCOMPARE(config2.clientMinimizedMode(), TabBoxConfig::ExcludeMinimizedClients);
    QCOMPARE(config2.showDesktopMode(), TabBoxConfig::ShowDesktopClient);
    QCOMPARE(config2.clientMultiScreenMode(), TabBoxConfig::ExcludeCurrentScreenClients);
    QCOMPARE(config2.clientSwitchingMode(), TabBoxConfig::StackingOrderSwitching);
    QCOMPARE(config2.desktopSwitchingMode(), TabBoxConfig::StaticDesktopSwitching);
    QCOMPARE(config2.layoutName(), QString("grid"));
}

QTEST_MAIN(TestTabBoxConfig)

#include "test_tabbox_config.moc"
