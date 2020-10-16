/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// kwin
#include "../atoms.h"
#include "../cursor.h"
#include "../input.h"
#include "../gestures.h"
#include "../main.h"
#include "../screenedge.h"
#include "../screens.h"
#include "../utils.h"
#include "../virtualdesktops.h"
#include "../xcbutils.h"
#include "../platform.h"
#include "mock_screens.h"
#include "mock_workspace.h"
#include "mock_x11client.h"
#include "testutils.h"
// Frameworks
#include <KConfigGroup>
// Qt
#include <QtTest>
#include <QX11Info>
// xcb
#include <xcb/xcb.h>
Q_DECLARE_METATYPE(KWin::ElectricBorder)

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core")

namespace KWin
{

Atoms* atoms;
int screen_number = 0;

InputRedirection *InputRedirection::s_self = nullptr;

void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action)
{
    Q_UNUSED(shortcut)
    Q_UNUSED(action)
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
   Q_UNUSED(modifiers)
   Q_UNUSED(axis)
   Q_UNUSED(action)
}

void InputRedirection::registerTouchpadSwipeShortcut(SwipeDirection, QAction*)
{
}

void updateXTime()
{
}

class TestObject : public QObject
{
    Q_OBJECT
public Q_SLOTS:
    bool callback(ElectricBorder border);
Q_SIGNALS:
    void gotCallback(KWin::ElectricBorder);
};

bool TestObject::callback(KWin::ElectricBorder border)
{
    emit gotCallback(border);
    return true;
}

}

class TestScreenEdges : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void testInit();
    void testCreatingInitialEdges();
    void testCallback();
    void testCallbackWithCheck();
    void testOverlappingEdges_data();
    void testOverlappingEdges();
    void testPushBack_data();
    void testPushBack();
    void testFullScreenBlocking();
    void testClientEdge();
    void testTouchEdge();
    void testTouchCallback_data();
    void testTouchCallback();
};

void TestScreenEdges::initTestCase()
{
    qApp->setProperty("x11RootWindow", QVariant::fromValue<quint32>(QX11Info::appRootWindow()));
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
    KWin::atoms = new KWin::Atoms;
    qRegisterMetaType<KWin::ElectricBorder>();
}

void TestScreenEdges::cleanupTestCase()
{
    delete KWin::atoms;
}

void TestScreenEdges::init()
{
    KWin::Cursors::self()->setMouse(new KWin::Cursor(this));

    using namespace KWin;
    new MockWorkspace(this);
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    Screens::create();
    QSignalSpy sp(screens(), &MockScreens::changed);
    QVERIFY(sp.wait());

    auto vd = VirtualDesktopManager::create();
    vd->setConfig(config);
    vd->load();
    auto s = ScreenEdges::create();
    s->setConfig(config);
}

void TestScreenEdges::cleanup()
{
    using namespace KWin;
    delete ScreenEdges::self();
    delete VirtualDesktopManager::self();
    delete Screens::self();
    delete workspace();
}

