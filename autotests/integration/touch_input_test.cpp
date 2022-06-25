/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "cursor.h"
#include "output.h"
#include "platform.h"
#include "touch_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/seat.h>
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
    void testTouchPointCount();
    void testUpdateFocusOnDecorationDestroy();
    void testGestureDetection();

private:
    Window *showWindow(bool decorated = false);
};

void TouchInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
}

void TouchInputTest::init()
{
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::XdgDecorationV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TouchInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

Window *TouchInputTest::showWindow(bool decorated)
{
    using namespace KWayland::Client;
#define VERIFY(statement)                                                 \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) \
        return nullptr;
#define COMPARE(actual, expected)                                                   \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
        return nullptr;

    KWayland::Client::Surface *surface = Test::createSurface(Test::waylandCompositor());
    VERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface, Test::CreationSetup::CreateOnly, surface);
    VERIFY(shellSurface);
    if (decorated) {
        auto decoration = Test::createXdgToplevelDecorationV1(shellSurface, shellSurface);
        decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    }
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    VERIFY(surfaceConfigureRequestedSpy.wait());
    // let's render
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface, QSize(100, 50), Qt::blue);

    VERIFY(window);
    COMPARE(workspace()->activeWindow(), window);

#undef VERIFY
#undef COMPARE

    return window;
}

void TouchInputTest::testTouchHidesCursor()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();
    QVERIFY(Test::waitForWaylandTouch());
    std::unique_ptr<Test::VirtualInputDevice> pointerDevice = Test::createPointerDevice();
    QVERIFY(Test::waitForWaylandPointer());

    QCOMPARE(Cursors::self()->isCursorHidden(), false);
    quint32 timestamp = 1;
    touchDevice->sendTouchDown(1, QPointF(125, 125), timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    touchDevice->sendTouchDown(2, QPointF(130, 125), timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);
    touchDevice->sendTouchUp(1, timestamp++);

    // now a mouse event should show the cursor again
    pointerDevice->sendPointerMotion(QPointF(0, 0), timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), false);

    // touch should hide again
    touchDevice->sendTouchDown(1, QPointF(125, 125), timestamp++);
    touchDevice->sendTouchUp(1, timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), true);

    // wheel should also show
    pointerDevice->sendPointerAxisVertical(1.0, timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), false);
}

void TouchInputTest::testMultipleTouchPoints_data()
{
    QTest::addColumn<bool>("decorated");

    QTest::newRow("undecorated") << false;
    QTest::newRow("decorated") << true;
}

void TouchInputTest::testMultipleTouchPoints()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();
    QVERIFY(Test::waitForWaylandTouch());

    using namespace KWayland::Client;
    QFETCH(bool, decorated);
    Window *window = showWindow(decorated);
    QCOMPARE(window->isDecorated(), decorated);
    window->move(QPoint(100, 100));
    QVERIFY(window);

    std::unique_ptr<KWayland::Client::Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy sequenceStartedSpy(touch.get(), &Touch::sequenceStarted);
    QSignalSpy pointAddedSpy(touch.get(), &Touch::pointAdded);
    QSignalSpy pointMovedSpy(touch.get(), &Touch::pointMoved);
    QSignalSpy pointRemovedSpy(touch.get(), &Touch::pointRemoved);
    QSignalSpy endedSpy(touch.get(), &Touch::sequenceEnded);

    quint32 timestamp = 1;
    touchDevice->sendTouchDown(1, QPointF(125, 125) + window->clientPos(), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(touch->sequence().count(), 1);
    QCOMPARE(touch->sequence().first()->isDown(), true);
    QCOMPARE(touch->sequence().first()->position(), QPointF(25, 25));
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);

    // a point outside the window
    touchDevice->sendTouchDown(2, QPointF(0, 0) + window->clientPos(), timestamp++);
    QVERIFY(pointAddedSpy.wait());
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().at(1)->isDown(), true);
    QCOMPARE(touch->sequence().at(1)->position(), QPointF(-100, -100));
    QCOMPARE(pointMovedSpy.count(), 0);

    // let's move that one
    touchDevice->sendTouchMotion(2, QPointF(100, 100) + window->clientPos(), timestamp++);
    QVERIFY(pointMovedSpy.wait());
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().at(1)->isDown(), true);
    QCOMPARE(touch->sequence().at(1)->position(), QPointF(0, 0));

    touchDevice->sendTouchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().first()->isDown(), false);
    QCOMPARE(endedSpy.count(), 0);

    touchDevice->sendTouchUp(2, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 2);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().first()->isDown(), false);
    QCOMPARE(touch->sequence().at(1)->isDown(), false);
    QCOMPARE(endedSpy.count(), 1);
}

