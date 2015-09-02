/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include <QtTest/QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/output.h"
#include "../../src/client/seat.h"
#include "../../src/client/shell.h"
#include "../../src/client/subcompositor.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/output_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/shell_interface.h"
#include "../../src/server/blur_interface.h"
#include "../../src/server/contrast_interface.h"
#include "../../src/server/subcompositor_interface.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandRegistry : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandRegistry(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testBindCompositor();
    void testBindShell();
    void testBindOutput();
    void testBindShm();
    void testBindSeat();
    void testBindSubCompositor();
    void testBindDataDeviceManager();
    void testBindBlurManager();
    void testBindContrastManager();
    void testGlobalSync();
    void testGlobalSyncThreaded();
    void testRemoval();
    void testDestroy();
    void testAnnounceMultiple();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositor;
    KWayland::Server::OutputInterface *m_output;
    KWayland::Server::SeatInterface *m_seat;
    KWayland::Server::ShellInterface *m_shell;
    KWayland::Server::SubCompositorInterface *m_subcompositor;
    KWayland::Server::DataDeviceManagerInterface *m_dataDeviceManager;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-registry-0");

TestWaylandRegistry::TestWaylandRegistry(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositor(nullptr)
    , m_output(nullptr)
    , m_seat(nullptr)
    , m_shell(nullptr)
    , m_subcompositor(nullptr)
    , m_dataDeviceManager(nullptr)
{
}

void TestWaylandRegistry::init()
{
    m_display = new KWayland::Server::Display();
    m_display->setSocketName(s_socketName);
    m_display->start();
    m_display->createShm();
    m_compositor = m_display->createCompositor();
    m_compositor->create();
    m_output = m_display->createOutput();
    m_output->create();
    m_seat = m_display->createSeat();
    m_seat->create();
    m_shell = m_display->createShell();
    m_shell->create();
    m_subcompositor = m_display->createSubCompositor();
    m_subcompositor->create();
    m_dataDeviceManager = m_display->createDataDeviceManager();
    m_dataDeviceManager->create();
    m_display->createBlurManager(this)->create();
    m_display->createContrastManager(this)->create();
}

void TestWaylandRegistry::cleanup()
{
    delete m_display;
    m_display = nullptr;
}

void TestWaylandRegistry::testCreate()
{
    KWayland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    KWayland::Client::Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.release();
    QVERIFY(!registry.isValid());
}

#define TEST_BIND(iface, signalName, bindMethod, destroyFunction) \
    KWayland::Client::ConnectionThread connection; \
    QSignalSpy connectedSpy(&connection, SIGNAL(connected())); \
    connection.setSocketName(s_socketName); \
    connection.initConnection(); \
    QVERIFY(connectedSpy.wait()); \
    \
    KWayland::Client::Registry registry; \
    /* before registry is created, we cannot bind the interface*/ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    QVERIFY(!registry.isValid()); \
    registry.create(&connection); \
    QVERIFY(registry.isValid()); \
    /* created but not yet connected still results in no bind */ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    /* interface information should be empty */ \
    QVERIFY(registry.interfaces(iface).isEmpty()); \
    QCOMPARE(registry.interface(iface).name, 0u); \
    QCOMPARE(registry.interface(iface).version, 0u); \
    \
    /* now let's register */ \
    QSignalSpy announced(&registry, signalName); \
    QVERIFY(announced.isValid()); \
    registry.setup(); \
    wl_display_flush(connection.display()); \
    QVERIFY(announced.wait()); \
    const quint32 name = announced.first().first().value<quint32>(); \
    const quint32 version = announced.first().last().value<quint32>(); \
    QCOMPARE(registry.interfaces(iface).count(), 1); \
    QCOMPARE(registry.interfaces(iface).first().name, name); \
    QCOMPARE(registry.interfaces(iface).first().version, version); \
    QCOMPARE(registry.interface(iface).name, name); \
    QCOMPARE(registry.interface(iface).version, version); \
    \
    /* registry should know about the interface now */ \
    QVERIFY(registry.hasInterface(iface)); \
    QVERIFY(!registry.bindMethod(name+1, version)); \
    QVERIFY(registry.bindMethod(name, version+1)); \
    auto *c = registry.bindMethod(name, version); \
    QVERIFY(c); \
    destroyFunction(c); \

void TestWaylandRegistry::testBindCompositor()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Compositor, SIGNAL(compositorAnnounced(quint32,quint32)), bindCompositor, wl_compositor_destroy)
}

void TestWaylandRegistry::testBindShell()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Shell, SIGNAL(shellAnnounced(quint32,quint32)), bindShell, free)
}

void TestWaylandRegistry::testBindOutput()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Output, SIGNAL(outputAnnounced(quint32,quint32)), bindOutput, wl_output_destroy)
}

void TestWaylandRegistry::testBindSeat()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Seat, SIGNAL(seatAnnounced(quint32,quint32)), bindSeat, wl_seat_destroy)
}