void TestScreenEdges::testInit()
{
    using namespace KWin;
    auto s = ScreenEdges::self();
    s->init();
    QCOMPARE(s->isDesktopSwitching(), false);
    QCOMPARE(s->isDesktopSwitchingMovingClients(), false);
    QCOMPARE(s->timeThreshold(), 150);
    QCOMPARE(s->reActivationThreshold(), 350);
    QCOMPARE(s->cursorPushBackDistance(), QSize(1, 1));
    QCOMPARE(s->actionTopLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTop(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTopRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottom(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionLeft(), ElectricBorderAction::ElectricActionNone);

    QList<Edge*> edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto e : edges) {
        QVERIFY(!e->isReserved());
        QVERIFY(e->inherits("KWin::WindowBasedEdge"));
        QVERIFY(!e->inherits("KWin::AreaBasedEdge"));
        QVERIFY(!e->client());
        QVERIFY(!e->isApproaching());
    }
    Edge *te = edges.at(0);
    QVERIFY(te->isCorner());
    QVERIFY(!te->isScreenEdge());
    QVERIFY(te->isLeft());
    QVERIFY(te->isTop());
    QVERIFY(!te->isRight());
    QVERIFY(!te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricTopLeft);
    te = edges.at(1);
    QVERIFY(te->isCorner());
    QVERIFY(!te->isScreenEdge());
    QVERIFY(te->isLeft());
    QVERIFY(!te->isTop());
    QVERIFY(!te->isRight());
    QVERIFY(te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricBottomLeft);
    te = edges.at(2);
    QVERIFY(!te->isCorner());
    QVERIFY(te->isScreenEdge());
    QVERIFY(te->isLeft());
    QVERIFY(!te->isTop());
    QVERIFY(!te->isRight());
    QVERIFY(!te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricLeft);
    te = edges.at(3);
    QVERIFY(te->isCorner());
    QVERIFY(!te->isScreenEdge());
    QVERIFY(!te->isLeft());
    QVERIFY(te->isTop());
    QVERIFY(te->isRight());
    QVERIFY(!te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricTopRight);
    te = edges.at(4);
    QVERIFY(te->isCorner());
    QVERIFY(!te->isScreenEdge());
    QVERIFY(!te->isLeft());
    QVERIFY(!te->isTop());
    QVERIFY(te->isRight());
    QVERIFY(te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricBottomRight);
    te = edges.at(5);
    QVERIFY(!te->isCorner());
    QVERIFY(te->isScreenEdge());
    QVERIFY(!te->isLeft());
    QVERIFY(!te->isTop());
    QVERIFY(te->isRight());
    QVERIFY(!te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricRight);
    te = edges.at(6);
    QVERIFY(!te->isCorner());
    QVERIFY(te->isScreenEdge());
    QVERIFY(!te->isLeft());
    QVERIFY(te->isTop());
    QVERIFY(!te->isRight());
    QVERIFY(!te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricTop);
    te = edges.at(7);
    QVERIFY(!te->isCorner());
    QVERIFY(te->isScreenEdge());
    QVERIFY(!te->isLeft());
    QVERIFY(!te->isTop());
    QVERIFY(!te->isRight());
    QVERIFY(te->isBottom());
    QCOMPARE(te->border(), ElectricBorder::ElectricBottom);

    // we shouldn't have any x windows, though
    QCOMPARE(s->windows().size(), 0);
}

void TestScreenEdges::testCreatingInitialEdges()
{
    using namespace KWin;
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("ElectricBorders", 2/*ElectricAlways*/);
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->init();
    // we don't have multiple desktops, so it's returning false
    QCOMPARE(s->isDesktopSwitching(), true);
    QCOMPARE(s->isDesktopSwitchingMovingClients(), true);
    QCOMPARE(s->actionTopLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTop(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTopRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottom(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionLeft(), ElectricBorderAction::ElectricActionNone);

    QEXPECT_FAIL("", "needs fixing", Continue);
    QCOMPARE(s->windows().size(), 0);

    // set some reasonable virtual desktops
    config->group("Desktops").writeEntry("Number", 4);
    config->sync();
    auto vd = VirtualDesktopManager::self();
    vd->setConfig(config);
    vd->load();
    vd->updateLayout();
    QCOMPARE(vd->count(), 4u);
    QCOMPARE(vd->grid().width(), 2);
    QCOMPARE(vd->grid().height(), 2);

    // approach windows for edges not created as screen too small
    s->updateLayout();
    auto edgeWindows = s->windows();
    QCOMPARE(edgeWindows.size(), 12);

    auto testWindowGeometry = [&](int index) {
        Xcb::WindowGeometry geo(edgeWindows[index]);
        return geo.rect();
    };
    QRect sg = screens()->geometry();
    const int co = s->cornerOffset();
    QList<QRect> expectedGeometries{
        QRect(0, 0, 1, 1),
        QRect(0, 0, co, co),
        QRect(0, sg.bottom(), 1, 1),
        QRect(0, sg.height() - co, co, co),
        QRect(0, co, 1, sg.height() - co*2),
//         QRect(0, co * 2 + 1, co, sg.height() - co*4),
        QRect(sg.right(), 0, 1, 1),
        QRect(sg.right() - co + 1, 0, co, co),
        QRect(sg.right(), sg.bottom(), 1, 1),
        QRect(sg.right() - co + 1, sg.bottom() - co + 1, co, co),
        QRect(sg.right(), co, 1, sg.height() - co*2),
//         QRect(sg.right() - co + 1, co * 2, co, sg.height() - co*4),
        QRect(co, 0, sg.width() - co * 2, 1),
//         QRect(co * 2, 0, sg.width() - co * 4, co),
        QRect(co, sg.bottom(), sg.width() - co * 2, 1),
//         QRect(co * 2, sg.height() - co, sg.width() - co * 4, co)
    };
    for (int i = 0; i < 12; ++i) {
        QCOMPARE(testWindowGeometry(i), expectedGeometries.at(i));
    }
    QList<Edge*> edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto e : edges) {
        QVERIFY(e->isReserved());
        QCOMPARE(e->activatesForPointer(), true);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }

    static_cast<MockScreens*>(screens())->setGeometries(QList<QRect>{QRect{0, 0, 1024, 768}});
    QSignalSpy changedSpy(screens(), &Screens::changed);
    QVERIFY(changedSpy.isValid());
    QVERIFY(changedSpy.wait());

    // let's update the layout and verify that we have edges
    s->recreateEdges();
    edgeWindows = s->windows();
    QCOMPARE(edgeWindows.size(), 16);
    sg = screens()->geometry();
    expectedGeometries = QList<QRect>{
        QRect(0, 0, 1, 1),
        QRect(0, 0, co, co),
        QRect(0, sg.bottom(), 1, 1),
        QRect(0, sg.height() - co, co, co),
        QRect(0, co, 1, sg.height() - co*2),
        QRect(0, co * 2 + 1, co, sg.height() - co*4),
        QRect(sg.right(), 0, 1, 1),
        QRect(sg.right() - co + 1, 0, co, co),
        QRect(sg.right(), sg.bottom(), 1, 1),
        QRect(sg.right() - co + 1, sg.bottom() - co + 1, co, co),
        QRect(sg.right(), co, 1, sg.height() - co*2),
        QRect(sg.right() - co + 1, co * 2, co, sg.height() - co*4),
        QRect(co, 0, sg.width() - co * 2, 1),
        QRect(co * 2, 0, sg.width() - co * 4, co),
        QRect(co, sg.bottom(), sg.width() - co * 2, 1),
        QRect(co * 2, sg.height() - co, sg.width() - co * 4, co)
    };
    for (int i = 0; i < 16; ++i) {
        QCOMPARE(testWindowGeometry(i), expectedGeometries.at(i));
    }

    // disable desktop switching again
    config->group("Windows").writeEntry("ElectricBorders", 1/*ElectricMoveOnly*/);
    s->reconfigure();
    QCOMPARE(s->isDesktopSwitching(), false);
    QCOMPARE(s->isDesktopSwitchingMovingClients(), true);
    QCOMPARE(s->windows().size(), 0);
    edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (int i = 0; i < 8; ++i) {
        auto e = edges.at(i);
        QVERIFY(!e->isReserved());
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
        QCOMPARE(e->approachGeometry(), expectedGeometries.at(i*2+1));
    }

    // let's start a move of window.
    X11Client client(workspace());
    workspace()->setMoveResizeClient(&client);
    for (int i = 0; i < 8; ++i) {
        auto e = edges.at(i);
        QVERIFY(!e->isReserved());
        QCOMPARE(e->activatesForPointer(), true);
        QCOMPARE(e->activatesForTouchGesture(), false);
        QCOMPARE(e->approachGeometry(), expectedGeometries.at(i*2+1));
    }
    // not for resize
    client.setResize(true);
    for (int i = 0; i < 8; ++i) {
        auto e = edges.at(i);
        QVERIFY(!e->isReserved());
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
        QCOMPARE(e->approachGeometry(), expectedGeometries.at(i*2+1));
    }
    workspace()->setMoveResizeClient(nullptr);
}

void TestScreenEdges::testCallback()
{
    using namespace KWin;
    MockWorkspace ws;
    static_cast<MockScreens*>(screens())->setGeometries(QList<QRect>{QRect{0, 0, 1024, 768}, QRect{200, 768, 1024, 768}});
    QSignalSpy changedSpy(screens(), &Screens::changed);
    QVERIFY(changedSpy.isValid());
    // first is before it's updated
    QVERIFY(changedSpy.wait());
    // second is after it's updated
    QVERIFY(changedSpy.wait());
    auto s = ScreenEdges::self();
    s->init();
    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);
    QVERIFY(spy.isValid());
    s->reserve(ElectricLeft, &callback, "callback");
    s->reserve(ElectricTopLeft, &callback, "callback");
    s->reserve(ElectricTop, &callback, "callback");
    s->reserve(ElectricTopRight, &callback, "callback");
    s->reserve(ElectricRight, &callback, "callback");
    s->reserve(ElectricBottomRight, &callback, "callback");
    s->reserve(ElectricBottom, &callback, "callback");
    s->reserve(ElectricBottomLeft, &callback, "callback");

    QList<Edge*> edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 10);
    for (auto e: edges) {
        QVERIFY(e->isReserved());
        QCOMPARE(e->activatesForPointer(), true);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }
    auto it = std::find_if(edges.constBegin(), edges.constEnd(), [](Edge *e) {
        return e->isScreenEdge() && e->isLeft() && e->approachGeometry().bottom() < 768;
    });
    QVERIFY(it != edges.constEnd());

    xcb_enter_notify_event_t event;
    auto setPos = [&event] (const QPoint &pos) {
        Cursors::self()->mouse()->setPos(pos);
        event.root_x = pos.x();
        event.root_y = pos.y();
        event.event_x = pos.x();
        event.event_y = pos.y();
    };
    event.root = XCB_WINDOW_NONE;
    event.child = XCB_WINDOW_NONE;
    event.event = (*it)->window();
    event.same_screen_focus = 1;
    event.time = QDateTime::currentMSecsSinceEpoch();
    setPos(QPoint(0, 50));
    auto isEntered = [s] (xcb_enter_notify_event_t *event) {
        return s->handleEnterNotifiy(event->event, QPoint(event->root_x, event->root_y), QDateTime::fromMSecsSinceEpoch(event->time, Qt::UTC));
    };
    QVERIFY(isEntered(&event));
    // doesn't trigger as the edge was not triggered yet
    QVERIFY(spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));

    // test doesn't trigger due to too much offset
    QTest::qWait(160);
    setPos(QPoint(0, 100));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 100));

    // doesn't trigger as we are waiting too long already
    QTest::qWait(200);
    setPos(QPoint(0, 101));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 101));

    // doesn't activate as we are waiting too short
    QTest::qWait(50);
    setPos(QPoint(0, 100));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 100));

    // and this one triggers
    QTest::qWait(110);
    setPos(QPoint(0, 101));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(!spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 101));

    // now let's try to trigger again
    QTest::qWait(351);
    setPos(QPoint(0, 100));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 100));
    // it's still under the reactivation
    QTest::qWait(50);
    setPos(QPoint(0, 100));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 100));
    // now it should trigger again
    QTest::qWait(250);
    setPos(QPoint(0, 100));
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.first().first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(spy.last().first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 100));

    // let's disable pushback
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("ElectricBorderPushbackPixels", 0);
    config->sync();
    s->setConfig(config);
    s->reconfigure();
    // it should trigger directly
    QTest::qWait(350);
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(spy.at(1).first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(spy.at(2).first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(0, 100));

    // now let's unreserve again
    s->unreserve(ElectricTopLeft, &callback);
    s->unreserve(ElectricTop, &callback);
    s->unreserve(ElectricTopRight, &callback);
    s->unreserve(ElectricRight, &callback);
    s->unreserve(ElectricBottomRight, &callback);
    s->unreserve(ElectricBottom, &callback);
    s->unreserve(ElectricBottomLeft, &callback);
    s->unreserve(ElectricLeft, &callback);
    for (auto e: s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly)) {
        QVERIFY(!e->isReserved());
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }
}

