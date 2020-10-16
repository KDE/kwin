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
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>

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
    void testLockedPointer();
    void testCloseWindowWithLockedPointer();
};

void TestPointerConstraints::initTestCase()
{
    qRegisterMetaType<PointerFunc>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
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
    QVERIFY(applicationStartedSpy.wait());
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
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));
}

void TestPointerConstraints::cleanup()
{
    Test::destroyWaylandConnection();
}

void TestPointerConstraints::testConfinedPointer_data()
{
    QTest::addColumn<PointerFunc>("positionFunction");
    QTest::addColumn<int>("xOffset");
    QTest::addColumn<int>("yOffset");
    PointerFunc bottomLeft = &QRect::bottomLeft;
    PointerFunc bottomRight = &QRect::bottomRight;
    PointerFunc topRight = &QRect::topRight;
    PointerFunc topLeft = &QRect::topLeft;

    QTest::newRow("XdgWmBase - bottomLeft")   << bottomLeft  << -1 << 1;
    QTest::newRow("XdgWmBase - bottomRight")  << bottomRight << 1  << 1;
    QTest::newRow("XdgWmBase - topLeft")      << topLeft  << -1 << -1;
    QTest::newRow("XdgWmBase - topRight")     << topRight << 1  << -1;
}

void TestPointerConstraints::testConfinedPointer()
{
    // this test sets up a Surface with a confined pointer
    // simple interaction test to verify that the pointer gets confined
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<ConfinedPointer> confinedPointer(Test::waylandPointerConstraints()->confinePointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy confinedSpy(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy.isValid());
    QSignalSpy unconfinedSpy(confinedPointer.data(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    if (c->pos() == QPoint(0, 0)) {
        c->move(QPoint(1, 1));
    }
    QVERIFY(!c->frameGeometry().contains(KWin::Cursors::self()->mouse()->pos()));

    // now let's confine
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(confinedSpy.wait());

    // picking a position outside the window geometry should not move pointer
    QSignalSpy pointerPositionChangedSpy(input(), &InputRedirection::globalPointerChanged);
    QVERIFY(pointerPositionChangedSpy.isValid());
    KWin::Cursors::self()->mouse()->setPos(QPoint(1280, 512));
    QVERIFY(pointerPositionChangedSpy.isEmpty());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center());

    // TODO: test relative motion
    QFETCH(PointerFunc, positionFunction);
    const QPoint position = positionFunction(c->frameGeometry());
    KWin::Cursors::self()->mouse()->setPos(position);
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);
    // moving one to right should not be possible
    QFETCH(int, xOffset);
    KWin::Cursors::self()->mouse()->setPos(position + QPoint(xOffset, 0));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);
    // moving one to bottom should not be possible
    QFETCH(int, yOffset);
    KWin::Cursors::self()->mouse()->setPos(position + QPoint(0, yOffset));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);

    // modifier + click should be ignored
    // first ensure the settings are ok
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", QStringLiteral("Meta"));
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
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
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // deactivate the client one more time with the persistent life time constraint, this should unconfine
    workspace()->activateClient(nullptr);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // activate it again, this confines again
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // create a second window and move it above our constrained window
    QScopedPointer<Surface> surface2(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface2(Test::createXdgShellStableSurface(surface2.data()));
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

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &KWaylandServer::SurfaceInterface::pointerConstraintsChanged);
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

void TestPointerConstraints::testLockedPointer()
{
    // this test sets up a Surface with a locked pointer
    // simple interaction test to verify that the pointer gets locked
    // the various ways to unlock are not tested as that's already verified by testConfinedPointer
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->frameGeometry().contains(KWin::Cursors::self()->mouse()->pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // try to move the pointer
    // TODO: add relative pointer
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center());

    // deactivate the client, this should unlock
    workspace()->activateClient(nullptr);
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unlockedSpy.wait());

    // moving cursor should be allowed again
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center() + QPoint(1, 1));

    lockedPointer.reset(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::Persistent));
    QSignalSpy lockedSpy2(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy2.isValid());

    // activate the client again, this should lock again
    workspace()->activateClient(static_cast<AbstractClient*>(input()->pointer()->focus()));
    QVERIFY(lockedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // try to move the pointer
    QCOMPARE(input()->pointer()->isConstrained(), true);
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center() + QPoint(1, 1));

    // delete pointer lock
    lockedPointer.reset(nullptr);
    Test::flushWaylandConnection();

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &KWaylandServer::SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(constraintsChangedSpy.isValid());
    QVERIFY(constraintsChangedSpy.wait());

    // moving cursor should be allowed again
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center());
}

void TestPointerConstraints::testCloseWindowWithLockedPointer()
{
    // test case which verifies that the pointer gets unlocked when the window for it gets closed
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<Pointer> pointer(Test::waylandSeat()->createPointer());
    QScopedPointer<LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.data(), pointer.data(), nullptr, PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());

    // now map the window
    auto c = Test::renderAndWaitForShown(surface.data(), QSize(100, 100), Qt::blue);
    QVERIFY(c);
    QVERIFY(!c->frameGeometry().contains(KWin::Cursors::self()->mouse()->pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::Cursors::self()->mouse()->setPos(c->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), c->frameGeometry().center());
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
