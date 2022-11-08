/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/pointerconstraints.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"
// server
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/pointerconstraints_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"

using namespace KWaylandServer;

Q_DECLARE_METATYPE(KWayland::Client::PointerConstraints::LifeTime)
Q_DECLARE_METATYPE(KWaylandServer::ConfinedPointerV1Interface::LifeTime)
Q_DECLARE_METATYPE(KWaylandServer::LockedPointerV1Interface::LifeTime)

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
    KWaylandServer::Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    PointerConstraintsV1Interface *m_pointerConstraintsInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Pointer *m_pointer = nullptr;
    KWayland::Client::PointerConstraints *m_pointerConstraints = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-pointer_constraint-0");

void TestPointerConstraints::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = new SeatInterface(m_display, m_display);
    m_seatInterface->setHasPointer(true);
    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_pointerConstraintsInterface = new PointerConstraintsV1Interface(m_display, m_display);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy interfaceAnnouncedSpy(&registry, &KWayland::Client::Registry::interfaceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor =
        registry.createCompositor(registry.interface(KWayland::Client::Registry::Interface::Compositor).name, registry.interface(KWayland::Client::Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_pointerConstraints = registry.createPointerConstraints(registry.interface(KWayland::Client::Registry::Interface::PointerConstraintsUnstableV1).name,
                                                             registry.interface(KWayland::Client::Registry::Interface::PointerConstraintsUnstableV1).version,
                                                             this);
    QVERIFY(m_pointerConstraints);
    QVERIFY(m_pointerConstraints->isValid());

    m_seat = registry.createSeat(registry.interface(KWayland::Client::Registry::Interface::Seat).name, registry.interface(KWayland::Client::Registry::Interface::Seat).version, this);
    QVERIFY(m_seat);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.wait());
    m_pointer = m_seat->createPointer(this);
    QVERIFY(m_pointer);
}

void TestPointerConstraints::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
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

    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_compositorInterface = nullptr;
    m_seatInterface = nullptr;
    m_pointerConstraintsInterface = nullptr;
}

