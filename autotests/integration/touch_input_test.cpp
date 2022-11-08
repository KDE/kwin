/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "touch_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <QAction>

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
    std::pair<Window *, std::unique_ptr<KWayland::Client::Surface>> showWindow(bool decorated = false);
    KWayland::Client::Touch *m_touch = nullptr;
};

void TouchInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TouchInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::XdgDecorationV1));
    QVERIFY(Test::waitForWaylandTouch());
    m_touch = Test::waylandSeat()->createTouch(Test::waylandSeat());
    QVERIFY(m_touch);
    QVERIFY(m_touch->isValid());

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TouchInputTest::cleanup()
{
    delete m_touch;
    m_touch = nullptr;
    Test::destroyWaylandConnection();
}

std::pair<Window *, std::unique_ptr<KWayland::Client::Surface>> TouchInputTest::showWindow(bool decorated)
{
#define VERIFY(statement)                                                 \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) \
        return {nullptr, nullptr};
#define COMPARE(actual, expected)                                                   \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
        return {nullptr, nullptr};

    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    VERIFY(surface.get());
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly, surface.get());
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
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    VERIFY(window);
    COMPARE(workspace()->activeWindow(), window);

#undef VERIFY
#undef COMPARE

    return {window, std::move(surface)};
}

void TouchInputTest::testTouchHidesCursor()
{
    QCOMPARE(Cursors::self()->isCursorHidden(), false);
    quint32 timestamp = 1;
    Test::touchDown(1, QPointF(125, 125), timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Test::touchDown(2, QPointF(130, 125), timestamp++);
    Test::touchUp(2, timestamp++);
    Test::touchUp(1, timestamp++);

    // now a mouse event should show the cursor again
    Test::pointerMotion(QPointF(0, 0), timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), false);

    // touch should hide again
    Test::touchDown(1, QPointF(125, 125), timestamp++);
    Test::touchUp(1, timestamp++);
    QCOMPARE(Cursors::self()->isCursorHidden(), true);

    // wheel should also show
    Test::pointerAxisVertical(1.0, timestamp++);
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
    QFETCH(bool, decorated);
    auto [window, surface] = showWindow(decorated);
    QCOMPARE(window->isDecorated(), decorated);
    window->move(QPoint(100, 100));
    QVERIFY(window);
    QSignalSpy sequenceStartedSpy(m_touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy pointAddedSpy(m_touch, &KWayland::Client::Touch::pointAdded);
    QSignalSpy pointMovedSpy(m_touch, &KWayland::Client::Touch::pointMoved);
    QSignalSpy pointRemovedSpy(m_touch, &KWayland::Client::Touch::pointRemoved);
    QSignalSpy endedSpy(m_touch, &KWayland::Client::Touch::sequenceEnded);

    quint32 timestamp = 1;
    Test::touchDown(1, window->mapFromLocal(QPointF(25, 25)), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 1);
    QCOMPARE(m_touch->sequence().first()->isDown(), true);
    QCOMPARE(m_touch->sequence().first()->position(), QPointF(25, 25));
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);

    // a point outside the window
    Test::touchDown(2, window->mapFromLocal(QPointF(-100, -100)), timestamp++);
    QVERIFY(pointAddedSpy.wait());
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(-100, -100));
    QCOMPARE(pointMovedSpy.count(), 0);

    // let's move that one
    Test::touchMotion(2, window->mapFromLocal(QPointF(0, 0)), timestamp++);
    QVERIFY(pointMovedSpy.wait());
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(0, 0));

    Test::touchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(endedSpy.count(), 0);

    Test::touchUp(2, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 2);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), false);
    QCOMPARE(endedSpy.count(), 1);
}

