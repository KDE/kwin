/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "cursor.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/pointerconstraints.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

#include <functional>

using namespace KWin;

typedef std::function<QPointF(const QRectF &)> PointerFunc;
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
    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    // set custom config which disables the OnScreenNotification
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup group = config->group(QStringLiteral("OnScreenNotification"));
    group.writeEntry(QStringLiteral("QmlPath"), QString("/does/not/exist.qml"));
    group.sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void TestPointerConstraints::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::PointerConstraints));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));
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
    PointerFunc bottomLeft = [](const QRectF &rect) {
        return rect.toRect().bottomLeft();
    };
    PointerFunc bottomRight = [](const QRectF &rect) {
        return rect.toRect().bottomRight();
    };
    PointerFunc topRight = [](const QRectF &rect) {
        return rect.toRect().topRight();
    };
    PointerFunc topLeft = [](const QRectF &rect) {
        return rect.toRect().topLeft();
    };

    QTest::newRow("XdgWmBase - bottomLeft") << bottomLeft << -1 << 1;
    QTest::newRow("XdgWmBase - bottomRight") << bottomRight << 1 << 1;
    QTest::newRow("XdgWmBase - topLeft") << topLeft << -1 << -1;
    QTest::newRow("XdgWmBase - topRight") << topRight << 1 << -1;
}

