/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "cursor.h"
#include "effectloader.h"
#include "main.h"
#include "screenedge.h"
#include "platform.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/surface.h>

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
    void testClientEdge_data();
    void testClientEdge();
    void testObjectEdge_data();
    void testObjectEdge();
};

void ScreenEdgesTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient *>();
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));

    // Disable effects, in particular present windows, which reserves a screen edge.
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    const auto builtinNames = EffectLoader().listOfKnownEffects();
    for (const QString &name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    Test::initWaylandWorkspace();
}

void ScreenEdgesTest::init()
{
    ScreenEdges::self()->recreateEdges();
    Workspace::self()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));

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
    auto group = config->group("TouchEdges");
    group.writeEntry("Top", "none");
    group.writeEntry("Left", "none");
    group.writeEntry("Bottom", "none");
    group.writeEntry("Right", "none");
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->reconfigure();

    // none of our actions should be reserved
    const QList<Edge *> edges = s->findChildren<Edge *>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto edge : edges) {
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
    for (auto edge : edges) {
        QCOMPARE(edge->isReserved(), edge->border() == border);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), edge->border() == border);
    }

    quint32 timestamp = 0;

    // press the finger
    QFETCH(QPointF, startPos);
    kwinApp()->platform()->touchDown(1, startPos, timestamp++);
    QVERIFY(actionTriggeredSpy.isEmpty());

    // move the finger
    QFETCH(QPointF, delta);
    kwinApp()->platform()->touchMotion(1, startPos + delta, timestamp++);
    QVERIFY(actionTriggeredSpy.isEmpty());

    // release the finger
    kwinApp()->platform()->touchUp(1, timestamp++);
    QVERIFY(actionTriggeredSpy.wait());
    QCOMPARE(actionTriggeredSpy.count(), 1);

    // unreserve again
    s->unreserveTouch(border, &action);
    for (auto edge : edges) {
        QCOMPARE(edge->isReserved(), false);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), false);
    }

    // reserve another action
    QScopedPointer<QAction> action2(new QAction);
    s->reserveTouch(border, action2.data());
    for (auto edge : edges) {
        QCOMPARE(edge->isReserved(), edge->border() == border);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), edge->border() == border);
    }

    // and unreserve by destroying
    action2.reset();
    for (auto edge : edges) {
        QCOMPARE(edge->isReserved(), false);
        QCOMPARE(edge->activatesForPointer(), false);
        QCOMPARE(edge->activatesForTouchGesture(), false);
    }
}

void ScreenEdgesTest::testPushBack_data()
{
    QTest::addColumn<KWin::ElectricBorder>("border");
    QTest::addColumn<int>("pushback");
    QTest::addColumn<QPoint>("trigger");
    QTest::addColumn<QPoint>("expected");

    QTest::newRow("top-left-3") << ElectricTopLeft << 3 << QPoint(0, 0) << QPoint(3, 3);
    QTest::newRow("top-5") << ElectricTop << 5 << QPoint(50, 0) << QPoint(50, 5);
    QTest::newRow("top-right-2") << ElectricTopRight << 2 << QPoint(1279, 0) << QPoint(1277, 2);
    QTest::newRow("right-10") << ElectricRight << 10 << QPoint(1279, 50) << QPoint(1269, 50);
    QTest::newRow("bottom-right-5") << ElectricBottomRight << 5 << QPoint(1279, 1023) << QPoint(1274, 1018);
    QTest::newRow("bottom-10") << ElectricBottom << 10 << QPoint(50, 1023) << QPoint(50, 1013);
    QTest::newRow("bottom-left-3") << ElectricBottomLeft << 3 << QPoint(0, 1023) << QPoint(3, 1020);
    QTest::newRow("left-10") << ElectricLeft << 10 << QPoint(0, 50) << QPoint(10, 50);
    QTest::newRow("invalid") << ElectricLeft << 10 << QPoint(50, 0) << QPoint(50, 0);
}

void ScreenEdgesTest::testPushBack()
{
    // This test verifies that the pointer will be pushed back if it approached a screen edge.

    QFETCH(int, pushback);
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("ElectricBorderPushbackPixels", pushback);
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->reconfigure();

    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);

    QFETCH(ElectricBorder, border);
    s->reserve(border, &callback, "callback");

    QFETCH(QPoint, trigger);
    kwinApp()->platform()->pointerMotion(trigger, 0);
    QVERIFY(spy.isEmpty());
    QTEST(Cursors::self()->mouse()->pos(), "expected");
}

void ScreenEdgesTest::testClientEdge_data()
{
    QTest::addColumn<ElectricBorder>("border");
    QTest::addColumn<QRect>("geometry");
    QTest::addColumn<QPointF>("triggerPoint");

    QTest::newRow("top") << ElectricTop << QRect(540, 0, 200, 5) << QPointF(640, 0);
    QTest::newRow("right") << ElectricRight << QRect(1275, 412, 5, 200) << QPointF(1279, 512);
    QTest::newRow("bottom") << ElectricBottom << QRect(540, 1019, 200, 5) << QPointF(640, 1023);
    QTest::newRow("left") << ElectricLeft << QRect(0, 412, 5, 200) << QPointF(0, 512);
}

void ScreenEdgesTest::testClientEdge()
{
    // This test verifies that a client will be shown when its screen edge is activated.
    QFETCH(QRect, geometry);

    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data()));
    AbstractClient *client = Test::renderAndWaitForShown(surface.data(), geometry.size(), Qt::red);
    QVERIFY(client);
    QVERIFY(client->isActive());
    client->move(geometry.topLeft());

    // Reserve an electric border.
    QFETCH(ElectricBorder, border);
    ScreenEdges::self()->reserve(client, border);

    // Hide the window.
    client->hideClient();
    QVERIFY(client->isHiddenInternal());

    // Trigger the screen edge.
    QFETCH(QPointF, triggerPoint);
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QVERIFY(client->isHiddenInternal());

    timestamp += 150 + 1;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QTRY_VERIFY(!client->isHiddenInternal());
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
    ScreenEdges::self()->reserve(border, &callback, "callback");

    QFETCH(QPointF, triggerPoint);
    QFETCH(QPointF, delta);

    // doesn't trigger as the edge was not triggered yet
    qint64 timestamp = 0;
    kwinApp()->platform()->pointerMotion(triggerPoint + delta, timestamp);
    QVERIFY(spy.isEmpty());

    // test doesn't trigger due to too much offset
    timestamp += 160;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QVERIFY(spy.isEmpty());

    // doesn't activate as we are waiting too short
    timestamp += 50;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QVERIFY(spy.isEmpty());

    // and this one triggers
    timestamp += 110;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QVERIFY(!spy.isEmpty());

    // now let's try to trigger again
    timestamp += 351;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 1);

    // it's still under the reactivation
    timestamp += 50;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 1);

    // now it should trigger again
    timestamp += 250;
    kwinApp()->platform()->pointerMotion(triggerPoint, timestamp);
    QCOMPARE(spy.count(), 2);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreenEdgesTest)
#include "screenedges_test.moc"
