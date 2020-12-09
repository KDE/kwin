/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/idle.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/idle_interface.h"
#include "../../src/server/seat_interface.h"

using namespace KWayland::Client;
using namespace KWaylandServer;

class IdleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testTimeout();
    void testSimulateUserActivity();
    void testServerSimulateUserActivity();
    void testIdleInhibit();
    void testIdleInhibitBlocksTimeout();

private:
    Display *m_display = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    IdleInterface *m_idleInterface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Seat *m_seat = nullptr;
    Idle *m_idle = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-idle-0");

void IdleTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = new SeatInterface(m_display);
    m_seatInterface->setName(QStringLiteral("seat0"));
    m_seatInterface->create();
    m_idleInterface = new IdleInterface(m_display);

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
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name,
                                 registry.interface(Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat->isValid());
    m_idle = registry.createIdle(registry.interface(Registry::Interface::Idle).name,
                                 registry.interface(Registry::Interface::Idle).version,
                                 this);
    QVERIFY(m_idle->isValid());
}

void IdleTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_idle)
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
    m_idleInterface = nullptr;
    m_seatInterface = nullptr;
}

void IdleTest::testTimeout()
{
    // this test verifies the basic functionality of a timeout, that it gets fired
    // and that it resumes from idle, etc.
    QScopedPointer<IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.data(), &IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.data(), &IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());

    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(500));
    // the default of 5 sec will now pass
    QVERIFY(idleSpy.wait());

    // simulate some activity
    QVERIFY(resumedFormIdleSpy.isEmpty());
    m_seatInterface->setTimestamp(1);
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    m_display->dispatchEvents();
}

void IdleTest::testSimulateUserActivity()
{
    // this test verifies that simulate user activity doesn't fire the timer
    QScopedPointer<IdleTimeout> timeout(m_idle->getTimeout(6000, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.data(), &IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.data(), &IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());
    m_connection->flush();

    QTest::qWait(4000);
    timeout->simulateUserActivity();
    // waiting default five sec should fail
    QVERIFY(!idleSpy.wait());
    // another 2 sec should fire
    QVERIFY(idleSpy.wait(2000));

    // now simulating user activity should emit a resumedFromIdle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    timeout->simulateUserActivity();
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    m_display->dispatchEvents();
}

void IdleTest::testServerSimulateUserActivity()
{
    // this test verifies that simulate user activity doesn't fire the timer
    QScopedPointer<IdleTimeout> timeout(m_idle->getTimeout(6000, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.data(), &IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.data(), &IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());
    m_connection->flush();

    QTest::qWait(4000);
    m_idleInterface->simulateUserActivity();
    // waiting default five sec should fail
    QVERIFY(!idleSpy.wait());
    // another 2 sec should fire
    QVERIFY(idleSpy.wait(2000));

    // now simulating user activity should emit a resumedFromIdle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    m_idleInterface->simulateUserActivity();
    QVERIFY(resumedFormIdleSpy.wait());

    timeout.reset();
    m_connection->flush();
    m_display->dispatchEvents();
}

void IdleTest::testIdleInhibit()
{
    QCOMPARE(m_idleInterface->isInhibited(), false);
    QSignalSpy idleInhibitedSpy(m_idleInterface, &IdleInterface::inhibitedChanged);
    QVERIFY(idleInhibitedSpy.isValid());
    m_idleInterface->inhibit();
    QCOMPARE(m_idleInterface->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    m_idleInterface->inhibit();
    QCOMPARE(m_idleInterface->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    m_idleInterface->uninhibit();
    QCOMPARE(m_idleInterface->isInhibited(), true);
    QCOMPARE(idleInhibitedSpy.count(), 1);
    m_idleInterface->uninhibit();
    QCOMPARE(m_idleInterface->isInhibited(), false);
    QCOMPARE(idleInhibitedSpy.count(), 2);
}

void IdleTest::testIdleInhibitBlocksTimeout()
{
    // this test verifies that a timeout does not fire when the system is inhibited

    // so first inhibit
    QCOMPARE(m_idleInterface->isInhibited(), false);
    m_idleInterface->inhibit();

    QScopedPointer<IdleTimeout> timeout(m_idle->getTimeout(1, m_seat));
    QVERIFY(timeout->isValid());
    QSignalSpy idleSpy(timeout.data(), &IdleTimeout::idle);
    QVERIFY(idleSpy.isValid());
    QSignalSpy resumedFormIdleSpy(timeout.data(), &IdleTimeout::resumeFromIdle);
    QVERIFY(resumedFormIdleSpy.isValid());

    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(500));
    // the default of 5 sec won't pass
    QVERIFY(!idleSpy.wait());

    // simulate some activity
    QVERIFY(resumedFormIdleSpy.isEmpty());
    m_seatInterface->setTimestamp(1);
    // resume from idle should not fire
    QVERIFY(!resumedFormIdleSpy.wait());

    // let's uninhibit
    m_idleInterface->uninhibit();
    QCOMPARE(m_idleInterface->isInhibited(), false);
    // we requested a timeout of 1 msec, but the minimum the server sets is 5 sec
    QVERIFY(!idleSpy.wait(500));
    // the default of 5 sec will now pass
    QVERIFY(idleSpy.wait());

    // if we inhibit now it will trigger a resume from idle
    QVERIFY(resumedFormIdleSpy.isEmpty());
    m_idleInterface->inhibit();
    QVERIFY(resumedFormIdleSpy.wait());

    // let's wait again just to verify that also inhibit for already existing IdleTimeout works
    QVERIFY(!idleSpy.wait(500));
    QVERIFY(!idleSpy.wait());
    QCOMPARE(idleSpy.count(), 1);

    timeout.reset();
    m_connection->flush();
    m_display->dispatchEvents();
}

QTEST_GUILESS_MAIN(IdleTest)
#include "test_idle.moc"
