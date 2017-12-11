/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2017 Roman Gilg <subdiff@gmail.com>

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
#include "kwin_wayland_test.h"

#include "platform.h"
#include "wayland_server.h"
#include "colorcorrection/manager.h"
#include "colorcorrection/constants.h"

#include <KConfigGroup>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_colorcorrect_nightcolor-0");

class ColorCorrectNightColorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testConfigRead_data();
    void testConfigRead();
    void testChangeConfiguration_data();
    void testChangeConfiguration();
    void testAutoLocationUpdate();
};

void ColorCorrectNightColorTest::initTestCase()
{
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
}

void ColorCorrectNightColorTest::init()
{
}

void ColorCorrectNightColorTest::cleanup()
{
}

void ColorCorrectNightColorTest::testConfigRead_data()
{
    QTest::addColumn<QString>("active");
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("nightTemperature");
    QTest::addColumn<double>("latitudeFixed");
    QTest::addColumn<double>("longitudeFixed");
    QTest::addColumn<QString>("morningBeginFixed");
    QTest::addColumn<QString>("eveningBeginFixed");
    QTest::addColumn<int>("transitionTime");
    QTest::addColumn<bool>("success");

    QTest::newRow("activeMode0") << "true" << 0 << 4500 << 45.5 << 35.1 << "0600" << "1800" << 30 << true;
    QTest::newRow("activeMode1") << "true" << 1 << 2500 << -10.5 << -8. << "0020" << "2000" << 60 << true;
    QTest::newRow("notActiveMode2") << "false" << 2 << 5000 << 90. << -180. << "0600" << "1800" << 1 << true;
    QTest::newRow("wrongData1") << "fa" << 3 << 7000 << 91. << -181. << "060" << "800" << 999999 << false;
    QTest::newRow("wrongData2") << "fa" << 3 << 7000 << 91. << -181. << "060" << "800" << -2 << false;
}

