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
    void testKdeNetWmScreenEdgeShow();
};

void ScreenEdgesTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");

    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({QRect(0, 0, 1280, 1024)});

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

static void enableAutoHide(xcb_connection_t *connection, xcb_window_t windowId, ElectricBorder border)
{
    if (border == ElectricNone) {
        xcb_delete_property(connection, windowId, atoms->kde_screen_edge_show);
    } else {
        uint32_t value = 0;

        switch (border) {
        case ElectricTop:
            value = 0;
            break;
        case ElectricRight:
            value = 1;
            break;
        case ElectricBottom:
            value = 2;
            break;
        case ElectricLeft:
            value = 3;
            break;
        default:
            Q_UNREACHABLE();
        }

        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, windowId, atoms->kde_screen_edge_show, XCB_ATOM_CARDINAL, 32, 1, &value);
    }
}

class ScreenEdgePropertyMonitor : public QObject
{
    Q_OBJECT
public:
    ScreenEdgePropertyMonitor(xcb_connection_t *c, xcb_window_t window)
        : QObject()
        , m_connection(c)
        , m_window(window)
        , m_notifier(new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this))
    {
        connect(m_notifier, &QSocketNotifier::activated, this, &ScreenEdgePropertyMonitor::processXcbEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &ScreenEdgePropertyMonitor::processXcbEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &ScreenEdgePropertyMonitor::processXcbEvents);
    }

Q_SIGNALS:
    void withdrawn();

private:
    void processXcbEvents()
    {
        while (auto event = xcb_poll_for_event(m_connection)) {
            const uint8_t eventType = event->response_type & ~0x80;
            switch (eventType) {
            case XCB_PROPERTY_NOTIFY: {
                auto propertyNotifyEvent = reinterpret_cast<xcb_property_notify_event_t *>(event);
                if (propertyNotifyEvent->window == m_window && propertyNotifyEvent->atom == atoms->kde_screen_edge_show && propertyNotifyEvent->state == XCB_PROPERTY_DELETE) {
                    Q_EMIT withdrawn();
                }
                break;
            }
            }
            free(event);
        }
    }

    xcb_connection_t *m_connection;
    xcb_window_t m_window;
    QSocketNotifier *m_notifier;
};

void ScreenEdgesTest::testKdeNetWmScreenEdgeShow()
{
    // This test verifies that _KDE_NET_WM_SCREEN_EDGE_SHOW is handled properly. Note that
    // _KDE_NET_WM_SCREEN_EDGE_SHOW has oneshot effect. It's deleted when the window is shown.

    auto config = kwinApp()->config();
    config->group(QStringLiteral("Windows")).writeEntry("ElectricBorderDelay", 75);
    config->sync();
    workspace()->slotReconfigure();

    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    // Create a test window at the bottom of the screen.
    const QRect windowGeometry(0, 1024 - 30, 1280, 30);
    const uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
    xcb_window_t windowId = xcb_generate_id(c.get());
    xcb_create_window(c.get(), XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      windowGeometry.x(),
                      windowGeometry.y(),
                      windowGeometry.width(),
                      windowGeometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK, values);
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_position(&hints, 1, windowGeometry.x(), windowGeometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, windowGeometry.width(), windowGeometry.height());
    xcb_icccm_set_wm_normal_hints(c.get(), windowId, &hints);
    xcb_change_property(c.get(), XCB_PROP_MODE_REPLACE, windowId, atoms->wm_client_leader, XCB_ATOM_WINDOW, 32, 1, &windowId);
    xcb_map_window(c.get(), windowId);
    xcb_flush(c.get());

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    QVERIFY(windowCreatedSpy.wait());
    Window *window = windowCreatedSpy.first().first().value<Window *>();
    QVERIFY(window);

    ScreenEdgePropertyMonitor screenEdgeMonitor(c.get(), windowId);
    QSignalSpy withdrawnSpy(&screenEdgeMonitor, &ScreenEdgePropertyMonitor::withdrawn);
    QSignalSpy hiddenChangedSpy(window, &Window::hiddenChanged);
    quint32 timestamp = 0;

    // The window will be shown when the pointer approaches its reserved screen edge.
    {
        enableAutoHide(c.get(), windowId, ElectricBottom);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(!window->isShown());

        Test::pointerMotion(QPointF(640, 1023), timestamp);
        timestamp += 160;
        Test::pointerMotion(QPointF(640, 1023), timestamp);
        QVERIFY(withdrawnSpy.wait());
        QVERIFY(window->isShown());
        timestamp += 160;
        Test::pointerMotion(QPointF(640, 512), timestamp);
        QVERIFY(window->isShown());
    }

    // The window will be shown when swiping on the touch screen.
    {
        enableAutoHide(c.get(), windowId, ElectricBottom);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(!window->isShown());

        Test::touchDown(0, QPointF(640, 1023), timestamp++);
        Test::touchMotion(0, QPointF(640, 512), timestamp++);
        Test::touchUp(0, timestamp++);
        QVERIFY(withdrawnSpy.wait());
        QVERIFY(window->isShown());
    }

    // The screen edge reservation won't be affected when recreating screen edges (can happen when the screen layout changes).
    {
        enableAutoHide(c.get(), windowId, ElectricBottom);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(!window->isShown());

        workspace()->screenEdges()->recreateEdges();
        QVERIFY(!withdrawnSpy.wait(50));
        QVERIFY(!window->isShown());

        enableAutoHide(c.get(), windowId, ElectricNone);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(window->isShown());
    }

    // The window will be shown and hidden in response to changing _KDE_NET_WM_SCREEN_EDGE_SHOW.
    {
        enableAutoHide(c.get(), windowId, ElectricBottom);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(!window->isShown());

        enableAutoHide(c.get(), windowId, ElectricNone);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(window->isShown());
    }

    // The approaching state will be reset if the window is shown manually.
    {
        QSignalSpy approachingSpy(workspace()->screenEdges(), &ScreenEdges::approaching);
        enableAutoHide(c.get(), windowId, ElectricBottom);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(!window->isShown());

        Test::pointerMotion(QPointF(640, 1020), timestamp++);
        QVERIFY(approachingSpy.last().at(1).toReal() == 0.0);
        Test::pointerMotion(QPointF(640, 1021), timestamp++);
        QVERIFY(approachingSpy.last().at(1).toReal() != 0.0);

        enableAutoHide(c.get(), windowId, ElectricNone);
        xcb_flush(c.get());
        QVERIFY(hiddenChangedSpy.wait());
        QVERIFY(window->isShown());
        QVERIFY(approachingSpy.last().at(1).toReal() == 0.0);

        Test::pointerMotion(QPointF(640, 512), timestamp++);
    }
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreenEdgesTest)
#include "screenedges_test.moc"
