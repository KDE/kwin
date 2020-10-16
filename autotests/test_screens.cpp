/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_workspace.h"
#include "../cursor.h"
#include "mock_screens.h"
#include "mock_x11client.h"
// frameworks
#include <KConfigGroup>
// Qt
#include <QtTest>

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

// Mock

class TestScreens : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void testCurrentFollowsMouse();
    void testReconfigure_data();
    void testReconfigure();
    void testSize_data();
    void testSize();
    void testCount();
    void testIntersecting_data();
    void testIntersecting();
    void testCurrent_data();
    void testCurrent();
    void testCurrentClient();
    void testCurrentWithFollowsMouse_data();
    void testCurrentWithFollowsMouse();
    void testCurrentPoint_data();
    void testCurrentPoint();
};

void TestScreens::init()
{
    KWin::Cursors::self()->setMouse(new KWin::Cursor(this));
}

void TestScreens::testCurrentFollowsMouse()
{
    KWin::MockWorkspace ws;
    KWin::Screens *screens = KWin::Screens::create(&ws);
    QVERIFY(!screens->isCurrentFollowsMouse());
    screens->setCurrentFollowsMouse(true);
    QVERIFY(screens->isCurrentFollowsMouse());
    // setting to same should not do anything
    screens->setCurrentFollowsMouse(true);
    QVERIFY(screens->isCurrentFollowsMouse());

    // setting back to other value
    screens->setCurrentFollowsMouse(false);
    QVERIFY(!screens->isCurrentFollowsMouse());
    // setting to same should not do anything
    screens->setCurrentFollowsMouse(false);
    QVERIFY(!screens->isCurrentFollowsMouse());
}

void TestScreens::testReconfigure_data()
{
    QTest::addColumn<QString>("focusPolicy");
    QTest::addColumn<bool>("expectedDefault");
    QTest::addColumn<bool>("setting");

    QTest::newRow("ClickToFocus")            << QStringLiteral("ClickToFocus")            << false << true;
    QTest::newRow("FocusFollowsMouse")       << QStringLiteral("FocusFollowsMouse")       << true  << false;
    QTest::newRow("FocusUnderMouse")         << QStringLiteral("FocusUnderMouse")         << true  << false;
    QTest::newRow("FocusStrictlyUnderMouse") << QStringLiteral("FocusStrictlyUnderMouse") << true  << false;
}

void TestScreens::testReconfigure()
{
    using namespace KWin;
    MockWorkspace ws;
    Screens::create(&ws);
    screens()->reconfigure();
    QVERIFY(!screens()->isCurrentFollowsMouse());

    QFETCH(QString, focusPolicy);

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("FocusPolicy", focusPolicy);
    config->group("Windows").sync();
    config->sync();

    screens()->setConfig(config);
    screens()->reconfigure();
    QTEST(screens()->isCurrentFollowsMouse(), "expectedDefault");

    QFETCH(bool, setting);
    config->group("Windows").writeEntry("ActiveMouseScreen", setting);
    config->sync();
    screens()->reconfigure();
    QCOMPARE(screens()->isCurrentFollowsMouse(), setting);
}

void TestScreens::testSize_data()
{
    QTest::addColumn< QList<QRect> >("geometries");
    QTest::addColumn<QSize>("expectedSize");

    QTest::newRow("empty") << QList<QRect>{{QRect()}} << QSize(0, 0);
    QTest::newRow("cloned") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QSize(200, 100);
    QTest::newRow("adjacent") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QSize(600, 400);
    QTest::newRow("overlapping") << QList<QRect>{{QRect{-10, -20, 50, 100}, QRect{0, 0, 100, 200}}} << QSize(110, 220);
    QTest::newRow("gap") << QList<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QSize(30, 60);
}

void TestScreens::testSize()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy sizeChangedSpy(screens(), &KWin::Screens::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    QCOMPARE(screens()->size(), QSize(100, 100));
    QFETCH(QList<QRect>, geometries);
    QVERIFY(!screens()->isChanging());
    mockScreens->setGeometries(geometries);
    QVERIFY(screens()->isChanging());

    QVERIFY(sizeChangedSpy.wait());
    QVERIFY(!screens()->isChanging());
    QTEST(screens()->size(), "expectedSize");
}

void TestScreens::testCount()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy countChangedSpy(screens(), &KWin::Screens::countChanged);
    QVERIFY(countChangedSpy.isValid());

    QCOMPARE(screens()->count(), 1);

    // change to two screens
    QList<QRect> geometries{{QRect{0, 0, 100, 200}, QRect{100, 0, 100, 200}}};
    mockScreens->setGeometries(geometries);

    QVERIFY(countChangedSpy.wait());
    QCOMPARE(countChangedSpy.count(), 1);
    QCOMPARE(countChangedSpy.first().first().toInt(), 1);
    QCOMPARE(countChangedSpy.first().last().toInt(), 2);
    QCOMPARE(screens()->count(), 2);

    // go back to one screen
    geometries.takeLast();
    mockScreens->setGeometries(geometries);
    QVERIFY(countChangedSpy.wait());
    QCOMPARE(countChangedSpy.count(), 2);
    QCOMPARE(countChangedSpy.last().first().toInt(), 2);
    QCOMPARE(countChangedSpy.last().last().toInt(), 1);
    QCOMPARE(screens()->count(), 1);

    // setting the same geometries shouldn't emit the signal, but we should get a changed signal
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());
    mockScreens->setGeometries(geometries);
    QVERIFY(changedSpy.wait());
    QCOMPARE(countChangedSpy.count(), 2);
}