void TestScreenEdges::testCallbackWithCheck()
{
    using namespace KWin;
    auto s = ScreenEdges::self();
    s->init();
    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);
    QVERIFY(spy.isValid());
    s->reserve(ElectricLeft, &callback, "callback");

    // check activating a different edge doesn't do anything
    s->check(QPoint(50, 0), QDateTime::currentDateTimeUtc(), true);
    QVERIFY(spy.isEmpty());

    // try a direct activate without pushback
    Cursors::self()->mouse()->setPos(0, 50);
    s->check(QPoint(0, 50), QDateTime::currentDateTimeUtc(), true);
    QCOMPARE(spy.count(), 1);
    QEXPECT_FAIL("", "Argument says force no pushback, but it gets pushed back. Needs investigation", Continue);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(0, 50));

    // use a different edge, this time with pushback
    s->reserve(KWin::ElectricRight, &callback, "callback");
    Cursors::self()->mouse()->setPos(99, 50);
    s->check(QPoint(99, 50), QDateTime::currentDateTimeUtc());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().first().value<ElectricBorder>(), ElectricLeft);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(98, 50));
    // and trigger it again
    QTest::qWait(160);
    Cursors::self()->mouse()->setPos(99, 50);
    s->check(QPoint(99, 50), QDateTime::currentDateTimeUtc());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().first().value<ElectricBorder>(), ElectricRight);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(98, 50));
}

