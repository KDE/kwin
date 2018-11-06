/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest>
// client
#include "../../src/client/connection_thread.h"
#include "../../src/client/compositor.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/pointer.h"
#include "../../src/client/pointerconstraints.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/pointerconstraints_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/surface_interface.h"

using namespace KWayland::Client;
using namespace KWayland::Server;

Q_DECLARE_METATYPE(KWayland::Client::PointerConstraints::LifeTime)
Q_DECLARE_METATYPE(KWayland::Server::ConfinedPointerInterface::LifeTime)
Q_DECLARE_METATYPE(KWayland::Server::LockedPointerInterface::LifeTime)

class TestPointerConstraints : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testLockPointer_data();
    void testLockPointer();

    void testConfinePointer_data();
    void testConfinePointer();
    void testAlreadyConstrained_data();
    void testAlreadyConstrained();

private:
    Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    PointerConstraintsInterface *m_pointerConstraintsInterface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Compositor *m_compositor = nullptr;
    Seat *m_seat = nullptr;
    Pointer *m_pointer = nullptr;
    PointerConstraints *m_pointerConstraints = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-pointer_constraint-0");

void TestPointerConstraints::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = m_display->createSeat(m_display);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->create();
    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    m_pointerConstraintsInterface = m_display->createPointerConstraints(PointerConstraintsInterfaceVersion::UnstableV1, m_display);
    m_pointerConstraintsInterface->create();

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy interfaceAnnouncedSpy(&registry, &Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_pointerConstraints = registry.createPointerConstraints(registry.interface(Registry::Interface::PointerConstraintsUnstableV1).name,
                                                             registry.interface(Registry::Interface::PointerConstraintsUnstableV1).version, this);
    QVERIFY(m_pointerConstraints);
    QVERIFY(m_pointerConstraints->isValid());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version, this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());
    m_pointer = m_seat->createPointer(this);
    QVERIFY(m_pointer);
}

