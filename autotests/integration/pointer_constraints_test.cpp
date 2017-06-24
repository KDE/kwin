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
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Server/seat_interface.h>

#include <linux/input.h>

#include <functional>

using namespace KWin;
using namespace KWayland::Client;

typedef std::function<QPoint(const QRect&)> PointerFunc;
Q_DECLARE_METATYPE(PointerFunc)

static const QString s_socketName = QStringLiteral("wayland_test_kwin_pointer_constraints-0");

class TestPointerConstraints : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testConfinedPointer_data();
    void testConfinedPointer();
    void testLockedPointer_data();
    void testLockedPointer();
    void testBreakConstrainedPointer_data();
    void testBreakConstrainedPointer();
    void testCloseWindowWithLockedPointer_data();
    void testCloseWindowWithLockedPointer();
};

void TestPointerConstraints::initTestCase()
{
    qRegisterMetaType<PointerFunc>();
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // set custom config which disables the OnScreenNotification
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group("OnScreenNotification");
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);


    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TestPointerConstraints::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::PointerConstraints));
    QVERIFY(Test::waitForWaylandPointer());

    screens()->setCurrent(0);
    KWin::Cursor::setPos(QPoint(1280, 512));
}

void TestPointerConstraints::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPointerConstraints::testConfinedPointer_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");
    QTest::addColumn<PointerFunc>("positionFunction");
    QTest::addColumn<int>("xOffset");
    QTest::addColumn<int>("yOffset");
    PointerFunc bottomLeft = &QRect::bottomLeft;
    PointerFunc bottomRight = &QRect::bottomRight;
    PointerFunc topRight = &QRect::topRight;
    PointerFunc topLeft = &QRect::topLeft;

    QTest::newRow("wlShell - bottomLeft")  << Test::ShellSurfaceType::WlShell << bottomLeft  << -1 << 1;
    QTest::newRow("wlShell - bottomRight") << Test::ShellSurfaceType::WlShell << bottomRight << 1  << 1;
    QTest::newRow("wlShell - topLeft")     << Test::ShellSurfaceType::WlShell << topLeft  << -1 << -1;
    QTest::newRow("wlShell - topRight")    << Test::ShellSurfaceType::WlShell << topRight << 1  << -1;
    QTest::newRow("XdgShellV5 - bottomLeft")  << Test::ShellSurfaceType::XdgShellV5 << bottomLeft  << -1 << 1;
    QTest::newRow("XdgShellV5 - bottomRight") << Test::ShellSurfaceType::XdgShellV5 << bottomRight << 1  << 1;
    QTest::newRow("XdgShellV5 - topLeft")     << Test::ShellSurfaceType::XdgShellV5 << topLeft  << -1 << -1;
    QTest::newRow("XdgShellV5 - topRight")    << Test::ShellSurfaceType::XdgShellV5 << topRight << 1  << -1;
}

void TestPointerConstraints::testConfinedPointer()
{
    // this test sets up a Surface with a confined pointer
    // simple interaction test to verify that the pointer gets confined
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<ConfinedPointer> confinedPointer(Test::waylandPointerConstraints()->confinePointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy confinedSpy(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy.isValid());
    QSignalSpy unconfinedSpy(confinedPointer.data(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    if (c->geometry().topLeft() == QPoint(0, 0)) {
        c->move(QPoint(1, 1));
    }
    QVERIFY(!c->geometry().contains(KWin::Cursor::pos()));

    // now let's confine
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(confinedSpy.wait());

    // picking a position outside the window geometry should not move pointer
    QSignalSpy pointerPositionChangedSpy(input(), &InputRedirection::globalPointerChanged);
    QVERIFY(pointerPositionChangedSpy.isValid());
    KWin::Cursor::setPos(QPoint(1280, 512));
    QVERIFY(pointerPositionChangedSpy.isEmpty());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());

    // TODO: test relative motion
    QFETCH(PointerFunc, positionFunction);
    const QPoint position = positionFunction(c->geometry());
    KWin::Cursor::setPos(position);
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursor::pos(), position);
    // moving one to right should not be possible
    QFETCH(int, xOffset);
    KWin::Cursor::setPos(position + QPoint(xOffset, 0));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursor::pos(), position);
    // moving one to bottom should not be possible
    QFETCH(int, yOffset);
    KWin::Cursor::setPos(position + QPoint(0, yOffset));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursor::pos(), position);

    // let's break the constraint explicitly
    input()->pointer()->breakPointerConstraints();
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unconfinedSpy.wait());
    confinedPointer.reset(Test::waylandPointerConstraints()->confinePointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy confinedSpy2(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy2.isValid());
    QSignalSpy unconfinedSpy2(confinedPointer.data(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy2.isValid());
    // should get confined
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // now let's unconfine again, any pointer movement should confine again
    input()->pointer()->breakPointerConstraints();
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unconfinedSpy2.wait());
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(confinedSpy2.wait());

    // let's use the other break constraint and block
    input()->pointer()->breakPointerConstraints();
    input()->pointer()->blockPointerConstraints();
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center() + QPoint(1, 1));
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(!confinedSpy2.wait());

    // now move outside and back in again, that should confine
    KWin::Cursor::setPos(c->geometry().bottomRight() + QPoint(1, 1));
    KWin::Cursor::setPos(c->geometry().center() + QPoint(1, 1));
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(confinedSpy2.wait());

    // create a second window and move it above our constrained window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<QObject> shellSurface2(Test::createShellSurface(type, surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(c2);
    QVERIFY(unconfinedSpy2.wait());
    // and unmapping the second window should confine again
    shellSurface2.reset();
    surface2.reset();
    QVERIFY(confinedSpy2.wait());

    // let's set a region which results in unconfined
    auto r = Test::waylandCompositor()->createRegion(QRegion(0, 0, 1, 1));
    confinedPointer->setRegion(r.get());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // and set a full region again, that should confine
    confinedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // and now unmap
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
    QCOMPARE(input()->pointer()->isConstrained(), false);
}