void TestWaylandRegistry::testBindShm()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Shm, SIGNAL(shmAnnounced(quint32,quint32)), bindShm, wl_shm_destroy)
}

void TestWaylandRegistry::testBindSubCompositor()
{
    TEST_BIND(KWayland::Client::Registry::Interface::SubCompositor, SIGNAL(subCompositorAnnounced(quint32,quint32)), bindSubCompositor, wl_subcompositor_destroy)
}

void TestWaylandRegistry::testBindDataDeviceManager()
{
    TEST_BIND(KWayland::Client::Registry::Interface::DataDeviceManager, SIGNAL(dataDeviceManagerAnnounced(quint32,quint32)), bindDataDeviceManager, wl_data_device_manager_destroy)
}

void TestWaylandRegistry::testBindBlurManager()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Blur, SIGNAL(blurAnnounced(quint32,quint32)), bindBlurManager, free)
}

void TestWaylandRegistry::testBindContrastManager()
{
    TEST_BIND(KWayland::Client::Registry::Interface::Contrast, SIGNAL(contrastAnnounced(quint32,quint32)), bindContrastManager, free)
}

#undef TEST_BIND

void TestWaylandRegistry::testRemoval()
{
    using namespace KWayland::Client;
    KWayland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
        [&connection] {
            wl_display_flush(connection.display());
        }
    );

    KWayland::Client::Registry registry;
    QSignalSpy shmAnnouncedSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    QVERIFY(shmAnnouncedSpy.isValid());
    QSignalSpy compositorAnnouncedSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    QVERIFY(outputAnnouncedSpy.isValid());
    QSignalSpy shellAnnouncedSpy(&registry, SIGNAL(shellAnnounced(quint32,quint32)));
    QVERIFY(shellAnnouncedSpy.isValid());
    QSignalSpy seatAnnouncedSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    QVERIFY(seatAnnouncedSpy.isValid());
    QSignalSpy subCompositorAnnouncedSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    QVERIFY(subCompositorAnnouncedSpy.isValid());

    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();

    QVERIFY(shmAnnouncedSpy.wait());
    QVERIFY(!compositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputAnnouncedSpy.isEmpty());
    QVERIFY(!shellAnnouncedSpy.isEmpty());
    QVERIFY(!seatAnnouncedSpy.isEmpty());
    QVERIFY(!subCompositorAnnouncedSpy.isEmpty());

    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::Output));
    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::Seat));
    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::Shell));
    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::Shm));
    QVERIFY(registry.hasInterface(KWayland::Client::Registry::Interface::SubCompositor));
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::FullscreenShell));

    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::Compositor).isEmpty());
    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::Output).isEmpty());
    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::Seat).isEmpty());
    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::Shell).isEmpty());
    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::Shm).isEmpty());
    QVERIFY(!registry.interfaces(KWayland::Client::Registry::Interface::SubCompositor).isEmpty());
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::FullscreenShell).isEmpty());

    QSignalSpy seatRemovedSpy(&registry, SIGNAL(seatRemoved(quint32)));
    QVERIFY(seatRemovedSpy.isValid());

    Seat *seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version, &registry);
    Shell *shell = registry.createShell(registry.interface(Registry::Interface::Shell).name, registry.interface(Registry::Interface::Shell).version, &registry);
    Output *output = registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);
    Compositor *compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version, &registry);
    SubCompositor *subcompositor = registry.createSubCompositor(registry.interface(Registry::Interface::SubCompositor).name, registry.interface(Registry::Interface::SubCompositor).version, &registry);
    connection.flush();
    m_display->dispatchEvents();
    QSignalSpy seatObjectRemovedSpy(seat, &Seat::removed);
    QVERIFY(seatObjectRemovedSpy.isValid());
    QSignalSpy shellObjectRemovedSpy(shell, &Shell::removed);
    QVERIFY(shellObjectRemovedSpy.isValid());
    QSignalSpy outputObjectRemovedSpy(output, &Output::removed);
    QVERIFY(outputObjectRemovedSpy.isValid());
    QSignalSpy compositorObjectRemovedSpy(compositor, &Compositor::removed);
    QVERIFY(compositorObjectRemovedSpy.isValid());
    QSignalSpy subcompositorObjectRemovedSpy(subcompositor, &SubCompositor::removed);
    QVERIFY(subcompositorObjectRemovedSpy.isValid());

    delete m_seat;
    QVERIFY(seatRemovedSpy.wait());
    QCOMPARE(seatRemovedSpy.first().first(), seatAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::Seat));
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::Seat).isEmpty());
    QCOMPARE(seatObjectRemovedSpy.count(), 1);

    QSignalSpy shellRemovedSpy(&registry, SIGNAL(shellRemoved(quint32)));
    QVERIFY(shellRemovedSpy.isValid());

    delete m_shell;
    QVERIFY(shellRemovedSpy.wait());
    QCOMPARE(shellRemovedSpy.first().first(), shellAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::Shell));
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::Shell).isEmpty());
    QCOMPARE(shellObjectRemovedSpy.count(), 1);

    QSignalSpy outputRemovedSpy(&registry, SIGNAL(outputRemoved(quint32)));
    QVERIFY(outputRemovedSpy.isValid());

    delete m_output;
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first(), outputAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::Output));
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::Output).isEmpty());
    QCOMPARE(outputObjectRemovedSpy.count(), 1);

    QSignalSpy compositorRemovedSpy(&registry, SIGNAL(compositorRemoved(quint32)));
    QVERIFY(compositorRemovedSpy.isValid());

    delete m_compositor;
    QVERIFY(compositorRemovedSpy.wait());
    QCOMPARE(compositorRemovedSpy.first().first(), compositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::Compositor));
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::Compositor).isEmpty());
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);

    QSignalSpy subCompositorRemovedSpy(&registry, SIGNAL(subCompositorRemoved(quint32)));
    QVERIFY(subCompositorRemovedSpy.isValid());

    delete m_subcompositor;
    QVERIFY(subCompositorRemovedSpy.wait());
    QCOMPARE(subCompositorRemovedSpy.first().first(), subCompositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWayland::Client::Registry::Interface::SubCompositor));
    QVERIFY(registry.interfaces(KWayland::Client::Registry::Interface::SubCompositor).isEmpty());
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);

    // cannot test shmRemoved as there is no functionality for it

    // verify everything has been removed only once
    QCOMPARE(seatObjectRemovedSpy.count(), 1);
    QCOMPARE(shellObjectRemovedSpy.count(), 1);
    QCOMPARE(outputObjectRemovedSpy.count(), 1);
    QCOMPARE(compositorObjectRemovedSpy.count(), 1);
    QCOMPARE(subcompositorObjectRemovedSpy.count(), 1);
}

