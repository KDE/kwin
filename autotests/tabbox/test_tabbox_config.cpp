/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tabbox/tabboxconfig.h"
#include <QtTest>
using namespace KWin;
using namespace KWin::TabBox;

class TestTabBoxConfig : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDefaultCtor();
    void testAssignmentOperator();
};

void TestTabBoxConfig::testDefaultCtor()
{
    TabBoxConfig config;
    QCOMPARE(config.isShowTabBox(), TabBoxConfig::defaultShowTabBox());
    QCOMPARE(config.isHighlightWindows(), TabBoxConfig::defaultHighlightWindow());
    QCOMPARE(config.tabBoxMode(), TabBoxConfig::ClientTabBox);
    QCOMPARE(config.clientDesktopMode(), TabBoxConfig::defaultDesktopMode());
    QCOMPARE(config.clientActivitiesMode(), TabBoxConfig::defaultActivitiesMode());
    QCOMPARE(config.clientApplicationsMode(), TabBoxConfig::defaultApplicationsMode());
    QCOMPARE(config.orderMinimizedMode(), TabBoxConfig::defaultOrderMinimizedMode());
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
    config.setTabBoxMode(TabBoxConfig::DesktopTabBox);
    config.setClientDesktopMode(TabBoxConfig::AllDesktopsClients);
    config.setClientActivitiesMode(TabBoxConfig::AllActivitiesClients);
    config.setClientApplicationsMode(TabBoxConfig::OneWindowPerApplication);
    config.setOrderMinimizedMode(TabBoxConfig::GroupByMinimized);
    config.setClientMinimizedMode(TabBoxConfig::ExcludeMinimizedClients);
    config.setShowDesktopMode(TabBoxConfig::ShowDesktopClient);
    config.setClientMultiScreenMode(TabBoxConfig::ExcludeCurrentScreenClients);
    config.setClientSwitchingMode(TabBoxConfig::StackingOrderSwitching);
    config.setDesktopSwitchingMode(TabBoxConfig::StaticDesktopSwitching);
    config.setLayoutName(QStringLiteral("grid"));
    TabBoxConfig config2;
    config2 = config;
    // verify the config2 values
    QCOMPARE(config2.isShowTabBox(), !TabBoxConfig::defaultShowTabBox());
    QCOMPARE(config2.isHighlightWindows(), !TabBoxConfig::defaultHighlightWindow());
    QCOMPARE(config2.tabBoxMode(), TabBoxConfig::DesktopTabBox);
    QCOMPARE(config2.clientDesktopMode(), TabBoxConfig::AllDesktopsClients);
    QCOMPARE(config2.clientActivitiesMode(), TabBoxConfig::AllActivitiesClients);
    QCOMPARE(config2.clientApplicationsMode(), TabBoxConfig::OneWindowPerApplication);
    QCOMPARE(config2.orderMinimizedMode(), TabBoxConfig::GroupByMinimized);
    QCOMPARE(config2.clientMinimizedMode(), TabBoxConfig::ExcludeMinimizedClients);
    QCOMPARE(config2.showDesktopMode(), TabBoxConfig::ShowDesktopClient);
    QCOMPARE(config2.clientMultiScreenMode(), TabBoxConfig::ExcludeCurrentScreenClients);
    QCOMPARE(config2.clientSwitchingMode(), TabBoxConfig::StackingOrderSwitching);
    QCOMPARE(config2.desktopSwitchingMode(), TabBoxConfig::StaticDesktopSwitching);
    QCOMPARE(config2.layoutName(), QStringLiteral("grid"));
}

QTEST_MAIN(TestTabBoxConfig)

#include "test_tabbox_config.moc"