void ColorCorrectNightColorTest::testConfigRead()
{
    QFETCH(QString, active);
    QFETCH(int, mode);
    QFETCH(int, nightTemperature);
    QFETCH(double, latitudeFixed);
    QFETCH(double, longitudeFixed);
    QFETCH(QString, morningBeginFixed);
    QFETCH(QString, eveningBeginFixed);
    QFETCH(int, transitionTime);
    QFETCH(bool, success);

    const bool activeDefault = true;
    const int modeDefault = 0;
    const int nightTemperatureUpperEnd = ColorCorrect::NEUTRAL_TEMPERATURE;
    const double latitudeFixedDefault = 0;
    const double longitudeFixedDefault = 0;
    const QTime morningBeginFixedDefault = QTime(6,0,0);
    const QTime eveningBeginFixedDefault = QTime(18,0,0);
    const int transitionTimeDefault = 30;

    KConfigGroup cfgGroup = kwinApp()->config()->group("NightColor");

    cfgGroup.writeEntry("Active", activeDefault);
    cfgGroup.writeEntry("Mode", modeDefault);
    cfgGroup.writeEntry("NightTemperature", nightTemperatureUpperEnd);
    cfgGroup.writeEntry("LatitudeFixed", latitudeFixedDefault);
    cfgGroup.writeEntry("LongitudeFixed", longitudeFixedDefault);
    cfgGroup.writeEntry("MorningBeginFixed", morningBeginFixedDefault.toString("hhmm"));
    cfgGroup.writeEntry("EveningBeginFixed", eveningBeginFixedDefault.toString("hhmm"));
    cfgGroup.writeEntry("TransitionTime", transitionTimeDefault);

    ColorCorrect::Manager *manager = kwinApp()->platform()->colorCorrectManager();
    manager->reparseConfigAndReset();
    auto info = manager->info();
    QVERIFY(!info.isEmpty());

    QCOMPARE(info.value("Active").toBool(), activeDefault);
    QCOMPARE(info.value("Mode").toInt(), modeDefault);
    QCOMPARE(info.value("NightTemperature").toInt(), nightTemperatureUpperEnd);
    QCOMPARE(info.value("LatitudeFixed").toDouble(), latitudeFixedDefault);
    QCOMPARE(info.value("LongitudeFixed").toDouble(), longitudeFixedDefault);
    QCOMPARE(QTime::fromString(info.value("MorningBeginFixed").toString(), Qt::ISODate), morningBeginFixedDefault);
    QCOMPARE(QTime::fromString(info.value("EveningBeginFixed").toString(), Qt::ISODate), eveningBeginFixedDefault);
    QCOMPARE(info.value("TransitionTime").toInt(), transitionTimeDefault);

    cfgGroup.writeEntry("Active", active);
    cfgGroup.writeEntry("Mode", mode);
    cfgGroup.writeEntry("NightTemperature", nightTemperature);
    cfgGroup.writeEntry("LatitudeFixed", latitudeFixed);
    cfgGroup.writeEntry("LongitudeFixed", longitudeFixed);
    cfgGroup.writeEntry("MorningBeginFixed", morningBeginFixed);
    cfgGroup.writeEntry("EveningBeginFixed", eveningBeginFixed);
    cfgGroup.writeEntry("TransitionTime", transitionTime);

    manager->reparseConfigAndReset();
    info = manager->info();
    QVERIFY(!info.isEmpty());

    if (success) {
        QCOMPARE(info.value("Active").toBool() ? QString("true") : QString("false"), active);
        QCOMPARE(info.value("Mode").toInt(), mode);
        QCOMPARE(info.value("NightTemperature").toInt(), nightTemperature);
        QCOMPARE(info.value("LatitudeFixed").toDouble(), latitudeFixed);
        QCOMPARE(info.value("LongitudeFixed").toDouble(), longitudeFixed);
        QCOMPARE(QTime::fromString(info.value("MorningBeginFixed").toString(), Qt::ISODate), QTime::fromString(morningBeginFixed, "hhmm"));
        QCOMPARE(QTime::fromString(info.value("EveningBeginFixed").toString(), Qt::ISODate), QTime::fromString(eveningBeginFixed, "hhmm"));
        QCOMPARE(info.value("TransitionTime").toInt(), transitionTime);
    } else {
        QCOMPARE(info.value("Active").toBool(), activeDefault);
        QCOMPARE(info.value("Mode").toInt(), modeDefault);
        QCOMPARE(info.value("NightTemperature").toInt(), nightTemperatureUpperEnd);
        QCOMPARE(info.value("LatitudeFixed").toDouble(), latitudeFixedDefault);
        QCOMPARE(info.value("LongitudeFixed").toDouble(), longitudeFixedDefault);
        QCOMPARE(QTime::fromString(info.value("MorningBeginFixed").toString(), Qt::ISODate), morningBeginFixedDefault);
        QCOMPARE(QTime::fromString(info.value("EveningBeginFixed").toString(), Qt::ISODate), eveningBeginFixedDefault);
        QCOMPARE(info.value("TransitionTime").toInt(), transitionTimeDefault);
    }
}