void TestPointerConstraints::testConfinedPointer()
{
    // this test sets up a Surface with a confined pointer
    // simple interaction test to verify that the pointer gets confined
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::ConfinedPointer> confinedPointer(Test::waylandPointerConstraints()->confinePointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
    QSignalSpy confinedSpy(confinedPointer.get(), &KWayland::Client::ConfinedPointer::confined);
    QSignalSpy unconfinedSpy(confinedPointer.get(), &KWayland::Client::ConfinedPointer::unconfined);

    // now map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window);
    if (window->pos() == QPoint(0, 0)) {
        window->move(QPoint(1, 1));
    }
    QVERIFY(!exclusiveContains(window->frameGeometry(), KWin::Cursors::self()->mouse()->pos()));

    // now let's confine
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(confinedSpy.wait());

    // picking a position outside the window geometry should not move pointer
    QSignalSpy pointerPositionChangedSpy(input(), &InputRedirection::globalPointerChanged);
    KWin::input()->pointer()->warp(QPoint(512, 512));
    QVERIFY(pointerPositionChangedSpy.isEmpty());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center());

    // TODO: test relative motion
    QFETCH(PointerFunc, positionFunction);
    const QPointF position = positionFunction(window->frameGeometry());
    KWin::input()->pointer()->warp(position);
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);
    // moving one to right should not be possible
    QFETCH(int, xOffset);
    KWin::input()->pointer()->warp(position + QPoint(xOffset, 0));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);
    // moving one to bottom should not be possible
    QFETCH(int, yOffset);
    KWin::input()->pointer()->warp(position + QPoint(0, yOffset));
    QCOMPARE(pointerPositionChangedSpy.count(), 1);
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), position);

    // modifier + click should be ignored
    // first ensure the settings are ok
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("MouseBindings"));
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
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);

    // set the opacity to 0.5
    window->setOpacity(0.5);
    QCOMPARE(window->opacity(), 0.5);

    // pointer is confined so shortcut should not work
    Test::pointerAxisVertical(-5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    Test::pointerAxisVertical(5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);

    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);

    // deactivate the window, this should unconfine
    workspace()->activateWindow(nullptr);
    QVERIFY(unconfinedSpy.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);

    // reconfine pointer (this time with persistent life time)
    confinedPointer.reset(Test::waylandPointerConstraints()->confinePointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::Persistent));
    QSignalSpy confinedSpy2(confinedPointer.get(), &KWayland::Client::ConfinedPointer::confined);
    QSignalSpy unconfinedSpy2(confinedPointer.get(), &KWayland::Client::ConfinedPointer::unconfined);

    // activate it again, this confines again
    workspace()->activateWindow(static_cast<Window *>(input()->pointer()->focus()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // deactivate the window one more time with the persistent life time constraint, this should unconfine
    workspace()->activateWindow(nullptr);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // activate it again, this confines again
    workspace()->activateWindow(static_cast<Window *>(input()->pointer()->focus()));
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // create a second window and move it above our constrained window
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(c2);
    QVERIFY(unconfinedSpy2.wait());
    // and unmapping the second window should confine again
    shellSurface2.reset();
    surface2.reset();
    QVERIFY(confinedSpy2.wait());

    // let's set a region which results in unconfined
    auto r = Test::waylandCompositor()->createRegion(QRegion(2, 2, 3, 3));
    confinedPointer->setRegion(r.get());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(unconfinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), false);
    // and set a full region again, that should confine
    confinedPointer->setRegion(nullptr);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(confinedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // delete pointer confine
    confinedPointer.reset(nullptr);
    Test::flushWaylandConnection();

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(constraintsChangedSpy.wait());

    // should be unconfined
    QCOMPARE(input()->pointer()->isConstrained(), false);

    // confine again
    confinedPointer.reset(Test::waylandPointerConstraints()->confinePointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::Persistent));
    QSignalSpy confinedSpy3(confinedPointer.get(), &KWayland::Client::ConfinedPointer::confined);
    QVERIFY(confinedSpy3.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // and now unmap
    shellSurface.reset();
    surface.reset();
    QVERIFY(Test::waitForWindowClosed(window));
    QCOMPARE(input()->pointer()->isConstrained(), false);
}

void TestPointerConstraints::testLockedPointer()
{
    // this test sets up a Surface with a locked pointer
    // simple interaction test to verify that the pointer gets locked
    // the various ways to unlock are not tested as that's already verified by testConfinedPointer
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::locked);
    QSignalSpy unlockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::unlocked);

    // now map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window);
    QVERIFY(!exclusiveContains(window->frameGeometry(), KWin::Cursors::self()->mouse()->pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center());
    QCOMPARE(input()->pointer()->isConstrained(), true);
    QVERIFY(lockedSpy.wait());

    // try to move the pointer
    // TODO: add relative pointer
    KWin::input()->pointer()->warp(window->frameGeometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center());

    // deactivate the window, this should unlock
    workspace()->activateWindow(nullptr);
    QCOMPARE(input()->pointer()->isConstrained(), false);
    QVERIFY(unlockedSpy.wait());

    // moving cursor should be allowed again
    KWin::input()->pointer()->warp(window->frameGeometry().center() + QPoint(1, 1));
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center() + QPoint(1, 1));

    lockedPointer.reset(Test::waylandPointerConstraints()->lockPointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::Persistent));
    QSignalSpy lockedSpy2(lockedPointer.get(), &KWayland::Client::LockedPointer::locked);

    // activate the window again, this should lock again
    workspace()->activateWindow(static_cast<Window *>(input()->pointer()->focus()));
    QVERIFY(lockedSpy2.wait());
    QCOMPARE(input()->pointer()->isConstrained(), true);

    // try to move the pointer
    QCOMPARE(input()->pointer()->isConstrained(), true);
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center() + QPoint(1, 1));

    // delete pointer lock
    lockedPointer.reset(nullptr);
    Test::flushWaylandConnection();

    QSignalSpy constraintsChangedSpy(input()->pointer()->focus()->surface(), &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(constraintsChangedSpy.wait());

    // moving cursor should be allowed again
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center());
}

void TestPointerConstraints::testCloseWindowWithLockedPointer()
{
    // test case which verifies that the pointer gets unlocked when the window for it gets closed
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    std::unique_ptr<KWayland::Client::Pointer> pointer(Test::waylandSeat()->createPointer());
    std::unique_ptr<KWayland::Client::LockedPointer> lockedPointer(Test::waylandPointerConstraints()->lockPointer(surface.get(), pointer.get(), nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
    QSignalSpy lockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::locked);
    QSignalSpy unlockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::unlocked);

    // now map the window
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::blue);
    QVERIFY(window);
    QVERIFY(!exclusiveContains(window->frameGeometry(), KWin::Cursors::self()->mouse()->pos()));

    // now let's lock
    QCOMPARE(input()->pointer()->isConstrained(), false);
    KWin::input()->pointer()->warp(window->frameGeometry().center());
    QCOMPARE(KWin::Cursors::self()->mouse()->pos(), window->frameGeometry().center());
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