void TouchInputTest::testCancel()
{
    auto [window, surface] = showWindow();
    window->move(QPoint(100, 100));
    QVERIFY(window);
    QSignalSpy sequenceStartedSpy(m_touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy cancelSpy(m_touch, &KWayland::Client::Touch::sequenceCanceled);
    QSignalSpy pointRemovedSpy(m_touch, &KWayland::Client::Touch::pointRemoved);

    quint32 timestamp = 1;
    Test::touchDown(1, QPointF(125, 125), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cancel
    Test::touchCancel();
    QVERIFY(cancelSpy.wait());
    QCOMPARE(cancelSpy.count(), 1);
}

void TouchInputTest::testTouchMouseAction()
{
    // this test verifies that a touch down on an inactive window will activate it

    // create two windows
    auto [c1, surface] = showWindow();
    QVERIFY(c1);
    auto [c2, surface2] = showWindow();
    QVERIFY(c2);

    QVERIFY(!c1->isActive());
    QVERIFY(c2->isActive());

    // also create a sequence started spy as the touch event should be passed through
    QSignalSpy sequenceStartedSpy(m_touch, &KWayland::Client::Touch::sequenceStarted);

    quint32 timestamp = 1;
    Test::touchDown(1, c1->frameGeometry().center(), timestamp++);
    QVERIFY(c1->isActive());

    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cleanup
    input()->touch()->cancel();
}

void TouchInputTest::testTouchPointCount()
{
    QCOMPARE(input()->touch()->touchPointCount(), 0);
    quint32 timestamp = 1;
    Test::touchDown(0, QPointF(125, 125), timestamp++);
    Test::touchDown(1, QPointF(125, 125), timestamp++);
    Test::touchDown(2, QPointF(125, 125), timestamp++);
    QCOMPARE(input()->touch()->touchPointCount(), 3);

    Test::touchUp(1, timestamp++);
    QCOMPARE(input()->touch()->touchPointCount(), 2);

    input()->touch()->cancel();
    QCOMPARE(input()->touch()->touchPointCount(), 0);
}

void TouchInputTest::testUpdateFocusOnDecorationDestroy()
{
    // This test verifies that a maximized window gets it's touch focus
    // if decoration was focused and then destroyed on maximize with BorderlessMaximizedWindows option.

    QSignalSpy sequenceEndedSpy(m_touch, &KWayland::Client::Touch::sequenceEnded);

    // Enable the borderless maximized windows option.
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
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
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
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
    Test::touchDown(1, window->frameGeometry().topLeft(), timestamp++);
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
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->isDecorated(), false);

    // Window should have focus
    QVERIFY(!input()->touch()->decoration());
    Test::touchUp(1, timestamp++);
    QVERIFY(!sequenceEndedSpy.wait(100));
    Test::touchDown(2, window->frameGeometry().center(), timestamp++);
    Test::touchUp(2, timestamp++);
    QVERIFY(sequenceEndedSpy.wait());

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void TouchInputTest::testGestureDetection()
{
    bool callbackTriggered = false;
    const auto callback = [&callbackTriggered](float progress) {
        callbackTriggered = true;
        qWarning() << "progress callback!" << progress;
    };
    QAction action;
    input()->forceRegisterTouchscreenSwipeShortcut(SwipeDirection::Right, 3, &action, callback);

    // verify that gestures are detected

    quint32 timestamp = 1;
    Test::touchDown(0, QPointF(500, 125), timestamp++);
    Test::touchDown(1, QPointF(500, 125), timestamp++);
    Test::touchDown(2, QPointF(500, 125), timestamp++);

    Test::touchMotion(0, QPointF(100, 125), timestamp++);
    QVERIFY(callbackTriggered);

    // verify that gestures are canceled properly
    QSignalSpy gestureCancelled(&action, &QAction::triggered);
    Test::touchUp(0, timestamp++);
    QVERIFY(gestureCancelled.wait());

    Test::touchUp(1, timestamp++);
    Test::touchUp(2, timestamp++);

    callbackTriggered = false;

    // verify that touch points too far apart don't trigger a gesture
    Test::touchDown(0, QPointF(125, 125), timestamp++);
    Test::touchDown(1, QPointF(10000, 125), timestamp++);
    Test::touchDown(2, QPointF(125, 125), timestamp++);
    QVERIFY(!callbackTriggered);

    Test::touchUp(0, timestamp++);
    Test::touchUp(1, timestamp++);
    Test::touchUp(2, timestamp++);

    // verify that touch points triggered too slow don't trigger a gesture
    Test::touchDown(0, QPointF(125, 125), timestamp++);
    timestamp += 1000;
    Test::touchDown(1, QPointF(125, 125), timestamp++);
    Test::touchDown(2, QPointF(125, 125), timestamp++);
    QVERIFY(!callbackTriggered);

    Test::touchUp(0, timestamp++);
    Test::touchUp(1, timestamp++);
    Test::touchUp(2, timestamp++);

    // verify that after a gesture has been canceled but never initiated, gestures still work
    Test::touchDown(0, QPointF(500, 125), timestamp++);
    Test::touchDown(1, QPointF(500, 125), timestamp++);
    Test::touchDown(2, QPointF(500, 125), timestamp++);

    Test::touchMotion(0, QPointF(100, 125), timestamp++);
    Test::touchMotion(1, QPointF(100, 125), timestamp++);
    Test::touchMotion(2, QPointF(100, 125), timestamp++);
    QVERIFY(callbackTriggered);

    Test::touchUp(0, timestamp++);
    Test::touchUp(1, timestamp++);
    Test::touchUp(2, timestamp++);
}
}

WAYLANDTEST_MAIN(KWin::TouchInputTest)
#include "touch_input_test.moc"