void TestScreenEdges::testOverlappingEdges_data()
{
    QTest::addColumn<QRect>("geo1");
    QTest::addColumn<QRect>("geo2");

    QTest::newRow("topleft-1x1") << QRect{0, 1, 1024, 768} << QRect{1, 0, 1024, 768};
    QTest::newRow("left-1x1-same") << QRect{0, 1, 1024, 766} << QRect{1, 0, 1024, 768};
    QTest::newRow("left-1x1-exchanged") << QRect{0, 1, 1024, 768} << QRect{1, 0, 1024, 766};
    QTest::newRow("bottomleft-1x1") << QRect{0, 0, 1024, 768} << QRect{1, 0, 1024, 769};
    QTest::newRow("bottomright-1x1") << QRect{0, 0, 1024, 768} << QRect{0, 0, 1023, 769};
    QTest::newRow("right-1x1-same") << QRect{0, 0, 1024, 768} << QRect{0, 1, 1025, 766};
    QTest::newRow("right-1x1-exchanged") << QRect{0, 0, 1024, 768} << QRect{1, 1, 1024, 768};
}


void TestScreenEdges::testOverlappingEdges()
{
    using namespace KWin;
    QFETCH(QRect, geo1);
    QFETCH(QRect, geo2);

    MockScreens* mockScreens = static_cast<MockScreens*>(screens());
    QSignalSpy sp(mockScreens, &MockScreens::changed);
    mockScreens->setGeometries({geo1, geo2});
    QVERIFY(sp.wait());

    QCOMPARE(screens()->count(), 2);
    auto screenEdges = ScreenEdges::self();
    screenEdges->init();
}