void TestPointerConstraints::testLockedPointer_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestPointerConstraints::testLockedPointer()
{
    // this test sets up a Surface with a locked pointer
    // simple interaction test to verify that the pointer gets locked
    // the various ways to unlock are not tested as that's already verified by testConfinedPointer
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->geometry().contains(KWin::Cursor::pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // try to move the pointer
    // TODO: add relative pointer
    KWin::Cursor::setPos(c->geometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());

    // now unlock again
    input()->pointer()->breakPointerConstraints();
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unlockedSpy.wait());

    // moving cursor should be allowed again
    KWin::Cursor::setPos(c->geometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center() + QPoint(1, 1));
}

void TestPointerConstraints::testBreakConstrainedPointer_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestPointerConstraints::testBreakConstrainedPointer()
{
    // this test verifies the breaking of Pointer constraints through the keyboard event filter
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy keyboardEnteredSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(keyboardEnteredSpy.isValid());
    QSignalSpy keyboardLeftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(keyboardLeftSpy.isValid());
    QSignalSpy keyChangedSpy(keyboard.data(), &Keyboard::keyChanged);
    QVERIFY(keyChangedSpy.isValid());
    QScopedPointer<LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->geometry().contains(KWin::Cursor::pos()));
    QVERIFY(keyboardEnteredSpy.wait());
    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // now try to break
    quint32 timestamp = 0;
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    // and just waiting should break constrain
    QVERIFY(unlockedSpy.wait());
    QCOMPARE(keyboardLeftSpy.count(), 1);
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // and should enter again
    QTRY_COMPARE(keyboardEnteredSpy.count(), 2);
    QCOMPARE(waylandServer()->seat()->focusedKeyboardSurface(), c->surface());
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
    QVERIFY(!keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 1);

    // now lock again
    // need to move out and in
    KWin::Cursor::setPos(c->geometry().bottomRight() + QPoint(1, 1));
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // and just do a key press/release on esc
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last().at(0).value<quint32>(), quint32(KEY_ESC));

    // and another variant which won't break
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last().at(0).value<quint32>(), quint32(KEY_LEFTSHIFT));
    QVERIFY(!unlockedSpy.wait());
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.last().at(0).value<quint32>(), quint32(KEY_ESC));

    // and now break for real
    kwinApp()->platform()->keyboardKeyPressed(KEY_ESC, timestamp++);
    QVERIFY(unlockedSpy.wait());
    kwinApp()->platform()->keyboardKeyReleased(KEY_ESC, timestamp++);
}

void TestPointerConstraints::testCloseWindowWithLockedPointer_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("XdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
}

void TestPointerConstraints::testCloseWindowWithLockedPointer()
{
    // test case which verifies that the pointer gets unlocked when the window for it gets closed
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::ShellSurfaceType, type);
    QScopedPointer<QObject> shellSurface(Test::createShellSurface(type, surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->geometry().contains(KWin::Cursor::pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // close the window
    shellSurface.reset();
    surface.reset();
    // this should result in unlocked
    QVERIFY(unlockedSpy.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
}

WAYLANDTEST_MAIN(TestPointerConstraints)
#include "pointer_constraints_test.moc"