void TouchInputTest::testCancel()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();

    using namespace KWayland::Client;
    Window *window = showWindow();
    window->move(QPoint(100, 100));
    QVERIFY(window);

    std::unique_ptr<KWayland::Client::Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy sequenceStartedSpy(touch.get(), &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy cancelSpy(touch.get(), &Touch::sequenceCanceled);
    QVERIFY(cancelSpy.isValid());
    QSignalSpy pointRemovedSpy(touch.get(), &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());

    quint32 timestamp = 1;
    touchDevice->sendTouchDown(1, QPointF(125, 125), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cancel
    touchDevice->sendTouchCancel();
    QVERIFY(cancelSpy.wait());
    QCOMPARE(cancelSpy.count(), 1);
}

void TouchInputTest::testTouchMouseAction()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();

    // this test verifies that a touch down on an inactive window will activate it
    using namespace KWayland::Client;
    // create two windows
    Window *c1 = showWindow();
    QVERIFY(c1);
    Window *c2 = showWindow();
    QVERIFY(c2);

    QVERIFY(!c1->isActive());
    QVERIFY(c2->isActive());

    // also create a sequence started spy as the touch event should be passed through
    std::unique_ptr<KWayland::Client::Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy sequenceStartedSpy(touch.get(), &Touch::sequenceStarted);

    quint32 timestamp = 1;
    touchDevice->sendTouchDown(1, c1->frameGeometry().center(), timestamp++);
    QVERIFY(c1->isActive());

    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cleanup
    input()->touch()->cancel();
}

void TouchInputTest::testTouchPointCount()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();
    QVERIFY(Test::waitForWaylandTouch());

    QCOMPARE(input()->touch()->touchPointCount(), 0);
    quint32 timestamp = 1;
    touchDevice->sendTouchDown(0, QPointF(125, 125), timestamp++);
    touchDevice->sendTouchDown(1, QPointF(125, 125), timestamp++);
    touchDevice->sendTouchDown(2, QPointF(125, 125), timestamp++);
    QCOMPARE(input()->touch()->touchPointCount(), 3);

    touchDevice->sendTouchUp(1, timestamp++);
    QCOMPARE(input()->touch()->touchPointCount(), 2);

    input()->touch()->cancel();
    QCOMPARE(input()->touch()->touchPointCount(), 0);
}

