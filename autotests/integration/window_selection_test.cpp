/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "cursor.h"
#include "keyboard_input.h"
#include "platform.h"
#include "pointer_input.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

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
    void testCancelOnWindowPointer();
    void testCancelOnWindowKeyboard();
};

void TestWindowSelection::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestWindowSelection::init()
{
    QVERIFY(Test::setupWaylandConnection(s_socketName, Test::AdditionalWaylandInterface::Seat));
    QVERIFY(Test::waitForWaylandPointer());

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
}

void TestWindowSelection::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestWindowSelection::testSelectOnWindowPointer()
{
    // this test verifies window selection through pointer works
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
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
    KWin::Cursor::setPos(client->geometry().center());
    QCOMPARE(input()->pointer()->window().data(), client);
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
    QVERIFY(input()->pointer()->window().isNull());

    // updating the pointer should not change anything
    input()->pointer()->update();
    QVERIFY(input()->pointer()->window().isNull());
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
    QCOMPARE(input()->pointer()->window().data(), client);
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
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
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
    QVERIFY(!client->geometry().contains(KWin::Cursor::pos()));

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
    while (KWin::Cursor::pos().x() >= client->geometry().x() + client->geometry().width()) {
        keyPress(KEY_LEFT);
    }
    while (KWin::Cursor::pos().x() <= client->geometry().x()) {
        keyPress(KEY_RIGHT);
    }
    while (KWin::Cursor::pos().y() <= client->geometry().y()) {
        keyPress(KEY_DOWN);
    }
    while (KWin::Cursor::pos().y() >= client->geometry().y() + client->geometry().height()) {
        keyPress(KEY_UP);
    }
    QFETCH(qint32, key);
    kwinApp()->platform()->keyboardKeyPressed(key, timestamp++);
    QCOMPARE(input()->isSelectingWindow(), false);
    QCOMPARE(selectedWindow, client);
    QCOMPARE(input()->pointer()->window().data(), client);
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

void TestWindowSelection::testCancelOnWindowPointer()
{
    // this test verifies that window selection cancels through right button click
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
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
    KWin::Cursor::setPos(client->geometry().center());
    QCOMPARE(input()->pointer()->window().data(), client);
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
    QCOMPARE(input()->pointer()->window().data(), client);
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
    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
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
    KWin::Cursor::setPos(client->geometry().center());
    QCOMPARE(input()->pointer()->window().data(), client);
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
    QCOMPARE(input()->pointer()->window().data(), client);
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

WAYLANDTEST_MAIN(TestWindowSelection)
#include "window_selection_test.moc"
