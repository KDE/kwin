/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "wayland_server.h"
#include "window.h"
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
    void testCurrent_data();
    void testCurrent();
    void testCurrentWithFollowsMouse_data();
    void testCurrentWithFollowsMouse();
    void testCurrentPoint_data();
    void testCurrentPoint();
};

void ScreensTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
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
    // Destroy the wayland connection of the test window.
    Test::destroyWaylandConnection();

    // Wipe the screens config clean.
    auto config = kwinApp()->config();
    purge(config.data());
    config->sync();
    workspace()->slotReconfigure();

    // Reset the screen layout of the test environment.
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));
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
    Output *output = workspace()->outputs().at(currentId);

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

    QTest::newRow("cloned") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void ScreensTest::testCurrentWithFollowsMouse()
{
    // Enable "active screen follows mouse"
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("ActiveMouseScreen", true);
    group.sync();
    workspace()->slotReconfigure();

    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, geometries));

    QFETCH(QPoint, cursorPos);
    KWin::Cursors::self()->mouse()->setPos(cursorPos);

    QFETCH(int, expectedId);
    Output *expected = workspace()->outputs().at(expectedId);
    QCOMPARE(workspace()->activeOutput(), expected);
}

void ScreensTest::testCurrentPoint_data()
{
    QTest::addColumn<QVector<QRect>>("geometries");
    QTest::addColumn<QPoint>("cursorPos");
    QTest::addColumn<int>("expectedId");

    QTest::newRow("cloned") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{0, 0, 200, 100}}} << QPoint(50, 50) << 0;
    QTest::newRow("adjacent-0") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(199, 99) << 0;
    QTest::newRow("adjacent-1") << QVector<QRect>{{QRect{0, 0, 200, 100}, QRect{200, 100, 400, 300}}} << QPoint(200, 100) << 1;
    QTest::newRow("gap") << QVector<QRect>{{QRect{0, 0, 10, 20}, QRect{20, 40, 10, 20}}} << QPoint(15, 30) << 1;
}

void ScreensTest::testCurrentPoint()
{
    QFETCH(QVector<QRect>, geometries);
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, geometries));

    // Disable "active screen follows mouse"
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("ActiveMouseScreen", false);
    group.sync();
    workspace()->slotReconfigure();

    QFETCH(QPoint, cursorPos);
    workspace()->setActiveOutput(cursorPos);

    QFETCH(int, expectedId);
    Output *expected = workspace()->outputs().at(expectedId);
    QCOMPARE(workspace()->activeOutput(), expected);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreensTest)
#include "screens_test.moc"
