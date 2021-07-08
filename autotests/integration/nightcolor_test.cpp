/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "wayland_server.h"

#include "plugins/nightcolor/nightcolormanager.h"
#include "plugins/nightcolor/constants.h"

#include <KConfigGroup>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_nightcolor-0");

class NightColorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testConfigRead_data();
    void testConfigRead();
};

void NightColorTest::initTestCase()
{
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    Test::initWaylandWorkspace();

    NightColorManager *manager = NightColorManager::self();
    QVERIFY(manager);
}

void NightColorTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void NightColorTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void NightColorTest::testConfigRead_data()
{
    QTest::addColumn<bool>("active");
    QTest::addColumn<int>("mode");

    QTest::newRow("activeMode0") << true << 0;
    QTest::newRow("activeMode1") << true << 1;
    QTest::newRow("activeMode2") << true << 3;
    QTest::newRow("notActiveMode2") << false << 2;
    QTest::newRow("wrongData1") << false << 4;
}

void NightColorTest::testConfigRead()
{
    QFETCH(bool, active);
    QFETCH(int, mode);

    const bool activeDefault = true;
    const int modeDefault = 0;

    KConfigGroup cfgGroup = kwinApp()->config()->group("NightColor");

    cfgGroup.writeEntry("Active", activeDefault);
    cfgGroup.writeEntry("Mode", modeDefault);
    kwinApp()->config()->sync();
    NightColorManager *manager = NightColorManager::self();
    manager->reconfigure();

    QCOMPARE(manager->isEnabled(), activeDefault);
    QCOMPARE(manager->mode(), modeDefault);

    cfgGroup.writeEntry("Active", active);
    cfgGroup.writeEntry("Mode", mode);
    kwinApp()->config()->sync();

    manager->reconfigure();

    QCOMPARE(manager->isEnabled(), active);
    if (mode > 3 || mode < 0) {
        QCOMPARE(manager->mode(), 0);
    } else {
        QCOMPARE(manager->mode(), mode);
    }

}

WAYLANDTEST_MAIN(NightColorTest)
#include "nightcolor_test.moc"