void TestScreenEdges::testPushBack_data()
{
    QTest::addColumn<KWin::ElectricBorder>("border");
    QTest::addColumn<int>("pushback");
    QTest::addColumn<QPoint>("trigger");
    QTest::addColumn<QPoint>("expected");

    QTest::newRow("topleft-3") << KWin::ElectricTopLeft << 3 << QPoint(0, 0) << QPoint(3, 3);
    QTest::newRow("top-5") << KWin::ElectricTop << 5 << QPoint(50, 0) << QPoint(50, 5);
    QTest::newRow("toprigth-2") << KWin::ElectricTopRight << 2 << QPoint(99, 0) << QPoint(97, 2);
    QTest::newRow("right-10") << KWin::ElectricRight << 10 << QPoint(99, 50) << QPoint(89, 50);
    QTest::newRow("bottomright-5") << KWin::ElectricBottomRight << 5 << QPoint(99, 99) << QPoint(94, 94);
    QTest::newRow("bottom-10") << KWin::ElectricBottom << 10 << QPoint(50, 99) << QPoint(50, 89);
    QTest::newRow("bottomleft-3") << KWin::ElectricBottomLeft << 3 << QPoint(0, 99) << QPoint(3, 96);
    QTest::newRow("left-10") << KWin::ElectricLeft << 10 << QPoint(0, 50) << QPoint(10, 50);
    QTest::newRow("invalid") << KWin::ElectricLeft << 10 << QPoint(50, 0) << QPoint(50, 0);
}

void TestScreenEdges::testPushBack()
{
    using namespace KWin;
    QFETCH(int, pushback);
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("ElectricBorderPushbackPixels", pushback);
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->init();
    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);
    QVERIFY(spy.isValid());
    QFETCH(ElectricBorder, border);
    s->reserve(border, &callback, "callback");

    QFETCH(QPoint, trigger);
    Cursors::self()->mouse()->setPos(trigger);
    xcb_enter_notify_event_t event;
    event.root_x = trigger.x();
    event.root_y = trigger.y();
    event.event_x = trigger.x();
    event.event_y = trigger.y();
    event.root = XCB_WINDOW_NONE;
    event.child = XCB_WINDOW_NONE;
    event.event = s->windows().first();
    event.same_screen_focus = 1;
    event.time = QDateTime::currentMSecsSinceEpoch();
    auto isEntered = [s] (xcb_enter_notify_event_t *event) {
        return s->handleEnterNotifiy(event->event, QPoint(event->root_x, event->root_y), QDateTime::fromMSecsSinceEpoch(event->time));
    };
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    QTEST(Cursors::self()->mouse()->pos(), "expected");

    // do the same without the event, but the check method
    Cursors::self()->mouse()->setPos(trigger);
    s->check(trigger, QDateTime::currentDateTimeUtc());
    QVERIFY(spy.isEmpty());
    QTEST(Cursors::self()->mouse()->pos(), "expected");
}