void ColorCorrectNightColorTest::testChangeConfiguration_data()
{
    QTest::addColumn<bool>("activeReadIn");
    QTest::addColumn<int>("modeReadIn");
    QTest::addColumn<int>("nightTemperatureReadIn");
    QTest::addColumn<double>("latitudeFixedReadIn");
    QTest::addColumn<double>("longitudeFixedReadIn");
    QTest::addColumn<QTime>("morBeginFixedReadIn");
    QTest::addColumn<QTime>("eveBeginFixedReadIn");
    QTest::addColumn<int>("transitionTimeReadIn");
    QTest::addColumn<bool>("successReadIn");

    QTest::newRow("data0") << true << 0 << 4500 << 45.5 << 35.1 << QTime(6,0,0) << QTime(18,0,0) << 30 << true;
    QTest::newRow("data1") << true << 1 << 2500 << -10.5 << -8. << QTime(0,2,0) << QTime(20,0,0) << 60 << true;
    QTest::newRow("data2") << false << 2 << 5000 << 90. << -180. << QTime(6,0,0) << QTime(19,1,1) << 1 << true;
    QTest::newRow("wrongData0") << true << 3 << 4500 << 0. << 0. << QTime(6,0,0) << QTime(18,0,0) << 30 << false;
    QTest::newRow("wrongData1") << true << 0 << 500 << 0. << 0. << QTime(6,0,0) << QTime(18,0,0) << 30 << false;
    QTest::newRow("wrongData2") << true << 0 << 7000 << 0. << 0. << QTime(6,0,0) << QTime(18,0,0) << 30 << false;
    QTest::newRow("wrongData3") << true << 0 << 4500 << 91. << -181. << QTime(6,0,0) << QTime(18,0,0) << 30 << false;
    QTest::newRow("wrongData4") << true << 0 << 4500 << 0. << 0. << QTime(18,0,0) << QTime(6,0,0) << 30 << false;
    QTest::newRow("wrongData5") << true << 0 << 4500 << 0. << 0. << QTime(6,0,0) << QTime(18,0,0) << 0 << false;
    QTest::newRow("wrongData6") << true << 0 << 4500 << 0. << 0. << QTime(6,0,0) << QTime(18,0,0) << -1 << false;
    QTest::newRow("wrongData7") << true << 0 << 4500 << 0. << 0. << QTime(12,0,0) << QTime(12,30,0) << 30 << false;
    QTest::newRow("wrongData8") << true << 0 << 4500 << 0. << 0. << QTime(1,0,0) << QTime(23,30,0) << 90 << false;
}

