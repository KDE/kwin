/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "abstract_client.h"
#include "cursor.h"
#include "keyboard_input.h"
#include "platform.h"
#include "pointer_input.h"
#include "screens.h"
#include "wayland_server.h"
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
using namespace KWayland::Client;

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
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestWindowSelection::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    screens()->setCurrent(0);
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));
}

void TestWindowSelection::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestWindowSelection::testSelectOnWindowPointer()
{
    // this test verifies window selection through pointer works
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(client->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), client);
    QVERIFY(pointerEnteredSpy.wait());

    Toplevel *selectedWindow = nullptr;
    auto callback = [&selectedWindow] (Toplevel *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
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
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
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
    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    // now release
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, client);
    QCOMPARE(input()->pointer()->focus(), client);
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
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(keyboardEnteredSpy.wait());
    QVERIFY(!client->frameGeometry().contains(KWin::Cursors::self()->mouse()->pos()));

    Toplevel *selectedWindow = nullptr;
    auto callback = [&selectedWindow] (Toplevel *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);
    QCOMPARE(keyboardLeftSpy.count(), 0);
    QVERIFY(keyboardLeftSpy.wait());
    QCOMPARE(pointerLeftSpy.count(), 0);
    QCOMPARE(keyboardLeftSpy.count(), 1);

    // simulate key press
    quint32 timestamp = 0;
    // move cursor through keys
    auto keyPress = [&timestamp] (qint32 key) {
        kwinApp()->platform()->keyboardKeyPressed(key, timestamp++);
        kwinApp()->platform()->keyboardKeyReleased(key, timestamp++);
    };
    while (KWin::Cursors::self()->mouse()->pos().x() >= client->frameGeometry().x() + client->frameGeometry().width()) {
        keyPress(KEY_LEFT);
    }
    while (KWin::Cursors::self()->mouse()->pos().x() <= client->frameGeometry().x()) {
        keyPress(KEY_RIGHT);
    }
    while (KWin::Cursors::self()->mouse()->pos().y() <= client->frameGeometry().y()) {
        keyPress(KEY_DOWN);
    }
    while (KWin::Cursors::self()->mouse()->pos().y() >= client->frameGeometry().y() + client->frameGeometry().height()) {
        keyPress(KEY_UP);
    }
    QFETCH(qint32, key);
    kwinApp()->platform()->keyboardKeyPressed(key, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, client);
    QCOMPARE(input()->pointer()->focus(), client);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 0);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
    kwinApp()->platform()->keyboardKeyReleased(key, timestamp++);
}

void TestWindowSelection::testSelectOnWindowTouch()
{
    // this test verifies window selection through touch
    QScopedPointer<Touch> touch(Test::waylandSeat()->createTouch());
    QSignalSpy touchStartedSpy(touch.data(), &Touch::sequenceStarted);
    QVERIFY(touchStartedSpy.isValid());
    QSignalSpy touchCanceledSpy(touch.data(), &Touch::sequenceCanceled);
    QVERIFY(touchCanceledSpy.isValid());
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);

    Toplevel *selectedWindow = nullptr;
    auto callback = [&selectedWindow] (Toplevel *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(!selectedWindow);

    // simulate touch down
    quint32 timestamp = 0;
    kwinApp()->platform()->touchDown(0, client->frameGeometry().center(), timestamp++);
    QVERIFY(!selectedWindow);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, client);

    // with movement
    selectedWindow = nullptr;
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
    kwinApp()->platform()->touchDown(0, client->frameGeometry().bottomRight() + QPoint(20, 20), timestamp++);
    QVERIFY(!selectedWindow);
    kwinApp()->platform()->touchMotion(0, client->frameGeometry().bottomRight() - QPoint(1, 1), timestamp++);
    QVERIFY(!selectedWindow);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(selectedWindow, client);
    QCOMPARE(input()->isSelectingWindow(), false);

    // it cancels active touch sequence on the window
    kwinApp()->platform()->touchDown(0, client->frameGeometry().center(), timestamp++);
    QVERIFY(touchStartedSpy.wait());
    selectedWindow = nullptr;
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QVERIFY(touchCanceledSpy.wait());
    QVERIFY(!selectedWindow);
    // this touch up does not yet select the window, it was started prior to the selection
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(!selectedWindow);
    kwinApp()->platform()->touchDown(0, client->frameGeometry().center(), timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(selectedWindow, client);
    QCOMPARE(input()->isSelectingWindow(), false);

    QCOMPARE(touchStartedSpy.count(), 1);
    QCOMPARE(touchCanceledSpy.count(), 1);
}

