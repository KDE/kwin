/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "atoms.h"
#include "cursor.h"
#include "effect/effectloader.h"
#include "main.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/surface.h>

#include <QAbstractEventDispatcher>
#include <QAction>
#include <QSocketNotifier>

#include <xcb/xcb_icccm.h>

Q_DECLARE_METATYPE(KWin::ElectricBorder)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_screen-edges-0");

class TestObject : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    bool callback(ElectricBorder border)
    {
        Q_EMIT gotCallback(border);
        return true;
    }

Q_SIGNALS:
    void gotCallback(KWin::ElectricBorder);
};

class ScreenEdgesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testTouchCallback_data();
    void testTouchCallback();
    void testPushBack_data();
    void testPushBack();
    void testObjectEdge_data();
    void testObjectEdge();
    void testMultipleEntry_data();
    void testMultipleEntry();
};

void ScreenEdgesTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");

    QVERIFY(waylandServer()->init(s_socketName));

    // Disable effects, in particular present windows, which reserves a screen edge.
    auto config = kwinApp()->config();
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});
}

void ScreenEdgesTest::init()
{
    workspace()->screenEdges()->recreateEdges();
    Workspace::self()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));

    QVERIFY(Test::setupWaylandConnection());
}

void ScreenEdgesTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void ScreenEdgesTest::testTouchCallback_data()
{
    QTest::addColumn<KWin::ElectricBorder>("border");
    QTest::addColumn<QPointF>("startPos");
    QTest::addColumn<QPointF>("delta");

    QTest::newRow("left") << ElectricLeft << QPointF(0, 50) << QPointF(256, 20);
    QTest::newRow("top") << ElectricTop << QPointF(50, 0) << QPointF(20, 250);
    QTest::newRow("right") << ElectricRight << QPointF(1279, 50) << QPointF(-256, 0);
    QTest::newRow("bottom") << ElectricBottom << QPointF(50, 1023) << QPointF(0, -205);
}

void ScreenEdgesTest::testTouchCallback()
{
    // This test verifies that touch screen edges trigger associated callbacks.

    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    auto group = config->group(QStringLiteral("TouchEdges"));
    group.writeEntry("Top", "none");
    group.writeEntry("Left", "none");
    group.writeEntry("Bottom", "none");
    group.writeEntry("Right", "none");
    config->sync();

    auto s = workspace()->screenEdges();
    s->setConfig(config);
    s->reconfigure();

    // none of our actions should be reserved
    const auto &edges = s->edges();
    QCOMPARE(edges.size(), 8);
    for (auto &edge : edges) {
        QCOMPARE(edge->isReserved(), false);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), false);
    }

    // let's reserve an action
    QAction action;
    QSignalSpy actionTriggeredSpy(&action, &QAction::triggered);

    // reserve on edge
    QFETCH(KWin::ElectricBorder, border);
    s->reserveTouch(border, &action);
    for (auto &edge : edges) {
        QCOMPARE(edge->isReserved(), edge->border() == border);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), edge->border() == border);
    }

    quint32 timestamp = 0;

    // press the finger
    QFETCH(QPointF, startPos);
    Test::touchDown(1, startPos, timestamp++);
    QVERIFY(actionTriggeredSpy.isEmpty());

    // move the finger
    QFETCH(QPointF, delta);
    Test::touchMotion(1, startPos + delta, timestamp++);
    QVERIFY(actionTriggeredSpy.isEmpty());

    // release the finger
    Test::touchUp(1, timestamp++);
    QVERIFY(actionTriggeredSpy.wait());
    QCOMPARE(actionTriggeredSpy.count(), 1);

    // unreserve again
    s->unreserveTouch(border, &action);
    for (auto &edge : edges) {
        QCOMPARE(edge->isReserved(), false);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), false);
    }

    // reserve another action
    std::unique_ptr<QAction> action2(new QAction);
    s->reserveTouch(border, action2.get());
    for (auto &edge : edges) {
        QCOMPARE(edge->isReserved(), edge->border() == border);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), edge->border() == border);
    }

    // and unreserve by destroying
    action2.reset();
    for (auto &edge : edges) {
        QCOMPARE(edge->isReserved(), false);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), false);
    }
}

void ScreenEdgesTest::testPushBack_data()
{
    QTest::addColumn<KWin::ElectricBorder>("border");
    QTest::addColumn<int>("pushback");
    QTest::addColumn<QPointF>("trigger");
    QTest::addColumn<QPointF>("expected");

    QTest::newRow("top-left-3") << ElectricTopLeft << 3 << QPointF(0, 0) << QPointF(3, 3);
    QTest::newRow("top-5") << ElectricTop << 5 << QPointF(50, 0) << QPointF(50, 5);
    QTest::newRow("top-right-2") << ElectricTopRight << 2 << QPointF(1279, 0) << QPointF(1277, 2);
    QTest::newRow("right-10") << ElectricRight << 10 << QPointF(1279, 50) << QPointF(1269, 50);
    QTest::newRow("bottom-right-5") << ElectricBottomRight << 5 << QPointF(1279, 1023) << QPointF(1274, 1018);
    QTest::newRow("bottom-10") << ElectricBottom << 10 << QPointF(50, 1023) << QPointF(50, 1013);
    QTest::newRow("bottom-left-3") << ElectricBottomLeft << 3 << QPointF(0, 1023) << QPointF(3, 1020);
    QTest::newRow("left-10") << ElectricLeft << 10 << QPointF(0, 50) << QPointF(10, 50);
    QTest::newRow("invalid") << ElectricLeft << 10 << QPointF(50, 0) << QPointF(50, 0);
}