void TestScreenEdges::testFullScreenBlocking()
{
    using namespace KWin;
    MockWorkspace ws;
    X11Client client(&ws);
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("Windows").writeEntry("ElectricBorderPushbackPixels", 1);
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->init();
    TestObject callback;
    QSignalSpy spy(&callback, &TestObject::gotCallback);
    QVERIFY(spy.isValid());
    s->reserve(KWin::ElectricLeft, &callback, "callback");
    s->reserve(KWin::ElectricBottomRight, &callback, "callback");
    QAction action;
    s->reserveTouch(KWin::ElectricRight, &action);
    // currently there is no active client yet, so check blocking shouldn't do anything
    emit s->checkBlocking();
    for (auto e: s->findChildren<Edge*>()) {
        QCOMPARE(e->activatesForTouchGesture(), e->border() == KWin::ElectricRight);
    }

    xcb_enter_notify_event_t event;
    Cursors::self()->mouse()->setPos(0, 50);
    event.root_x = 0;
    event.root_y = 50;
    event.event_x = 0;
    event.event_y = 50;
    event.root = XCB_WINDOW_NONE;
    event.child = XCB_WINDOW_NONE;
    event.event = s->windows().first();
    event.same_screen_focus = 1;
    event.time = QDateTime::currentMSecsSinceEpoch();
    auto isEntered = [s] (xcb_enter_notify_event_t *event) {
        return s->handleEnterNotifiy(event->event, QPoint(event->root_x, event->root_y), QDateTime::fromMSecsSinceEpoch(event->time));
    };
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));

    client.setFrameGeometry(screens()->geometry());
    client.setActive(true);
    client.setFullScreen(true);
    ws.setActiveClient(&client);
    emit s->checkBlocking();
    // the signal doesn't trigger for corners, let's go over all windows just to be sure that it doesn't call for corners
    for (auto e: s->findChildren<Edge*>()) {
        e->checkBlocking();
        QCOMPARE(e->activatesForTouchGesture(), false);
    }
    // calling again should not trigger
    QTest::qWait(160);
    Cursors::self()->mouse()->setPos(0, 50);
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    // and no pushback
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(0, 50));

    // let's make the client not fullscreen, which should trigger
    client.setFullScreen(false);
    emit s->checkBlocking();
    for (auto e: s->findChildren<Edge*>()) {
        QCOMPARE(e->activatesForTouchGesture(), e->border() == KWin::ElectricRight);
    }
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(!spy.isEmpty());
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));

    // let's make the client fullscreen again, but with a geometry not intersecting the left edge
    QTest::qWait(351);
    client.setFullScreen(true);
    client.setFrameGeometry(client.frameGeometry().translated(10, 0));
    emit s->checkBlocking();
    spy.clear();
    Cursors::self()->mouse()->setPos(0, 50);
    event.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    // and a pushback
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));

    // just to be sure, let's set geometry back
    client.setFrameGeometry(screens()->geometry());
    emit s->checkBlocking();
    Cursors::self()->mouse()->setPos(0, 50);
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    // and no pushback
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(0, 50));

    // the corner should always trigger
    s->unreserve(KWin::ElectricLeft, &callback);
    event.event_x = 99;
    event.event_y = 99;
    event.root_x = 99;
    event.root_y = 99;
    event.event = s->windows().first();
    event.time = QDateTime::currentMSecsSinceEpoch();
    Cursors::self()->mouse()->setPos(99, 99);
    QVERIFY(isEntered(&event));
    QVERIFY(spy.isEmpty());
    // and pushback
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(98, 98));
    QTest::qWait(160);
    event.time = QDateTime::currentMSecsSinceEpoch();
    Cursors::self()->mouse()->setPos(99, 99);
    QVERIFY(isEntered(&event));
    QVERIFY(!spy.isEmpty());
}

