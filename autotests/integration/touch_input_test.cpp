/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "platform.h"
#include "cursor.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_touch_input-0");

class TouchInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testTouchHidesCursor();
    void testMultipleTouchPoints_data();
    void testMultipleTouchPoints();
    void testCancel();
    void testTouchMouseAction();

private:
    AbstractClient *showWindow(bool decorated = false);
    KWayland::Client::Touch *m_touch = nullptr;
};

void TouchInputTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TouchInputTest::init()
{
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::Decoration));
    QVERIFY(Test::waitForWaylandTouch());
    m_touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    QVERIFY(m_touch);
    QVERIFY(m_touch->isValid());

    screens()->setCurrent(0);
    Cursors::self()->mouse()->setPos(QPoint(1280, 512));
}

void TouchInputTest::cleanup()
{
    delete m_touch;
    m_touch = nullptr;
    Test::destroyWaylandConnection();
}

AbstractClient *TouchInputTest::showWindow(bool decorated)
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;

    Surface *surface = Test::createSurface(Test::waylandCompositor());
    VERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    VERIFY(shellSurface);
    if (decorated) {
        auto deco = Test::waylandServerSideDecoration()->create(surface, surface);
        QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
        VERIFY(decoSpy.isValid());
        VERIFY(decoSpy.wait());
        deco->requestMode(ServerSideDecoration::Mode::Server);
        VERIFY(decoSpy.wait());
        COMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    }
    // let's render
    auto c = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);

    VERIFY(c);
    COMPARE(workspace()->activeClient(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

void TouchInputTest::testTouchHidesCursor()
{
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(1, QPointF(125, 125), timestamp++);
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->touchDown(2, QPointF(130, 125), timestamp++);
    kwinApp()->platform()->touchUp(2, timestamp++);
    kwinApp()->platform()->touchUp(1, timestamp++);

    // now a mouse event should show the cursor again
    kwinApp()->platform()->pointerMotion(QPointF(0, 0), timestamp++);
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);

    // touch should hide again
    kwinApp()->platform()->touchDown(1, QPointF(125, 125), timestamp++);
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);

    // wheel should also show
    kwinApp()->platform()->pointerAxisVertical(1.0, timestamp++);
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
}

void TouchInputTest::testMultipleTouchPoints_data()
{
    QTest::addColumn<bool>("decorated");

    QTest::newRow("undecorated") << false;
    QTest::newRow("decorated") << true;
}

void TouchInputTest::testMultipleTouchPoints()
{
    using namespace KWayland::Client;
    QFETCH(bool, decorated);
    AbstractClient *c = showWindow(decorated);
    QCOMPARE(c->isDecorated(), decorated);
    c->move(100, 100);
    QVERIFY(c);
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy pointAddedSpy(m_touch, &Touch::pointAdded);
    QVERIFY(pointAddedSpy.isValid());
    QSignalSpy pointMovedSpy(m_touch, &Touch::pointMoved);
    QVERIFY(pointMovedSpy.isValid());
    QSignalSpy pointRemovedSpy(m_touch, &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());
    QSignalSpy endedSpy(m_touch, &Touch::sequenceEnded);
    QVERIFY(endedSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(1, QPointF(125, 125) + c->clientPos(), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 1);
    QCOMPARE(m_touch->sequence().first()->isDown(), true);
    QCOMPARE(m_touch->sequence().first()->position(), QPointF(25, 25));
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);

    // a point outside the window
    kwinApp()->platform()->touchDown(2, QPointF(0, 0) + c->clientPos(), timestamp++);
    QVERIFY(pointAddedSpy.wait());
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(-100, -100));
    QCOMPARE(pointMovedSpy.count(), 0);

    // let's move that one
    kwinApp()->platform()->touchMotion(2, QPointF(100, 100) + c->clientPos(), timestamp++);
    QVERIFY(pointMovedSpy.wait());
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(0, 0));

    kwinApp()->platform()->touchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(endedSpy.count(), 0);

    kwinApp()->platform()->touchUp(2, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 2);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), false);
    QCOMPARE(endedSpy.count(), 1);
}

void TouchInputTest::testCancel()
{
    using namespace KWayland::Client;
    AbstractClient *c = showWindow();
    c->move(100, 100);
    QVERIFY(c);
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy cancelSpy(m_touch, &Touch::sequenceCanceled);
    QVERIFY(cancelSpy.isValid());
    QSignalSpy pointRemovedSpy(m_touch, &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(1, QPointF(125, 125), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cancel
    kwinApp()->platform()->touchCancel();
    QVERIFY(cancelSpy.wait());
    QCOMPARE(cancelSpy.count(), 1);

    kwinApp()->platform()->touchUp(1, timestamp++);
    QVERIFY(!pointRemovedSpy.wait(100));
    QCOMPARE(pointRemovedSpy.count(), 0);
}

void TouchInputTest::testTouchMouseAction()
{
    // this test verifies that a touch down on an inactive client will activate it
    using namespace KWayland::Client;
    // create two windows
    AbstractClient *c1 = showWindow();
    QVERIFY(c1);
    AbstractClient *c2 = showWindow();
    QVERIFY(c2);

    QVERIFY(!c1->isActive());
    QVERIFY(c2->isActive());

    // also create a sequence started spy as the touch event should be passed through
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->touchDown(1, c1->frameGeometry().center(), timestamp++);
    QVERIFY(c1->isActive());

    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cleanup
    kwinApp()->platform()->touchCancel();
}

}

WAYLANDTEST_MAIN(KWin::TouchInputTest)
#include "touch_input_test.moc"