void TouchInputTest::testUpdateFocusOnDecorationDestroy()
{
    // This test verifies that a maximized window gets it's touch focus
    // if decoration was focused and then destroyed on maximize with BorderlessMaximizedWindows option.

    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();
    QVERIFY(Test::waitForWaylandTouch());

    std::unique_ptr<KWayland::Client::Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy sequenceEndedSpy(touch.get(), &KWayland::Client::Touch::sequenceEnded);

    // Enable the borderless maximized windows option.
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // Create the test window.
    QScopedPointer<KWayland::Client::Surface> surface(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.data(), Test::CreationSetup::CreateOnly));
    QScopedPointer<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.data()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.data(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy decorationConfigureRequestedSpy(decoration.data(), &Test::XdgToplevelDecorationV1::configureRequested);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->isDecorated(), true);

    // We should receive a configure event when the window becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Simulate decoration hover
    quint32 timestamp = 0;
    touchDevice->sendTouchDown(1, window->frameGeometry().topLeft(), timestamp++);
    QVERIFY(input()->touch()->decoration());

    // Maximize when on decoration
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    QVERIFY(frameGeometryChangedSpy.isValid());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->isDecorated(), false);

    // Window should have focus
    QVERIFY(!input()->touch()->decoration());
    touchDevice->sendTouchUp(1, timestamp++);
    QVERIFY(!sequenceEndedSpy.wait(100));
    touchDevice->sendTouchDown(2, window->frameGeometry().center(), timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);
    QVERIFY(sequenceEndedSpy.wait());

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void TouchInputTest::testGestureDetection()
{
    std::unique_ptr<Test::VirtualInputDevice> touchDevice = Test::createTouchDevice();

    bool callbackTriggered = false;
    const auto callback = [&callbackTriggered](float progress) {
        Q_UNUSED(progress);
        callbackTriggered = true;
        qWarning() << "progress callback!" << progress;
    };
    QAction action;
    input()->forceRegisterTouchscreenSwipeShortcut(SwipeDirection::Right, 3, &action, callback);

    // verify that gestures are detected

    quint32 timestamp = 1;
    touchDevice->sendTouchDown(0, QPointF(500, 125), timestamp++);
    touchDevice->sendTouchDown(1, QPointF(500, 125), timestamp++);
    touchDevice->sendTouchDown(2, QPointF(500, 125), timestamp++);

    touchDevice->sendTouchMotion(0, QPointF(100, 125), timestamp++);
    QVERIFY(callbackTriggered);

    // verify that gestures are canceled properly
    QSignalSpy gestureCancelled(&action, &QAction::triggered);
    QVERIFY(gestureCancelled.isValid());
    touchDevice->sendTouchUp(0, timestamp++);
    QVERIFY(gestureCancelled.wait());

    touchDevice->sendTouchUp(1, timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);

    callbackTriggered = false;

    // verify that touch points too far apart don't trigger a gesture
    touchDevice->sendTouchDown(0, QPointF(125, 125), timestamp++);
    touchDevice->sendTouchDown(1, QPointF(10000, 125), timestamp++);
    touchDevice->sendTouchDown(2, QPointF(125, 125), timestamp++);
    QVERIFY(!callbackTriggered);

    touchDevice->sendTouchUp(0, timestamp++);
    touchDevice->sendTouchUp(1, timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);

    // verify that touch points triggered too slow don't trigger a gesture
    touchDevice->sendTouchDown(0, QPointF(125, 125), timestamp++);
    timestamp += 1000;
    touchDevice->sendTouchDown(1, QPointF(125, 125), timestamp++);
    touchDevice->sendTouchDown(2, QPointF(125, 125), timestamp++);
    QVERIFY(!callbackTriggered);

    touchDevice->sendTouchUp(0, timestamp++);
    touchDevice->sendTouchUp(1, timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);

    // verify that after a gesture has been canceled but never initiated, gestures still work
    touchDevice->sendTouchDown(0, QPointF(500, 125), timestamp++);
    touchDevice->sendTouchDown(1, QPointF(500, 125), timestamp++);
    touchDevice->sendTouchDown(2, QPointF(500, 125), timestamp++);

    touchDevice->sendTouchMotion(0, QPointF(100, 125), timestamp++);
    touchDevice->sendTouchMotion(1, QPointF(100, 125), timestamp++);
    touchDevice->sendTouchMotion(2, QPointF(100, 125), timestamp++);
    QVERIFY(callbackTriggered);

    touchDevice->sendTouchUp(0, timestamp++);
    touchDevice->sendTouchUp(1, timestamp++);
    touchDevice->sendTouchUp(2, timestamp++);
}
}

WAYLANDTEST_MAIN(KWin::TouchInputTest)
#include "touch_input_test.moc"