void TestScreenEdges::testClientEdge()
{
    using namespace KWin;
    X11Client client(workspace());
    client.setFrameGeometry(QRect(10, 50, 10, 50));
    auto s = ScreenEdges::self();
    s->init();

    s->reserve(&client, KWin::ElectricBottom);

    QPointer<Edge> edge = s->findChildren<Edge*>().last();

    QCOMPARE(edge->isReserved(), true);
    QCOMPARE(edge->activatesForPointer(), true);
    QCOMPARE(edge->activatesForTouchGesture(), true);

    //remove old reserves and resize to be in the middle of the screen
    s->reserve(&client, KWin::ElectricNone);
    client.setFrameGeometry(QRect(2, 2, 20, 20));

    // for none of the edges it should be able to be set
    for (int i = 0; i < ELECTRIC_COUNT; ++i) {
        client.setHiddenInternal(true);
        s->reserve(&client, static_cast<ElectricBorder>(i));
        QCOMPARE(client.isHiddenInternal(), false);
    }

    // now let's try to set it and activate it
    client.setFrameGeometry(screens()->geometry());
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricLeft);
    QCOMPARE(client.isHiddenInternal(), true);

    xcb_enter_notify_event_t event;
    Cursors::self()->mouse()->setPos(0, 50);
    event.root_x = 0;
    event.root_y = 50;
    event.event_x = 0;
    event.event_y = 50;
    event.root = XCB_WINDOW_NONE;
    event.child = XCB_WINDOW_NONE;
    event.event = s->windows().first();
    event.same_screen_focus = 1;
    event.time = QDateTime::currentMSecsSinceEpoch();
    auto isEntered = [s] (xcb_enter_notify_event_t *event) {
        return s->handleEnterNotifiy(event->event, QPoint(event->root_x, event->root_y), QDateTime::fromMSecsSinceEpoch(event->time));
    };
    QVERIFY(isEntered(&event));
    // autohiding panels shall activate instantly
    QCOMPARE(client.isHiddenInternal(), false);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));

    // now let's reserve the client for each of the edges, in the end for the right one
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricTop);
    s->reserve(&client, KWin::ElectricBottom);
    QCOMPARE(client.isHiddenInternal(), true);
    // corners shouldn't get reserved
    s->reserve(&client, KWin::ElectricTopLeft);
    QCOMPARE(client.isHiddenInternal(), false);
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricTopRight);
    QCOMPARE(client.isHiddenInternal(), false);
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricBottomRight);
    QCOMPARE(client.isHiddenInternal(), false);
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricBottomLeft);
    QCOMPARE(client.isHiddenInternal(), false);
    // now finally reserve on right one
    client.setHiddenInternal(true);
    s->reserve(&client, KWin::ElectricRight);
    QCOMPARE(client.isHiddenInternal(), true);

    // now let's emulate the removal of a Client through Workspace
    emit workspace()->clientRemoved(&client);
    for (auto e : s->findChildren<Edge*>()) {
        QVERIFY(!e->client());
    }
    QCOMPARE(client.isHiddenInternal(), true);

    // now let's try to trigger the client showing with the check method instead of enter notify
    s->reserve(&client, KWin::ElectricTop);
    QCOMPARE(client.isHiddenInternal(), true);
    Cursors::self()->mouse()->setPos(50, 0);
    s->check(QPoint(50, 0), QDateTime::currentDateTimeUtc());
    QCOMPARE(client.isHiddenInternal(), false);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(50, 1));

    // unreserve by setting to none edge
    s->reserve(&client, KWin::ElectricNone);
    // check on previous edge again, should fail
    client.setHiddenInternal(true);
    Cursors::self()->mouse()->setPos(50, 0);
    s->check(QPoint(50, 0), QDateTime::currentDateTimeUtc());
    QCOMPARE(client.isHiddenInternal(), true);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(50, 0));

    // set to windows can cover
    client.setFrameGeometry(screens()->geometry());
    client.setHiddenInternal(false);
    client.setKeepBelow(true);
    s->reserve(&client, KWin::ElectricLeft);
    QCOMPARE(client.keepBelow(), true);
    QCOMPARE(client.isHiddenInternal(), false);

    xcb_enter_notify_event_t event2;
    Cursors::self()->mouse()->setPos(0, 50);
    event2.root_x = 0;
    event2.root_y = 50;
    event2.event_x = 0;
    event2.event_y = 50;
    event2.root = XCB_WINDOW_NONE;
    event2.child = XCB_WINDOW_NONE;
    event2.event = s->windows().first();
    event2.same_screen_focus = 1;
    event2.time = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(isEntered(&event2));
    QCOMPARE(client.keepBelow(), false);
    QCOMPARE(client.isHiddenInternal(), false);
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 50));
}

void TestScreenEdges::testTouchEdge()
{
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");
    using namespace KWin;
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    auto group = config->group("TouchEdges");
    group.writeEntry("Top", "krunner");
    group.writeEntry("Left", "krunner");
    group.writeEntry("Bottom", "krunner");
    group.writeEntry("Right", "krunner");
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->init();
    // we don't have multiple desktops, so it's returning false
    QCOMPARE(s->isDesktopSwitching(), false);
    QCOMPARE(s->isDesktopSwitchingMovingClients(), false);
    QCOMPARE(s->actionTopLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTop(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionTopRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomRight(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottom(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionBottomLeft(), ElectricBorderAction::ElectricActionNone);
    QCOMPARE(s->actionLeft(), ElectricBorderAction::ElectricActionNone);

    QList<Edge*> edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), e->isScreenEdge());
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), e->isScreenEdge());
    }

    // try to activate the edge through pointer, should not be possible
    auto it = std::find_if(edges.constBegin(), edges.constEnd(), [](Edge *e) {
        return e->isScreenEdge() && e->isLeft();
    });
    QVERIFY(it != edges.constEnd());

    QSignalSpy approachingSpy(s, &ScreenEdges::approaching);
    QVERIFY(approachingSpy.isValid());

    xcb_enter_notify_event_t event;
    auto setPos = [&event] (const QPoint &pos) {
        Cursors::self()->mouse()->setPos(pos);
        event.root_x = pos.x();
        event.root_y = pos.y();
        event.event_x = pos.x();
        event.event_y = pos.y();
    };
    event.root = XCB_WINDOW_NONE;
    event.child = XCB_WINDOW_NONE;
    event.event = (*it)->window();
    event.same_screen_focus = 1;
    event.time = QDateTime::currentMSecsSinceEpoch();
    setPos(QPoint(0, 50));
    auto isEntered = [s] (xcb_enter_notify_event_t *event) {
        return s->handleEnterNotifiy(event->event, QPoint(event->root_x, event->root_y), QDateTime::fromMSecsSinceEpoch(event->time, Qt::UTC));
    };
    QCOMPARE(isEntered(&event), false);
    QVERIFY(approachingSpy.isEmpty());
    // let's also verify the check
    s->check(QPoint(0, 50), QDateTime::currentDateTimeUtc(), false);
    QVERIFY(approachingSpy.isEmpty());

    s->gestureRecognizer()->startSwipeGesture(QPoint(0, 50));
    QCOMPARE(approachingSpy.count(), 1);
    s->gestureRecognizer()->cancelSwipeGesture();
    QCOMPARE(approachingSpy.count(), 2);

    // let's reconfigure
    group.writeEntry("Top", "none");
    group.writeEntry("Left", "none");
    group.writeEntry("Bottom", "none");
    group.writeEntry("Right", "none");
    config->sync();
    s->reconfigure();

    edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), false);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }

}