void TestPointerConstraints::testLockPointer_data()
{
    QTest::addColumn<KWayland::Client::PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<LockedPointerV1Interface::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << KWayland::Client::PointerConstraints::LifeTime::Persistent << LockedPointerV1Interface::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << KWayland::Client::PointerConstraints::LifeTime::OneShot << LockedPointerV1Interface::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testLockPointer()
{
    // this test verifies the basic interaction for lock pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    QVERIFY(!serverSurface->lockedPointer());
    QVERIFY(!serverSurface->confinedPointer());

    // now create the locked pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QFETCH(KWayland::Client::PointerConstraints::LifeTime, clientLifeTime);
    std::unique_ptr<KWayland::Client::LockedPointer> lockedPointer(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy lockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::locked);
    QSignalSpy unlockedSpy(lockedPointer.get(), &KWayland::Client::LockedPointer::unlocked);
    QVERIFY(lockedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverLockedPointer = serverSurface->lockedPointer();
    QVERIFY(serverLockedPointer);
    QVERIFY(!serverSurface->confinedPointer());

    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->region(), QRegion());
    QFETCH(LockedPointerV1Interface::LifeTime, serverLifeTime);
    QCOMPARE(serverLockedPointer->lifeTime(), serverLifeTime);
    // setting to unlocked now should not trigger an unlocked spy
    serverLockedPointer->setLocked(false);
    QVERIFY(!unlockedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverLockedPointer, &QObject::destroyed);
    QSignalSpy regionChangedSpy(serverLockedPointer, &LockedPointerV1Interface::regionChanged);
    lockedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    lockedPointer->setRegion(nullptr);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverLockedPointer->region(), QRegion());

    // let's lock the surface
    QSignalSpy lockedChangedSpy(serverLockedPointer, &LockedPointerV1Interface::lockedChanged);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QSignalSpy pointerMotionSpy(m_pointer, &KWayland::Client::Pointer::motion);
    m_seatInterface->notifyPointerMotion(QPoint(0, 1));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(pointerMotionSpy.wait());

    serverLockedPointer->setLocked(true);
    QCOMPARE(serverLockedPointer->isLocked(), true);
    m_seatInterface->notifyPointerMotion(QPoint(1, 1));
    m_seatInterface->notifyPointerFrame();
    QCOMPARE(lockedChangedSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QVERIFY(lockedSpy.isEmpty());
    QVERIFY(lockedSpy.wait());
    QVERIFY(unlockedSpy.isEmpty());

    const QPointF hint = QPointF(1.5, 0.5);
    QSignalSpy hintChangedSpy(serverLockedPointer, &LockedPointerV1Interface::cursorPositionHintChanged);
    lockedPointer->setCursorPositionHint(hint);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(hintChangedSpy.wait());
    QCOMPARE(serverLockedPointer->cursorPositionHint(), hint);

    // and unlock again
    serverLockedPointer->setLocked(false);
    QCOMPARE(serverLockedPointer->isLocked(), false);
    QCOMPARE(serverLockedPointer->cursorPositionHint(), QPointF(-1., -1.));
    QCOMPARE(lockedChangedSpy.count(), 2);
    QTEST(bool(serverSurface->lockedPointer()), "hasConstraintAfterUnlock");
    QFETCH(int, pointerChangedCount);
    QCOMPARE(pointerConstraintsChangedSpy.count(), pointerChangedCount);
    QVERIFY(unlockedSpy.wait());
    QCOMPARE(unlockedSpy.count(), 1);
    QCOMPARE(lockedSpy.count(), 1);

    // now motion should work again
    m_seatInterface->notifyPointerMotion(QPoint(0, 1));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(pointerMotionSpy.wait());
    QCOMPARE(pointerMotionSpy.count(), 2);

    lockedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

void TestPointerConstraints::testConfinePointer_data()
{
    QTest::addColumn<KWayland::Client::PointerConstraints::LifeTime>("clientLifeTime");
    QTest::addColumn<ConfinedPointerV1Interface::LifeTime>("serverLifeTime");
    QTest::addColumn<bool>("hasConstraintAfterUnlock");
    QTest::addColumn<int>("pointerChangedCount");

    QTest::newRow("persistent") << KWayland::Client::PointerConstraints::LifeTime::Persistent << ConfinedPointerV1Interface::LifeTime::Persistent << true << 1;
    QTest::newRow("oneshot") << KWayland::Client::PointerConstraints::LifeTime::OneShot << ConfinedPointerV1Interface::LifeTime::OneShot << false << 2;
}

void TestPointerConstraints::testConfinePointer()
{
    // this test verifies the basic interaction for confined pointer
    // first create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    QVERIFY(!serverSurface->lockedPointer());
    QVERIFY(!serverSurface->confinedPointer());

    // now create the confined pointer
    QSignalSpy pointerConstraintsChangedSpy(serverSurface, &SurfaceInterface::pointerConstraintsChanged);
    QFETCH(KWayland::Client::PointerConstraints::LifeTime, clientLifeTime);
    std::unique_ptr<KWayland::Client::ConfinedPointer> confinedPointer(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, clientLifeTime));
    QSignalSpy confinedSpy(confinedPointer.get(), &KWayland::Client::ConfinedPointer::confined);
    QSignalSpy unconfinedSpy(confinedPointer.get(), &KWayland::Client::ConfinedPointer::unconfined);
    QVERIFY(confinedPointer->isValid());
    QVERIFY(pointerConstraintsChangedSpy.wait());

    auto serverConfinedPointer = serverSurface->confinedPointer();
    QVERIFY(serverConfinedPointer);
    QVERIFY(!serverSurface->lockedPointer());

    QCOMPARE(serverConfinedPointer->isConfined(), false);
    QCOMPARE(serverConfinedPointer->region(), QRegion());
    QFETCH(ConfinedPointerV1Interface::LifeTime, serverLifeTime);
    QCOMPARE(serverConfinedPointer->lifeTime(), serverLifeTime);
    // setting to unconfined now should not trigger an unconfined spy
    serverConfinedPointer->setConfined(false);
    QVERIFY(!unconfinedSpy.wait(500));

    // try setting a region
    QSignalSpy destroyedSpy(serverConfinedPointer, &QObject::destroyed);
    QSignalSpy regionChangedSpy(serverConfinedPointer, &ConfinedPointerV1Interface::regionChanged);
    confinedPointer->setRegion(m_compositor->createRegion(QRegion(0, 5, 10, 20), m_compositor));
    // it's double buffered
    QVERIFY(!regionChangedSpy.wait(500));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion(0, 5, 10, 20));
    // and unset region again
    confinedPointer->setRegion(nullptr);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(regionChangedSpy.wait());
    QCOMPARE(serverConfinedPointer->region(), QRegion());

    // let's confine the surface
    QSignalSpy confinedChangedSpy(serverConfinedPointer, &ConfinedPointerV1Interface::confinedChanged);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
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
    QTEST(bool(serverSurface->confinedPointer()), "hasConstraintAfterUnlock");
    QFETCH(int, pointerChangedCount);
    QCOMPARE(pointerConstraintsChangedSpy.count(), pointerChangedCount);
    QVERIFY(unconfinedSpy.wait());
    QCOMPARE(unconfinedSpy.count(), 1);
    QCOMPARE(confinedSpy.count(), 1);

    confinedPointer.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(pointerConstraintsChangedSpy.count(), 2);
}

enum class Constraint {
    Lock,
    Confine,
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
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QFETCH(Constraint, firstConstraint);
    std::unique_ptr<KWayland::Client::ConfinedPointer> confinedPointer;
    std::unique_ptr<KWayland::Client::LockedPointer> lockedPointer;
    switch (firstConstraint) {
    case Constraint::Lock:
        lockedPointer.reset(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer.reset(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
        break;
    default:
        Q_UNREACHABLE();
    }
    QVERIFY(confinedPointer || lockedPointer);

    QSignalSpy errorSpy(m_connection, &KWayland::Client::ConnectionThread::errorOccurred);
    QFETCH(Constraint, secondConstraint);
    std::unique_ptr<KWayland::Client::ConfinedPointer> confinedPointer2;
    std::unique_ptr<KWayland::Client::LockedPointer> lockedPointer2;
    switch (secondConstraint) {
    case Constraint::Lock:
        lockedPointer2.reset(m_pointerConstraints->lockPointer(surface.get(), m_pointer, nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
        break;
    case Constraint::Confine:
        confinedPointer2.reset(m_pointerConstraints->confinePointer(surface.get(), m_pointer, nullptr, KWayland::Client::PointerConstraints::LifeTime::OneShot));
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