void TestScreens::testIntersecting_data()
{
    QTest::addColumn<QList<QRect>>("geometries");
    QTest::addColumn<QRect>("testGeometry");
    QTest::addColumn<int>("expectedCount");

    QTest::newRow("null-rect") << QList<QRect>{{QRect{0, 0, 100, 100}}} << QRect() << 0;
    QTest::newRow("non-overlapping") << QList<QRect>{{QRect{0, 0, 100, 100}}} << QRect(100, 0, 100, 100) << 0;
    QTest::newRow("in-between") << QList<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QRect(15, 0, 2, 2) << 0;
    QTest::newRow("gap-overlapping") << QList<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QRect(9, 10, 200, 200) << 2;
    QTest::newRow("larger") << QList<QRect>{{QRect{0, 0, 100, 100}}} << QRect(-10, -10, 200, 200) << 1;
    QTest::newRow("several") << QList<QRect>{{QRect{0, 0, 100, 100}, QRect{100, 0, 100, 100}, QRect{200, 100, 100, 100}, QRect{300, 100, 100, 100}}} << QRect(0, 0, 300, 300) << 3;
}

void TestScreens::testIntersecting()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());
    QFETCH(QList<QRect>, geometries);
    mockScreens->setGeometries(geometries);
    // first is before it's updated
    QVERIFY(changedSpy.wait());
    // second is after it's updated
    QVERIFY(changedSpy.wait());

    QFETCH(QRect, testGeometry);
    QCOMPARE(screens()->count(), geometries.count());
    QTEST(screens()->intersecting(testGeometry), "expectedCount");
}

void TestScreens::testCurrent_data()
{
    QTest::addColumn<int>("current");
    QTest::addColumn<bool>("signal");

    QTest::newRow("unchanged") << 0 << false;
    QTest::newRow("changed") << 1 << true;
}

void TestScreens::testCurrent()
{
    using namespace KWin;
    MockWorkspace ws;
    Screens::create(&ws);
    QSignalSpy currentChangedSpy(screens(), &KWin::Screens::currentChanged);
    QVERIFY(currentChangedSpy.isValid());

    QFETCH(int, current);
    screens()->setCurrent(current);
    QCOMPARE(screens()->current(), current);
    QTEST(!currentChangedSpy.isEmpty(), "signal");
}

void TestScreens::testCurrentClient()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());
    mockScreens->setGeometries(QList<QRect>{{QRect{0, 0, 100, 100}, QRect{100, 0, 100, 100}}});
    // first is before it's updated
    QVERIFY(changedSpy.wait());
    // second is after it's updated
    QVERIFY(changedSpy.wait());

    QSignalSpy currentChangedSpy(screens(), &KWin::Screens::currentChanged);
    QVERIFY(currentChangedSpy.isValid());

    // create a mock client
    X11Client *client = new X11Client(&ws);
    client->setScreen(1);

    // it's not the active client, so changing won't work
    screens()->setCurrent(client);
    QVERIFY(currentChangedSpy.isEmpty());
    QCOMPARE(screens()->current(), 0);

    // making the client active should affect things
    client->setActive(true);
    ws.setActiveClient(client);

    // first of all current should be changed just by the fact that there is an active client
    QCOMPARE(screens()->current(), 1);
    // but also calling setCurrent should emit the changed signal
    screens()->setCurrent(client);
    QCOMPARE(currentChangedSpy.count(), 1);
    QCOMPARE(screens()->current(), 1);

    // setting current with the same client again should not change, though
    screens()->setCurrent(client);
    QCOMPARE(currentChangedSpy.count(), 1);

    // and it should even still be on screen 1 if we make the client non-current again
    ws.setActiveClient(nullptr);
    client->setActive(false);
    QCOMPARE(screens()->current(), 1);
}

void TestScreens::testCurrentWithFollowsMouse_data()
{
    QTest::addColumn< QList<QRect> >("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expected");

    QTest::newRow("empty") << QList<QRect>{{QRect()}} << QPoint(100, 100) << 0;
    QTest::newRow("cloned") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QList<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void TestScreens::testCurrentWithFollowsMouse()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());
    screens()->setCurrentFollowsMouse(true);
    QCOMPARE(screens()->current(), 0);

    QFETCH(QList<QRect>, geometries);
    mockScreens->setGeometries(geometries);
    // first is before it's updated
    QVERIFY(changedSpy.wait());
    // second is after it's updated
    QVERIFY(changedSpy.wait());

    QFETCH(QPoint, cursorPos);
    KWin::Cursors::self()->mouse()->setPos(cursorPos);
    QTEST(screens()->current(), "expected");
}

void TestScreens::testCurrentPoint_data()
{
    QTest::addColumn< QList<QRect> >("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expected");

    QTest::newRow("empty") << QList<QRect>{{QRect()}} << QPoint(100, 100) << 0;
    QTest::newRow("cloned") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QList<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QList<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void TestScreens::testCurrentPoint()
{
    using namespace KWin;
    MockWorkspace ws;
    MockScreens *mockScreens = static_cast<MockScreens*>(Screens::create(&ws));
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());

    QFETCH(QList<QRect>, geometries);
    mockScreens->setGeometries(geometries);
    // first is before it's updated
    QVERIFY(changedSpy.wait());
    // second is after it's updated
    QVERIFY(changedSpy.wait());

    QFETCH(QPoint, cursorPos);
    screens()->setCurrent(cursorPos);
    QTEST(screens()->current(), "expected");
}

QTEST_MAIN(TestScreens)
#include "test_screens.moc"