void TestPointerConstraints::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_pointerConstraints)
    CLEANUP(m_pointer)
    CLEANUP(m_seat)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_compositorInterface)
    CLEANUP(m_seatInterface);
    CLEANUP(m_pointerConstraintsInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestPointerConstraints::testLockPointer_data()
{
    QTest::addColumn<PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<LockedPointerInterface::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << PointerConstraints::LifeTime::Persistent << LockedPointerInterface::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << PointerConstraints::LifeTime::OneShot << LockedPointerInterface::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testLockPointer()
{
    // this test verifies the basic interaction for lock pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->lockedPointer().isNull());
    QVERIFY(serverSurface->confinedPointer().isNull());

    // now create the locked pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(pointerConstraintsChangedSpy.isValid());
    QFETCH(PointerConstraints::LifeTime, clientLifeTime);
    QScopedPointer<LockedPointer> lockedPointer(m_pointerConstraints->lockPointer(surface.data(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy lockedSpy(lockedPointer.data(), &LockedPointer::locked);
    QVERIFY(lockedSpy.isValid());
    QSignalSpy unlockedSpy(lockedPointer.data(), &LockedPointer::unlocked);
    QVERIFY(unlockedSpy.isValid());
    QVERIFY(lockedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverLockedPointer = serverSurface->lockedPointer();
    QVERIFY(serverLockedPointer);
    QVERIFY(serverSurface->confinedPointer().isNull());

    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->region(), QRegion());
    QFETCH(LockedPointerInterface::LifeTime, serverLifeTime);
    QCOMPARE(serverLockedPointer->lifeTime(), serverLifeTime);
    // setting to unlocked now should not trigger an unlocked spy
    serverLockedPointer->setLocked(false);
    QVERIFY(!unlockedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverLockedPointer.data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy regionChangedSpy(serverLockedPointer.data(), &LockedPointerInterface::regionChanged);
    QVERIFY(regionChangedSpy.isValid());
    lockedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    lockedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion());

    // let's lock the surface
    QSignalSpy lockedChangedSpy(serverLockedPointer.data(), &LockedPointerInterface::lockedChanged);
    QVERIFY(lockedChangedSpy.isValid());
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QSignalSpy pointerMotionSpy(m_pointer, &Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    m_seatInterface->setPointerPos(QPoint(0, 1));
    QVERIFY(pointerMotionSpy.wait());

    serverLockedPointer->setLocked(true);
    QCOMPARE(serverLockedPointer->isLocked(), true);
    m_seatInterface->setPointerPos(QPoint(1, 1));
    QCOMPARE(lockedChangedSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QVERIFY(lockedSpy.isEmpty());
    QVERIFY(lockedSpy.wait());
    QVERIFY(unlockedSpy.isEmpty());

    const QPointF hint = QPointF(1.5, 0.5);
    QSignalSpy hintChangedSpy(serverLockedPointer.data(), &LockedPointerInterface::cursorPositionHintChanged);
    lockedPointer->setCursorPositionHint(hint);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(hintChangedSpy.wait());
    QCOMPARE(serverLockedPointer->cursorPositionHint(), hint);

    // and unlock again
    serverLockedPointer->setLocked(false);
    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    QCOMPARE(lockedChangedSpy.count(), 2);
    QTEST(!serverSurface->lockedPointer().isNull(), "hasConstraintAfterUnlock");
    QTEST(pointerConstraintsChangedSpy.count(), "pointerChangedCount");
    QVERIFY(unlockedSpy.wait());
    QCOMPARE(unlockedSpy.count(), 1);
    QCOMPARE(lockedSpy.count(), 1);

    // now motion should work again
    m_seatInterface->setPointerPos(QPoint(0, 1));
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.count(), 2);

    lockedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

void TestPointerConstraints::testConfinePointer_data()
{
    QTest::addColumn<PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<ConfinedPointerInterface::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << PointerConstraints::LifeTime::Persistent << ConfinedPointerInterface::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << PointerConstraints::LifeTime::OneShot << ConfinedPointerInterface::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testConfinePointer()
{
    // this test verifies the basic interaction for confined pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    QVERIFY(serverSurface->lockedPointer().isNull());
    QVERIFY(serverSurface->confinedPointer().isNull());

    // now create the confined pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QVERIFY(pointerConstraintsChangedSpy.isValid());
    QFETCH(PointerConstraints::LifeTime, clientLifeTime);
    QScopedPointer<ConfinedPointer> confinedPointer(m_pointerConstraints->confinePointer(surface.data(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy confinedSpy(confinedPointer.data(), &ConfinedPointer::confined);
    QVERIFY(confinedSpy.isValid());
    QSignalSpy unconfinedSpy(confinedPointer.data(), &ConfinedPointer::unconfined);
    QVERIFY(unconfinedSpy.isValid());
    QVERIFY(confinedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverConfinedPointer = serverSurface->confinedPointer();
    QVERIFY(serverConfinedPointer);
    QVERIFY(serverSurface->lockedPointer().isNull());

    QCOMPARE(serverConfinedPointer->isConfined(), false);
    QCOMPARE(serverConfinedPointer->region(), QRegion());
    QFETCH(ConfinedPointerInterface::LifeTime, serverLifeTime);
    QCOMPARE(serverConfinedPointer->lifeTime(), serverLifeTime);
    // setting to unconfined now should not trigger an unconfined spy
    serverConfinedPointer->setConfined(false);
    QVERIFY(!unconfinedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverConfinedPointer.data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    QSignalSpy regionChangedSpy(serverConfinedPointer.data(), &ConfinedPointerInterface::regionChanged);
    QVERIFY(regionChangedSpy.isValid());
    confinedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    confinedPointer->setRegion(nullptr);
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion());

    // let's confine the surface
    QSignalSpy confinedChangedSpy(serverConfinedPointer.data(), &ConfinedPointerInterface::confinedChanged);
    QVERIFY(confinedChangedSpy.isValid());
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    serverConfinedPointer->setConfined(true);
    QCOMPARE(serverConfinedPointer->isConfined(), true);
    QCOMPARE(confinedChangedSpy.count(), 1);
    QVERIFY(confinedSpy.isEmpty());
    QVERIFY(confinedSpy.wait());
    QVERIFY(unconfinedSpy.isEmpty());

    // and unconfine again
    serverConfinedPointer->setConfined(false);
    QCOMPARE(serverConfinedPointer->isConfined(), false);
    QCOMPARE(confinedChangedSpy.count(), 2);
    QTEST(!serverSurface->confinedPointer().isNull(), "hasConstraintAfterUnlock");
    QTEST(pointerConstraintsChangedSpy.count(), "pointerChangedCount");
    QVERIFY(unconfinedSpy.wait());
    QCOMPARE(unconfinedSpy.count(), 1);
    QCOMPARE(confinedSpy.count(), 1);

    confinedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

enum class Constraint {
    Lock,
    Confine
};

Q_DECLARE_METATYPE(Constraint)

void TestPointerConstraints::testAlreadyConstrained_data()
{
    QTest::addColumn<Constraint>("firstConstraint");
    QTest::addColumn<Constraint>("secondConstraint");

    QTest::newRow("confine-confine") << Constraint::Confine << Constraint::Confine;
    QTest::newRow("lock-confine") << Constraint::Lock << Constraint::Confine;
    QTest::newRow("confine-lock") << Constraint::Confine << Constraint::Lock;
    QTest::newRow("lock-lock") << Constraint::Lock << Constraint::Lock;
}

void TestPointerConstraints::testAlreadyConstrained()
{
    // this test verifies that creating a pointer constraint for an already constrained surface triggers an error
    // first create a surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QFETCH(Constraint, firstConstraint);
    QScopedPointer<ConfinedPointer> confinedPointer;
    QScopedPointer<LockedPointer> lockedPointer;
    switch (firstConstraint) {
    case Constraint::Lock:
        lockedPointer.reset(m_pointerConstraints->lockPointer(surface.data(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer.reset(m_pointerConstraints->confinePointer(surface.data(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    default:
        Q_UNREACHABLE();
    }
    QVERIFY(confinedPointer || lockedPointer);

    QSignalSpy errorSpy(m_connection, &ConnectionThread::errorOccurred);
    QVERIFY(errorSpy.isValid());
    QFETCH(Constraint, secondConstraint);
    QScopedPointer<ConfinedPointer> confinedPointer2;
    QScopedPointer<LockedPointer> lockedPointer2;
    switch (secondConstraint) {
    case Constraint::Lock:
        lockedPointer2.reset(m_pointerConstraints->lockPointer(surface.data(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer2.reset(m_pointerConstraints->confinePointer(surface.data(), m_pointer, nullptr, PointerConstraints::LifeTime::OneShot));
        break;
    default:
        Q_UNREACHABLE();
    }
    QVERIFY(errorSpy.wait());
    QVERIFY(m_connection->hasError());
    if (confinedPointer2) {
        confinedPointer2->destroy();
    }
    if (lockedPointer2) {
        lockedPointer2->destroy();
    }
    if (confinedPointer) {
        confinedPointer->destroy();
    }
    if (lockedPointer) {
        lockedPointer->destroy();
    }
    surface->destroy();
    m_compositor->destroy();
    m_pointerConstraints->destroy();
    m_pointer->destroy();
    m_seat->destroy();
    m_queue->destroy();
}

QTEST_GUILESS_MAIN(TestPointerConstraints)
#include "test_pointer_constraints.moc"
