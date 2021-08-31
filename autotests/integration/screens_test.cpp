/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "abstract_output.h"
#include "cursor.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/surface.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_screens-0");

class ScreensTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSize_data();
    void testSize();
    void testCount();
    void testIntersecting_data();
    void testIntersecting();
    void testCurrent_data();
    void testCurrent();
    void testCurrentWithFollowsMouse_data();
    void testCurrentWithFollowsMouse();
    void testCurrentPoint_data();
    void testCurrentPoint();
};

void ScreensTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
}

void ScreensTest::init()
{
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));

    QVERIFY(Test::setupWaylandConnection());
}

static void purge(KConfig *config)
{
    const QStringList groups = config->groupList();
    for (const QString &group : groups) {
        config->deleteGroup(group);
    }
}

void ScreensTest::cleanup()
{
    // Destroy the wayland connection of the test client.
    Test::destroyWaylandConnection();

    // Wipe the screens config clean.
    auto config = kwinApp()->config();
    purge(config.data());
    config->sync();
    workspace()->slotReconfigure();

    // Reset the screen layout of the test environment.
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
}

void ScreensTest::testSize_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QSize>("expectedSize");

    QTest::newRow("empty") << QVector<QRect>{{QRect()}} << QSize(0, 0);
    QTest::newRow("cloned") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QSize(200, 100);
    QTest::newRow("adjacent") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QSize(600, 400);
    QTest::newRow("overlapping") << QVector<QRect>{{QRect{-10, -20, 50, 100}, QRect{0, 0, 100, 200}}} << QSize(110, 220);
    QTest::newRow("gap") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QSize(30, 60);
}

void ScreensTest::testSize()
{
    QSignalSpy sizeChangedSpy(screens(), &Screens::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection,
                              Q_ARG(int, geometries.count()), Q_ARG(QVector<QRect>, geometries));

    QVERIFY(sizeChangedSpy.wait());
    QTEST(screens()->size(), "expectedSize");
}

void ScreensTest::testCount()
{
    QSignalSpy countChangedSpy(screens(), &Screens::countChanged);
    QVERIFY(countChangedSpy.isValid());

    // the test environments has two outputs
    QCOMPARE(screens()->count(), 2);

    // change to one screen
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection, Q_ARG(int, 1));
    QVERIFY(countChangedSpy.wait());
    QCOMPARE(countChangedSpy.count(), 1);
    QCOMPARE(screens()->count(), 1);

    // setting the same geometries shouldn't emit the signal, but we should get a changed signal
    QSignalSpy changedSpy(screens(), &Screens::changed);
    QVERIFY(changedSpy.isValid());
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection, Q_ARG(int, 1));
    QVERIFY(changedSpy.wait());
    QCOMPARE(countChangedSpy.count(), 1);
}

void ScreensTest::testIntersecting_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QRect>("testGeometry");
    QTest::addColumn<int>("expectedCount");

    QTest::newRow("null-rect") << QVector<QRect>{{QRect{0, 0, 100, 100}}} << QRect() << 0;
    QTest::newRow("non-overlapping") << QVector<QRect>{{QRect{0, 0, 100, 100}}} << QRect(100, 0, 100, 100) << 0;
    QTest::newRow("in-between") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QRect(15, 0, 2, 2) << 0;
    QTest::newRow("gap-overlapping") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QRect(9, 10, 200, 200) << 2;
    QTest::newRow("larger") << QVector<QRect>{{QRect{0, 0, 100, 100}}} << QRect(-10, -10, 200, 200) << 1;
    QTest::newRow("several") << QVector<QRect>{{QRect{0, 0, 100, 100}, QRect{100, 0, 100, 100}, QRect{200, 100, 100, 100}, QRect{300, 100, 100, 100}}} << QRect(0, 0, 300, 300) << 3;
}

void ScreensTest::testIntersecting()
{
    QSignalSpy changedSpy(screens(), &Screens::changed);
    QVERIFY(changedSpy.isValid());

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection,
                              Q_ARG(int, geometries.count()), Q_ARG(QVector<QRect>, geometries));
    QVERIFY(changedSpy.wait());

    QFETCH(QRect, testGeometry);
    QCOMPARE(screens()->count(), geometries.count());
    QTEST(screens()->intersecting(testGeometry), "expectedCount");
}

void ScreensTest::testCurrent_data()
{
    QTest::addColumn<int>("currentId");

    QTest::newRow("first") << 0;
    QTest::newRow("second") << 1;
}

void ScreensTest::testCurrent()
{
    QFETCH(int, currentId);
    AbstractOutput *output = kwinApp()->platform()->findOutput(currentId);

    // Disable "active screen follows mouse"
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("ActiveMouseScreen", false);
    group.sync();
    workspace()->slotReconfigure();

    workspace()->setActiveOutput(output);
    QCOMPARE(workspace()->activeOutput(), output);
}

void ScreensTest::testCurrentWithFollowsMouse_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expectedId");

    QTest::newRow("empty") << QVector<QRect>{{QRect()}} << QPoint(100, 100) << 0;
    QTest::newRow("cloned") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void ScreensTest::testCurrentWithFollowsMouse()
{
    QSignalSpy changedSpy(screens(), &Screens::changed);
    QVERIFY(changedSpy.isValid());

    // Enable "active screen follows mouse"
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("ActiveMouseScreen", true);
    group.sync();
    workspace()->slotReconfigure();

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection,
                              Q_ARG(int, geometries.count()), Q_ARG(QVector<QRect>, geometries));
    QVERIFY(changedSpy.wait());

    QFETCH(QPoint, cursorPos);
    KWin::Cursors::self()->mouse()->setPos(cursorPos);

    QFETCH(int, expectedId);
    AbstractOutput *expected = kwinApp()->platform()->findOutput(expectedId);
    QCOMPARE(workspace()->activeOutput(), expected);
}

void ScreensTest::testCurrentPoint_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expectedId");

    QTest::newRow("empty") << QVector<QRect>{{QRect()}} << QPoint(100, 100) << 0;
    QTest::newRow("cloned") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void ScreensTest::testCurrentPoint()
{
    QSignalSpy changedSpy(screens(), &KWin::Screens::changed);
    QVERIFY(changedSpy.isValid());

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection,
                              Q_ARG(int, geometries.count()), Q_ARG(QVector<QRect>, geometries));
    QVERIFY(changedSpy.wait());

    // Disable "active screen follows mouse"
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("ActiveMouseScreen", false);
    group.sync();
    workspace()->slotReconfigure();

    QFETCH(QPoint, cursorPos);
    workspace()->setActiveOutput(cursorPos);

    QFETCH(int, expectedId);
    AbstractOutput *expected = kwinApp()->platform()->findOutput(expectedId);
    QCOMPARE(workspace()->activeOutput(), expected);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreensTest)
#include "screens_test.moc"
