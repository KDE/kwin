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
#include "keyboard_input.h"
#include "pointer_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_window_selection-0");

class TestWindowSelection : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testSelectOnWindowPointer();
    void testSelectOnWindowKeyboard_data();
    void testSelectOnWindowKeyboard();
    void testSelectOnWindowTouch();
    void testCancelOnWindowPointer();
    void testCancelOnWindowKeyboard();

    void testSelectPointPointer();
    void testSelectPointTouch();
};

void TestWindowSelection::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestWindowSelection::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void TestWindowSelection::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestWindowSelection::testSelectOnWindowPointer()
{
    // this test verifies window selection through pointer works
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardLeftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), window);
    QVERIFY(pointerEnteredSpy.wait());

    Window *selectedWindow = nullptr;
    auto callback = [&selectedWindow](Window *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(pointerLeftSpy.wait());
    if (keyboardLeftSpy.isEmpty()) {
        QVERIFY(keyboardLeftSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // simulate left button press
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QVERIFY(!input()->pointer()->focus());

    // updating the pointer should not change anything
    input()->pointer()->update();
    QVERIFY(!input()->pointer()->focus());
    // updating keyboard should also not change
    input()->keyboard()->update();

    // perform a right button click
    Test::pointerButtonPressed(BTN_RIGHT, timestamp++);
    Test::pointerButtonReleased(BTN_RIGHT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    // now release
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, window);
    QCOMPARE(input()->pointer()->focus(), window);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 2);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
}

void TestWindowSelection::testSelectOnWindowKeyboard_data()
{
    QTest::addColumn<qint32>("key");

    QTest::newRow("enter") << KEY_ENTER;
    QTest::newRow("keypad enter") << KEY_KPENTER;
    QTest::newRow("space") << KEY_SPACE;
}

void TestWindowSelection::testSelectOnWindowKeyboard()
{
    // this test verifies window selection through keyboard key
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardLeftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(keyboardEnteredSpy.wait());
    QVERIFY(!window->frameGeometry().contains(KWin::Cursors::self()->mouse()->pos()));

    Window *selectedWindow = nullptr;
    auto callback = [&selectedWindow](Window *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(keyboardLeftSpy.wait());
    QCOMPARE(pointerLeftSpy.count(), 0);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // simulate key press
    quint32 timestamp = 0;
    // move cursor through keys
    auto keyPress = [&timestamp](qint32 key) {
        Test::keyboardKeyPressed(key, timestamp++);
        Test::keyboardKeyReleased(key, timestamp++);
    };
    while (KWin::Cursors::self()->mouse()->pos().x() >= window->frameGeometry().x() + window->frameGeometry().width()) {
        keyPress(KEY_LEFT);
    }
    while (KWin::Cursors::self()->mouse()->pos().x() <= window->frameGeometry().x()) {
        keyPress(KEY_RIGHT);
    }
    while (KWin::Cursors::self()->mouse()->pos().y() <= window->frameGeometry().y()) {
        keyPress(KEY_DOWN);
    }
    while (KWin::Cursors::self()->mouse()->pos().y() >= window->frameGeometry().y() + window->frameGeometry().height()) {
        keyPress(KEY_UP);
    }
    QFETCH(qint32, key);
    Test::keyboardKeyPressed(key, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, window);
    QCOMPARE(input()->pointer()->focus(), window);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 0);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
    Test::keyboardKeyReleased(key, timestamp++);
}

void TestWindowSelection::testSelectOnWindowTouch()
{
    // this test verifies window selection through touch
    std::unique_ptr<KWayland::Client::Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy touchStartedSpy(touch.get(), &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy touchCanceledSpy(touch.get(), &KWayland::Client::Touch::sequenceCanceled);
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    Window *selectedWindow = nullptr;
    auto callback = [&selectedWindow](Window *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);

    // simulate touch down
    quint32 timestamp = 0;
    Test::touchDown(0, window->frameGeometry().center(), timestamp++);
    QVERIFY(!selectedWindow);
    Test::touchUp(0, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, window);

    // with movement
    selectedWindow = nullptr;
    kwinApp()->startInteractiveWindowSelection(callback);
    Test::touchDown(0, window->frameGeometry().bottomRight() + QPoint(20, 20), timestamp++);
    QVERIFY(!selectedWindow);
    Test::touchMotion(0, window->frameGeometry().bottomRight() - QPoint(1, 1), timestamp++);
    QVERIFY(!selectedWindow);
    Test::touchUp(0, timestamp++);
    QCOMPARE(selectedWindow, window);
    QCOMPARE(input()->isSelectingWindow(), false);

    // it cancels active touch sequence on the window
    Test::touchDown(0, window->frameGeometry().center(), timestamp++);
    QVERIFY(touchStartedSpy.wait());
    selectedWindow = nullptr;
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(touchCanceledSpy.wait());
    QVERIFY(!selectedWindow);
    // this touch up does not yet select the window, it was started prior to the selection
    Test::touchUp(0, timestamp++);
    QVERIFY(!selectedWindow);
    Test::touchDown(0, window->frameGeometry().center(), timestamp++);
    Test::touchUp(0, timestamp++);
    QCOMPARE(selectedWindow, window);
    QCOMPARE(input()->isSelectingWindow(), false);

    QCOMPARE(touchStartedSpy.count(), 1);
    QCOMPARE(touchCanceledSpy.count(), 1);
}

void TestWindowSelection::testCancelOnWindowPointer()
{
    // this test verifies that window selection cancels through right button click
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardLeftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), window);
    QVERIFY(pointerEnteredSpy.wait());

    Window *selectedWindow = nullptr;
    auto callback = [&selectedWindow](Window *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(pointerLeftSpy.wait());
    if (keyboardLeftSpy.isEmpty()) {
        QVERIFY(keyboardLeftSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // simulate left button press
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_RIGHT, timestamp++);
    Test::pointerButtonReleased(BTN_RIGHT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QVERIFY(!selectedWindow);
    QCOMPARE(input()->pointer()->focus(), window);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 2);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
}

void TestWindowSelection::testCancelOnWindowKeyboard()
{
    // this test verifies that cancel window selection through escape key works
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardLeftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), window);
    QVERIFY(pointerEnteredSpy.wait());

    Window *selectedWindow = nullptr;
    auto callback = [&selectedWindow](Window *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(pointerLeftSpy.wait());
    if (keyboardLeftSpy.isEmpty()) {
        QVERIFY(keyboardLeftSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // simulate left button press
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_ESC, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QVERIFY(!selectedWindow);
    QCOMPARE(input()->pointer()->focus(), window);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 2);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
    Test::keyboardKeyReleased(KEY_ESC, timestamp++);
}

void TestWindowSelection::testSelectPointPointer()
{
    // this test verifies point selection through pointer works
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy keyboardEnteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy keyboardLeftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), window);
    QVERIFY(pointerEnteredSpy.wait());

    QPoint point;
    auto callback = [&point](const QPoint &p) {
        point = p;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractivePositionSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(pointerLeftSpy.wait());
    if (keyboardLeftSpy.isEmpty()) {
        QVERIFY(keyboardLeftSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // trying again should not be allowed
    QPoint point2;
    kwinApp()->startInteractivePositionSelection([&point2](const QPoint &p) {
        point2 = p;
    });
    QCOMPARE(point2, QPoint(-1, -1));

    // simulate left button press
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());
    QVERIFY(!input()->pointer()->focus());

    // updating the pointer should not change anything
    input()->pointer()->update();
    QVERIFY(!input()->pointer()->focus());
    // updating keyboard should also not change
    input()->keyboard()->update();

    // perform a right button click
    Test::pointerButtonPressed(BTN_RIGHT, timestamp++);
    Test::pointerButtonReleased(BTN_RIGHT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());
    // now release
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(point, input()->globalPointer().toPoint());
    QCOMPARE(input()->pointer()->focus(), window);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 2);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
}

void TestWindowSelection::testSelectPointTouch()
{
    // this test verifies point selection through touch works
    QPoint point;
    auto callback = [&point](const QPoint &p) {
        point = p;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->startInteractivePositionSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());

    // let's create multiple touch points
    quint32 timestamp = 0;
    Test::touchDown(0, QPointF(0, 1), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    Test::touchDown(1, QPointF(10, 20), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    Test::touchDown(2, QPointF(30, 40), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);

    // let's move our points
    Test::touchMotion(0, QPointF(5, 10), timestamp++);
    Test::touchMotion(2, QPointF(20, 25), timestamp++);
    Test::touchMotion(1, QPointF(25, 35), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    Test::touchUp(0, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    Test::touchUp(2, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    Test::touchUp(1, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(point, QPoint(25, 35));
}

WAYLANDTEST_MAIN(TestWindowSelection)
#include "window_selection_test.moc"