void ColorCorrectNightColorTest::testChangeConfiguration()
{
    QFETCH(bool, activeReadIn);
    QFETCH(int, modeReadIn);
    QFETCH(int, nightTemperatureReadIn);
    QFETCH(double, latitudeFixedReadIn);
    QFETCH(double, longitudeFixedReadIn);
    QFETCH(QTime, morBeginFixedReadIn);
    QFETCH(QTime, eveBeginFixedReadIn);
    QFETCH(int, transitionTimeReadIn);
    QFETCH(bool, successReadIn);

    const bool activeDefault = true;
    const int modeDefault = 0;
    const int nightTemperatureDefault = ColorCorrect::DEFAULT_NIGHT_TEMPERATURE;
    const double latitudeFixedDefault = 0;
    const double longitudeFixedDefault = 0;
    const QTime morningBeginFixedDefault = QTime(6,0,0);
    const QTime eveningBeginFixedDefault = QTime(18,0,0);
    const int transitionTimeDefault = 30;

    // init with default values
    bool active = activeDefault;
    int mode = modeDefault;
    int nightTemperature = nightTemperatureDefault;
    double latitudeFixed = latitudeFixedDefault;
    double longitudeFixed = longitudeFixedDefault;
    QTime morningBeginFixed = morningBeginFixedDefault;
    QTime eveningBeginFixed = eveningBeginFixedDefault;
    int transitionTime = transitionTimeDefault;

    bool activeExpect = activeDefault;
    int modeExpect = modeDefault;
    int nightTemperatureExpect = nightTemperatureDefault;
    double latitudeFixedExpect = latitudeFixedDefault;
    double longitudeFixedExpect = longitudeFixedDefault;
    QTime morningBeginFixedExpect = morningBeginFixedDefault;
    QTime eveningBeginFixedExpect = eveningBeginFixedDefault;
    int transitionTimeExpect = transitionTimeDefault;

    QHash<QString, QVariant> data;

    auto setData = [&active, &mode, &nightTemperature,
            &latitudeFixed, &longitudeFixed,
            &morningBeginFixed, &eveningBeginFixed, &transitionTime,
            &data]() {
        data["Active"] = active;
        data["Mode"] = mode;
        data["NightTemperature"] = nightTemperature;

        data["LatitudeFixed"] = latitudeFixed;
        data["LongitudeFixed"] = longitudeFixed;

        data["MorningBeginFixed"] = morningBeginFixed.toString(Qt::ISODate);
        data["EveningBeginFixed"] = eveningBeginFixed.toString(Qt::ISODate);
        data["TransitionTime"] = transitionTime;
    };

    auto compareValues = [&activeExpect, &modeExpect, &nightTemperatureExpect,
            &latitudeFixedExpect, &longitudeFixedExpect,
            &morningBeginFixedExpect, &eveningBeginFixedExpect,
            &transitionTimeExpect](QHash<QString, QVariant> info) {
        QCOMPARE(info.value("Active").toBool(), activeExpect);
        QCOMPARE(info.value("Mode").toInt(), modeExpect);
        QCOMPARE(info.value("NightTemperature").toInt(), nightTemperatureExpect);
        QCOMPARE(info.value("LatitudeFixed").toDouble(), latitudeFixedExpect);
        QCOMPARE(info.value("LongitudeFixed").toDouble(), longitudeFixedExpect);
        QCOMPARE(info.value("MorningBeginFixed").toString(), morningBeginFixedExpect.toString(Qt::ISODate));
        QCOMPARE(info.value("EveningBeginFixed").toString(), eveningBeginFixedExpect.toString(Qt::ISODate));
        QCOMPARE(info.value("TransitionTime").toInt(), transitionTimeExpect);
    };

    ColorCorrect::Manager *manager = kwinApp()->platform()->colorCorrectManager();

    // test with default values
    setData();
    manager->changeConfiguration(data);
    compareValues(manager->info());

    // set to test values
    active = activeReadIn;
    mode = modeReadIn;
    nightTemperature = nightTemperatureReadIn;
    latitudeFixed = latitudeFixedReadIn;
    longitudeFixed = longitudeFixedReadIn;
    morningBeginFixed = morBeginFixedReadIn;
    eveningBeginFixed = eveBeginFixedReadIn;
    transitionTime = transitionTimeReadIn;

    if (successReadIn) {
        activeExpect = activeReadIn;
        modeExpect = modeReadIn;
        nightTemperatureExpect = nightTemperatureReadIn;
        latitudeFixedExpect = latitudeFixedReadIn;
        longitudeFixedExpect = longitudeFixedReadIn;
        morningBeginFixedExpect = morBeginFixedReadIn;
        eveningBeginFixedExpect = eveBeginFixedReadIn;
        transitionTimeExpect = transitionTimeReadIn;
    }

    // test with test values
    setData();
    QCOMPARE(manager->changeConfiguration(data), successReadIn);
    compareValues(manager->info());
}

void ColorCorrectNightColorTest::testAutoLocationUpdate()
{
    ColorCorrect::Manager *manager = kwinApp()->platform()->colorCorrectManager();
    auto info = manager->info();
    QCOMPARE(info.value("LatitudeAuto").toDouble(), 0.);
    QCOMPARE(info.value("LongitudeAuto").toDouble(), 0.);

    // wrong latitude value
    manager->autoLocationUpdate(91, 15);
    info = manager->info();
    QCOMPARE(info.value("LatitudeAuto").toDouble(), 0.);
    QCOMPARE(info.value("LongitudeAuto").toDouble(), 0.);

    // wrong longitude value
    manager->autoLocationUpdate(50, -181);
    info = manager->info();
    QCOMPARE(info.value("LatitudeAuto").toDouble(), 0.);
    QCOMPARE(info.value("LongitudeAuto").toDouble(), 0.);

    // change
    manager->autoLocationUpdate(50, -180);
    info = manager->info();
    QCOMPARE(info.value("LatitudeAuto").toDouble(), 50.);
    QCOMPARE(info.value("LongitudeAuto").toDouble(), -180.);

    // small deviation only
    manager->autoLocationUpdate(51.5, -179.5);
    info = manager->info();
    QCOMPARE(info.value("LongitudeAuto").toDouble(), -180.);
    QCOMPARE(info.value("LatitudeAuto").toDouble(), 50.);
}

WAYLANDTEST_MAIN(ColorCorrectNightColorTest)
#include "colorcorrect_nightcolor_test.moc"