void TestWaylandRegistry::testDestroy()
{
    using namespace KWayland::Client;
    KWayland::Client::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();
    QVERIFY(registry.isValid());

    connect(&connection, &ConnectionThread::connectionDied, &registry, &Registry::destroy);

    QSignalSpy connectionDiedSpy(&connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the registry should be destroyed;
    QVERIFY(!registry.isValid());

    // calling destroy again should not fail
    registry.destroy();
}

void TestWaylandRegistry::testGlobalSync()
{
    using namespace KWayland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    Registry registry;
    QSignalSpy syncSpy(&registry, SIGNAL(interfacesAnnounced()));
    // Most simple case: don't even use the ConnectionThread,
    // just its display.
    registry.create(connection.display());
    registry.setup();
    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);
    registry.destroy();
}

void TestWaylandRegistry::testGlobalSyncThreaded()
{
    // More complex case, use a ConnectionThread, in a different Thread,
    // and our own EventQueue
    using namespace KWayland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QThread thread;
    connection.moveToThread(&thread);
    thread.start();

    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.initConnection();

    QVERIFY(connectedSpy.wait());
    EventQueue queue;
    queue.setup(&connection);

    Registry registry;
    QSignalSpy syncSpy(&registry, SIGNAL(interfacesAnnounced()));
    registry.setEventQueue(&queue);
    registry.create(&connection);
    registry.setup();

    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);
    registry.destroy();

    thread.quit();
    thread.wait();
}

void TestWaylandRegistry::testAnnounceMultiple()
{
    using namespace KWayland::Client;
    ConnectionThread connection;
    connection.setSocketName(s_socketName);
    QSignalSpy connectedSpy(&connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    connection.initConnection();
    QVERIFY(connectedSpy.wait());
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
        [&connection] {
            wl_display_flush(connection.display());
        }
    );

    Registry registry;
    QSignalSpy syncSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(syncSpy.isValid());
    // Most simple case: don't even use the ConnectionThread,
    // just its display.
    registry.create(connection.display());
    registry.setup();
    QVERIFY(syncSpy.wait());
    QCOMPARE(syncSpy.count(), 1);

    // we should have one output now
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 1);

    QSignalSpy outputAnnouncedSpy(&registry, &Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());
    m_display->createOutput()->create();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.first().last().value<quint32>());

    auto output = m_display->createOutput();
    output->create();
    QVERIFY(outputAnnouncedSpy.wait());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 3);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.last().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.last().last().value<quint32>());

    QSignalSpy outputRemovedSpy(&registry, &Registry::outputRemoved);
    QVERIFY(outputRemovedSpy.isValid());
    delete output;
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first().value<quint32>(), outputAnnouncedSpy.last().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).count(), 2);
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interfaces(Registry::Interface::Output).last().version, outputAnnouncedSpy.first().last().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).name, outputAnnouncedSpy.first().first().value<quint32>());
    QCOMPARE(registry.interface(Registry::Interface::Output).version, outputAnnouncedSpy.first().last().value<quint32>());
}

QTEST_GUILESS_MAIN(TestWaylandRegistry)
#include "test_wayland_registry.moc"
