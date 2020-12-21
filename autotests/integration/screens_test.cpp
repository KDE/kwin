/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "cursor.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/xdgshell.h>

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

void ScreensTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void ScreensTest::init()
{
    Screens::self()->setCurrent(0);
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
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    purge(config.data());
    config->sync();
    screens()->setConfig(config);
    screens()->reconfigure();

    // Reset the screen layout of the test environment.
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
}

void ScreensTest::testCurrentFollowsMouse()
{
    QVERIFY(!screens()->isCurrentFollowsMouse());
    screens()->setCurrentFollowsMouse(true);
    QVERIFY(screens()->isCurrentFollowsMouse());
    // setting to same should not do anything
    screens()->setCurrentFollowsMouse(true);
    QVERIFY(screens()->isCurrentFollowsMouse());

    // setting back to other value
    screens()->setCurrentFollowsMouse(false);
    QVERIFY(!screens()->isCurrentFollowsMouse());
    // setting to same should not do anything
    screens()->setCurrentFollowsMouse(false);
    QVERIFY(!screens()->isCurrentFollowsMouse());
}

void ScreensTest::testReconfigure_data()
{
    QTest::addColumn<QString>("focusPolicy");
    QTest::addColumn<bool>("expectedDefault");
    QTest::addColumn<bool>("setting");

    QTest::newRow("ClickToFocus")            << QStringLiteral("ClickToFocus")            << false << true;
    QTest::newRow("FocusFollowsMouse")       << QStringLiteral("FocusFollowsMouse")       << true  << false;
    QTest::newRow("FocusUnderMouse")         << QStringLiteral("FocusUnderMouse")         << true  << false;
    QTest::newRow("FocusStrictlyUnderMouse") << QStringLiteral("FocusStrictlyUnderMouse") << true  << false;
}

void ScreensTest::testReconfigure()
{
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
    QTest::addColumn<int>("current");
    QTest::addColumn<bool>("signal");

    QTest::newRow("unchanged") << 0 << false;
    QTest::newRow("changed") << 1 << true;
}

void ScreensTest::testCurrent()
{
    QSignalSpy currentChangedSpy(screens(), &KWin::Screens::currentChanged);
    QVERIFY(currentChangedSpy.isValid());

    QFETCH(int, current);
    screens()->setCurrent(current);
    QCOMPARE(screens()->current(), current);
    QTEST(!currentChangedSpy.isEmpty(), "signal");
}

void ScreensTest::testCurrentClient()
{
    QSignalSpy currentChangedSpy(screens(), &Screens::currentChanged);
    QVERIFY(currentChangedSpy.isValid());

    // create a test window
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<KWayland::Client::XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), QSize(200, 100), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());

    // if the window is sent to another screen, that screen will become current
    client->sendToScreen(1);
    QCOMPARE(currentChangedSpy.count(), 1);
    QCOMPARE(screens()->current(), 1);

    // setting current with the same client again should not change
    screens()->setCurrent(client);
    QCOMPARE(currentChangedSpy.count(), 1);

    // and it should even still be on screen 1 if we make the client non-current again
    workspace()->setActiveClient(nullptr);
    client->setActive(false);
    QCOMPARE(screens()->current(), 1);

    // it's not the active client, so changing won't work
    screens()->setCurrent(client);
    client->sendToScreen(0);
    QCOMPARE(currentChangedSpy.count(), 1);
    QCOMPARE(screens()->current(), 1);
}

void ScreensTest::testCurrentWithFollowsMouse_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expected");

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
    screens()->setCurrentFollowsMouse(true);
    QCOMPARE(screens()->current(), 0);

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::QueuedConnection,
                              Q_ARG(int, geometries.count()), Q_ARG(QVector<QRect>, geometries));
    QVERIFY(changedSpy.wait());

    QFETCH(QPoint, cursorPos);
    KWin::Cursors::self()->mouse()->setPos(cursorPos);
    QTEST(screens()->current(), "expected");
}

void ScreensTest::testCurrentPoint_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expected");

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

    QFETCH(QPoint, cursorPos);
    screens()->setCurrent(cursorPos);
    QTEST(screens()->current(), "expected");
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreensTest)
#include "screens_test.moc"