void TestWindowSelection::testCancelOnWindowPointer()
{
    // this test verifies that window selection cancels through right button click
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(client->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), client);
    QVERIFY(pointerEnteredSpy.wait());

    Toplevel *selectedWindow = nullptr;
    auto callback = [&selectedWindow] (Toplevel *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
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
    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QVERIFY(!selectedWindow);
    QCOMPARE(input()->pointer()->focus(), client);
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
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(client->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), client);
    QVERIFY(pointerEnteredSpy.wait());

    Toplevel *selectedWindow = nullptr;
    auto callback = [&selectedWindow] (Toplevel *t) {
        selectedWindow = t;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractiveWindowSelection(callback);
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
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QVERIFY(!selectedWindow);
    QCOMPARE(input()->pointer()->focus(), client);
    // should give back keyboard and pointer
    QVERIFY(pointerEnteredSpy.wait());
    if (keyboardEnteredSpy.count() != 2) {
        QVERIFY(keyboardEnteredSpy.wait());
    }
    QCOMPARE(pointerLeftSpy.count(), 1);
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 2);
    QCOMPARE(keyboardEnteredSpy.count(), 2);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
}

void TestWindowSelection::testSelectPointPointer()
{
    // this test verifies point selection through pointer works
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy pointerEnteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(pointer.data(), &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());

    auto client = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client);
    QVERIFY(keyboardEnteredSpy.wait());
    KWin::Cursors::self()->mouse()->setPos(client->frameGeometry().center());
    QCOMPARE(input()->pointer()->focus(), client);
    QVERIFY(pointerEnteredSpy.wait());

    QPoint point;
    auto callback = [&point] (const QPoint &p) {
        point = p;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractivePositionSelection(callback);
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
    kwinApp()->platform()->startInteractivePositionSelection([&point2] (const QPoint &p) {
        point2 = p;
    });
    QCOMPARE(point2, QPoint(-1, -1));

    // simulate left button press
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
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
    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, timestamp++);
    // should not have ended the mode
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());
    // now release
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(point, input()->globalPointer().toPoint());
    QCOMPARE(input()->pointer()->focus(), client);
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
    auto callback = [&point] (const QPoint &p) {
        point = p;
    };

    // start the interaction
    QCOMPARE(input()->isSelectingWindow(), false);
    kwinApp()->platform()->startInteractivePositionSelection(callback);
    QCOMPARE(input()->isSelectingWindow(), true);
    QCOMPARE(point, QPoint());

    // let's create multiple touch points
    quint32 timestamp = 0;
    kwinApp()->platform()->touchDown(0, QPointF(0, 1), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    kwinApp()->platform()->touchDown(1, QPointF(10, 20), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    kwinApp()->platform()->touchDown(2, QPointF(30, 40), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);

    // let's move our points
    kwinApp()->platform()->touchMotion(0, QPointF(5, 10), timestamp++);
    kwinApp()->platform()->touchMotion(2, QPointF(20, 25), timestamp++);
    kwinApp()->platform()->touchMotion(1, QPointF(25, 35), timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    kwinApp()->platform()->touchUp(2, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), true);
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(point, QPoint(25, 35));
}

WAYLANDTEST_MAIN(TestWindowSelection)
#include "window_selection_test.moc"
