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
#include "xdgshellclient.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>

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
    void testCloseWindowWithLockedPointer_data();
    void testCloseWindowWithLockedPointer();
};

void TestPointerConstraints::initTestCase()
{
    qRegisterMetaType<PointerFunc>();
    qRegisterMetaType<KWin::XdgShellClient *>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

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
    QTest::addColumn<Test::XdgShellSurfaceType>("type");
    QTest::addColumn<PointerFunc>("positionFunction");
    QTest::addColumn<int>("xOffset");
    QTest::addColumn<int>("yOffset");
    PointerFunc bottomLeft = &QRect::bottomLeft;
    PointerFunc bottomRight = &QRect::bottomRight;
    PointerFunc topRight = &QRect::topRight;
    PointerFunc topLeft = &QRect::topLeft;

    QTest::newRow("XdgShellV6 - bottomLeft")  << Test::XdgShellSurfaceType::XdgShellV6 << bottomLeft  << -1 << 1;
    QTest::newRow("XdgShellV6 - bottomRight") << Test::XdgShellSurfaceType::XdgShellV6 << bottomRight << 1  << 1;
    QTest::newRow("XdgShellV6 - topLeft")     << Test::XdgShellSurfaceType::XdgShellV6 << topLeft  << -1 << -1;
    QTest::newRow("XdgShellV6 - topRight")    << Test::XdgShellSurfaceType::XdgShellV6 << topRight << 1  << -1;
    QTest::newRow("XdgWmBase - bottomLeft")   << Test::XdgShellSurfaceType::XdgShellStable << bottomLeft  << -1 << 1;
    QTest::newRow("XdgWmBase - bottomRight")  << Test::XdgShellSurfaceType::XdgShellStable << bottomRight << 1  << 1;
    QTest::newRow("XdgWmBase - topLeft")      << Test::XdgShellSurfaceType::XdgShellStable << topLeft  << -1 << -1;
    QTest::newRow("XdgWmBase - topRight")     << Test::XdgShellSurfaceType::XdgShellStable << topRight << 1  << -1;
}

void TestPointerConstraints::testConfinedPointer()
{
    // this test sets up a Surface with a confined pointer
    // simple interaction test to verify that the pointer gets confined
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::XdgShellSurfaceType, type);
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellSurface(type, surface.data()));
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

    // modifier + click should be ignored
    // first ensure the settings are ok
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", QStringLiteral("Alt"));
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::AltModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll2(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedMove);

    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!c->isMove());
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);

    // set the opacity to 0.5
    c->setOpacity(0.5);
    QCOMPARE(c->opacity(), 0.5);

    // pointer is confined so shortcut should not work
    kwinApp()->platform()->pointerAxisVertical(-5, timestamp++);
    QCOMPARE(c->opacity(), 0.5);
    kwinApp()->platform()->pointerAxisVertical(5, timestamp++);
    QCOMPARE(c->opacity(), 0.5);

    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTALT, timestamp++);

    // deactivate the client, this should unconfine
    workspace()->activateClient(nullptr);
    QVERIFY(unconfinedSpy.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);

    // reconfine pointer (this time with persistent life time)
    confinedPointer.reset(Test::waylandPointerConstraints()->confinePointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy confinedSpy2(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy2.isValid());
    QSignalSpy unconfinedSpy2(confinedPointer.data(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy2.isValid());

    // activate it again, this confines again
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus().data()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // deactivate the client one more time with the persistent life time constraint, this should unconfine
    workspace()->activateClient(nullptr);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // activate it again, this confines again
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus().data()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // create a second window and move it above our constrained window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellSurface(type, surface2.data()));
    auto c2 = Test::renderAndWaitForShown(surface2.data(), QSize(1280, 1024), Qt::blue);
    QVERIFY(c2);
    QVERIFY(unconfinedSpy2.wait());
    // and unmapping the second window should confine again
    shellSurface2.reset();
    surface2.reset();
    QVERIFY(confinedSpy2.wait());

    // let's set a region which results in unconfined
    auto r = Test::waylandCompositor()->createRegion(QRegion(2, 2, 3, 3));
    confinedPointer->setRegion(r.get());
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // and set a full region again, that should confine
    confinedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // delete pointer confine
    confinedPointer.reset(nullptr);
    Test::flushWaylandConnection();

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &KWayland::Server::SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(constraintsChangedSpy.isValid());
    QVERIFY(constraintsChangedSpy.wait());

    // should be unconfined
    QCOMPARE(input()->pointer()->isConstrained(), false);

    // confine again
    confinedPointer.reset(Test::waylandPointerConstraints()->confinePointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy confinedSpy3(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy3.isValid());
    QVERIFY(confinedSpy3.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // and now unmap
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowDestroyed(c));
    QCOMPARE(input()->pointer()->isConstrained(), false);
}

void TestPointerConstraints::testLockedPointer_data()
{
    QTest::addColumn<Test::XdgShellSurfaceType>("type");

    QTest::newRow("xdgShellV6") << Test::XdgShellSurfaceType::XdgShellV6;
    QTest::newRow("xdgWmBase") << Test::XdgShellSurfaceType::XdgShellStable;
}

void TestPointerConstraints::testLockedPointer()
{
    // this test sets up a Surface with a locked pointer
    // simple interaction test to verify that the pointer gets locked
    // the various ways to unlock are not tested as that's already verified by testConfinedPointer
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::XdgShellSurfaceType, type);
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellSurface(type, surface.data()));
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

    // deactivate the client, this should unlock
    workspace()->activateClient(nullptr);
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unlockedSpy.wait());

    // moving cursor should be allowed again
    KWin::Cursor::setPos(c->geometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center() + QPoint(1, 1));

    lockedPointer.reset(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy lockedSpy2(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy2.isValid());

    // activate the client again, this should lock again
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus().data()));
    QVERIFY(lockedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // try to move the pointer
    QCOMPARE(input()->pointer()->isConstrained(), true);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center() + QPoint(1, 1));

    // delete pointer lock
    lockedPointer.reset(nullptr);
    Test::flushWaylandConnection();

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &KWayland::Server::SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(constraintsChangedSpy.isValid());
    QVERIFY(constraintsChangedSpy.wait());

    // moving cursor should be allowed again
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursor::setPos(c->geometry().center());
    QCOMPARE(KWin::Cursor::pos(), c->geometry().center());
}

void TestPointerConstraints::testCloseWindowWithLockedPointer_data()
{
    QTest::addColumn<Test::XdgShellSurfaceType>("type");

    QTest::newRow("XdgShellV6") << Test::XdgShellSurfaceType::XdgShellV6;
    QTest::newRow("XdgWmBase") << Test::XdgShellSurfaceType::XdgShellStable;
}

void TestPointerConstraints::testCloseWindowWithLockedPointer()
{
    // test case which verifies that the pointer gets unlocked when the window for it gets closed
    QScopedPointer<Surface> surface(Test::createSurface());
    QFETCH(Test::XdgShellSurfaceType, type);
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellSurface(type, surface.data()));
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