void ScreenEdgesTest::testPushBack()
{
    // This test verifies that the pointer will be pushed back if it approached a screen edge.

    QFETCH(int, pushback);
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group(QStringLiteral("Windows")).writeEntry("ElectricBorderPushbackPixels", pushback);
    config->sync();

    auto s = workspace()->screenEdges();
    s->setConfig(config);
    s->reconfigure();

    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);

    QFETCH(ElectricBorder, border);
    s->reserve(border, &callback, "callback");

    QFETCH(QPointF, trigger);
    Test::pointerMotion(trigger, 0);
    QVERIFY(spy.isEmpty());
    QTEST(Cursors::self()->mouse()->pos(), "expected");
}

void ScreenEdgesTest::testObjectEdge_data()
{
    QTest::addColumn<ElectricBorder>("border");
    QTest::addColumn<QPointF>("triggerPoint");
    QTest::addColumn<QPointF>("delta");

    QTest::newRow("top") << ElectricTop << QPointF(640, 0) << QPointF(0, 50);
    QTest::newRow("right") << ElectricRight << QPointF(1279, 512) << QPointF(-50, 0);
    QTest::newRow("bottom") << ElectricBottom << QPointF(640, 1023) << QPointF(0, -50);
    QTest::newRow("left") << ElectricLeft << QPointF(0, 512) << QPointF(50, 0);
}

void ScreenEdgesTest::testObjectEdge()
{
    // This test verifies that a screen edge reserved by a script or any QObject is activated.

    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);

    // Reserve a screen edge border.
    QFETCH(ElectricBorder, border);
    workspace()->screenEdges()->reserve(border, &callback, "callback");

    QFETCH(QPointF, triggerPoint);
    QFETCH(QPointF, delta);

    // doesn't trigger as the edge was not triggered yet
    qint64 timestamp = 0;
    Test::pointerMotion(triggerPoint + delta, timestamp);
    QVERIFY(spy.isEmpty());

    // test doesn't trigger due to too much offset
    timestamp += 160;
    Test::pointerMotion(triggerPoint, timestamp);
    QVERIFY(spy.isEmpty());

    // doesn't activate as we are waiting too short
    timestamp += 50;
    Test::pointerMotion(triggerPoint, timestamp);
    QVERIFY(spy.isEmpty());

    // and this one triggers
    timestamp += 110;
    Test::pointerMotion(triggerPoint, timestamp);
    QVERIFY(!spy.isEmpty());

    // now let's try to trigger again
    timestamp += 351;
    Test::pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 1);

    // it's still under the reactivation
    timestamp += 50;
    Test::pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 1);

    // now it should trigger again
    timestamp += 250;
    Test::pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 2);
}

void ScreenEdgesTest::testMultipleEntry_data()
{
    QTest::addColumn<ElectricBorder>("border");
    QTest::addColumn<QPointF>("triggerPoint");
    QTest::addColumn<QPointF>("delta");

    QTest::newRow("top") << ElectricTop << QPointF(640, 0) << QPointF(0, 50);
    QTest::newRow("right") << ElectricRight << QPointF(1279, 512) << QPointF(-50, 0);
    QTest::newRow("bottom") << ElectricBottom << QPointF(640, 1023) << QPointF(0, -50);
    QTest::newRow("left") << ElectricLeft << QPointF(0, 512) << QPointF(50, 0);
}

void ScreenEdgesTest::testMultipleEntry()
{
    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);

    // Reserve a screen edge border.
    QFETCH(ElectricBorder, border);
    workspace()->screenEdges()->reserve(border, &callback, "callback");

    QFETCH(QPointF, triggerPoint);
    QFETCH(QPointF, delta);

    qint64 timestamp = 0;

    while (timestamp < 300) {
        // doesn't activate from repeated entries of short duration
        Test::pointerMotion(triggerPoint, timestamp);
        QVERIFY(spy.isEmpty());
        timestamp += 50;
        Test::pointerMotion(triggerPoint + delta, timestamp);
        QVERIFY(spy.isEmpty());
        timestamp += 50;
    }

    // and this one triggers
    Test::pointerMotion(triggerPoint, timestamp);
    timestamp += 110;
    Test::pointerMotion(triggerPoint, timestamp);
    QVERIFY(!spy.isEmpty());
    QCOMPARE(spy.count(), 1);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreenEdgesTest)
#include "screenedges_test.moc"