void TestScreenEdges::testTouchCallback_data()
{
    QTest::addColumn<KWin::ElectricBorder>("border");
    QTest::addColumn<QPoint>("startPos");
    QTest::addColumn<QSizeF>("delta");

    QTest::newRow("left") << KWin::ElectricLeft << QPoint(0, 50) << QSizeF(250, 20);
    QTest::newRow("top") << KWin::ElectricTop << QPoint(50, 0) << QSizeF(20, 250);
    QTest::newRow("right") << KWin::ElectricRight << QPoint(99, 50) << QSizeF(-200, 0);
    QTest::newRow("bottom") << KWin::ElectricBottom << QPoint(50, 99) << QSizeF(0, -200);
}

void TestScreenEdges::testTouchCallback()
{
    qRegisterMetaType<KWin::ElectricBorder>("ElectricBorder");
    using namespace KWin;
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    auto group = config->group("TouchEdges");
    group.writeEntry("Top", "none");
    group.writeEntry("Left", "none");
    group.writeEntry("Bottom", "none");
    group.writeEntry("Right", "none");
    config->sync();

    auto s = ScreenEdges::self();
    s->setConfig(config);
    s->init();

    // none of our actions should be reserved
    const QList<Edge*> edges = s->findChildren<Edge*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(edges.size(), 8);
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), false);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }

    // let's reserve an action
    QAction action;
    QSignalSpy actionTriggeredSpy(&action, &QAction::triggered);
    QVERIFY(actionTriggeredSpy.isValid());
    QSignalSpy approachingSpy(s, &ScreenEdges::approaching);
    QVERIFY(approachingSpy.isValid());

    // reserve on edge
    QFETCH(KWin::ElectricBorder, border);
    s->reserveTouch(border, &action);
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), e->border() == border);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), e->border() == border);
    }

    QVERIFY(approachingSpy.isEmpty());
    QFETCH(QPoint, startPos);
    QCOMPARE(s->gestureRecognizer()->startSwipeGesture(startPos), 1);
    QVERIFY(actionTriggeredSpy.isEmpty());
    QCOMPARE(approachingSpy.count(), 1);
    QFETCH(QSizeF, delta);
    s->gestureRecognizer()->updateSwipeGesture(delta);
    QCOMPARE(approachingSpy.count(), 2);
    QVERIFY(actionTriggeredSpy.isEmpty());
    s->gestureRecognizer()->endSwipeGesture();
    QVERIFY(actionTriggeredSpy.wait());
    QCOMPARE(actionTriggeredSpy.count(), 1);
    QCOMPARE(approachingSpy.count(), 3);

    // unreserve again
    s->unreserveTouch(border, &action);
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), false);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }

    // reserve another action
    QScopedPointer<QAction> action2(new QAction);
    s->reserveTouch(border, action2.data());
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), e->border() == border);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), e->border() == border);
    }
    // and unreserve by destroying
    action2.reset();
    for (auto e : edges) {
        QCOMPARE(e->isReserved(), false);
        QCOMPARE(e->activatesForPointer(), false);
        QCOMPARE(e->activatesForTouchGesture(), false);
    }
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestScreenEdges)
#include "test_screen_edges.moc"
