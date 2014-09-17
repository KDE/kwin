/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../wayland_client/connection_thread.h"
#include "../../wayland_client/registry.h"
#include "../wayland_server/compositor_interface.h"
#include "../../wayland_server/display.h"
#include "../wayland_server/output_interface.h"
#include "../wayland_server/seat_interface.h"
#include "../wayland_server/shell_interface.h"
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
    void testRemoval();

private:
    KWin::WaylandServer::Display *m_display;
    KWin::WaylandServer::CompositorInterface *m_compositor;
    KWin::WaylandServer::OutputInterface *m_output;
    KWin::WaylandServer::SeatInterface *m_seat;
    KWin::WaylandServer::ShellInterface *m_shell;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-registry-0");

TestWaylandRegistry::TestWaylandRegistry(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositor(nullptr)
    , m_output(nullptr)
    , m_seat(nullptr)
    , m_shell(nullptr)
{
}

void TestWaylandRegistry::init()
{
    m_display = new KWin::WaylandServer::Display();
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
}

void TestWaylandRegistry::cleanup()
{
    delete m_display;
    m_display = nullptr;
}

void TestWaylandRegistry::testCreate()
{
    KWin::Wayland::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());

    KWin::Wayland::Registry registry;
    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    QVERIFY(registry.isValid());
    registry.release();
    QVERIFY(!registry.isValid());
}

#define TEST_BIND(interface, signalName, bindMethod, destroyFunction) \
    KWin::Wayland::ConnectionThread connection; \
    QSignalSpy connectedSpy(&connection, SIGNAL(connected())); \
    connection.setSocketName(s_socketName); \
    connection.initConnection(); \
    QVERIFY(connectedSpy.wait()); \
    \
    KWin::Wayland::Registry registry; \
    /* before registry is created, we cannot bind the interface*/ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    QVERIFY(!registry.isValid()); \
    registry.create(connection.display()); \
    QVERIFY(registry.isValid()); \
    /* created but not yet connected still results in no bind */ \
    QVERIFY(!registry.bindMethod(0, 0)); \
    \
    /* now lets register */ \
    QSignalSpy announced(&registry, signalName); \
    QVERIFY(announced.isValid()); \
    registry.setup(); \
    wl_display_flush(connection.display()); \
    QVERIFY(announced.wait()); \
    const quint32 name = announced.first().first().value<quint32>(); \
    const quint32 version = announced.first().last().value<quint32>(); \
    \
    /* registry should now about the interface now */ \
    QVERIFY(registry.hasInterface(interface)); \
    QVERIFY(!registry.bindMethod(name+1, version)); \
    QVERIFY(!registry.bindMethod(name, version+1)); \
    auto *c = registry.bindMethod(name, version); \
    QVERIFY(c); \
    destroyFunction(c); \

void TestWaylandRegistry::testBindCompositor()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Compositor, SIGNAL(compositorAnnounced(quint32,quint32)), bindCompositor, wl_compositor_destroy)
}

void TestWaylandRegistry::testBindShell()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Shell, SIGNAL(shellAnnounced(quint32,quint32)), bindShell, free)
}

void TestWaylandRegistry::testBindOutput()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Output, SIGNAL(outputAnnounced(quint32,quint32)), bindOutput, wl_output_destroy)
}

void TestWaylandRegistry::testBindSeat()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Seat, SIGNAL(seatAnnounced(quint32,quint32)), bindSeat, wl_seat_destroy)
}

void TestWaylandRegistry::testBindShm()
{
    TEST_BIND(KWin::Wayland::Registry::Interface::Shm, SIGNAL(shmAnnounced(quint32,quint32)), bindShm, wl_shm_destroy)
}

#undef TEST_BIND

void TestWaylandRegistry::testRemoval()
{
    KWin::Wayland::ConnectionThread connection;
    QSignalSpy connectedSpy(&connection, SIGNAL(connected()));
    connection.setSocketName(s_socketName);
    connection.initConnection();
    QVERIFY(connectedSpy.wait());
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, &connection,
        [&connection] {
            wl_display_flush(connection.display());
        }
    );

    KWin::Wayland::Registry registry;
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

    QVERIFY(!registry.isValid());
    registry.create(connection.display());
    registry.setup();

    QVERIFY(shmAnnouncedSpy.wait());
    QVERIFY(!compositorAnnouncedSpy.isEmpty());
    QVERIFY(!outputAnnouncedSpy.isEmpty());
    QVERIFY(!shellAnnouncedSpy.isEmpty());
    QVERIFY(!seatAnnouncedSpy.isEmpty());

    QVERIFY(registry.hasInterface(KWin::Wayland::Registry::Interface::Compositor));
    QVERIFY(registry.hasInterface(KWin::Wayland::Registry::Interface::Output));
    QVERIFY(registry.hasInterface(KWin::Wayland::Registry::Interface::Seat));
    QVERIFY(registry.hasInterface(KWin::Wayland::Registry::Interface::Shell));
    QVERIFY(registry.hasInterface(KWin::Wayland::Registry::Interface::Shm));
    QVERIFY(!registry.hasInterface(KWin::Wayland::Registry::Interface::FullscreenShell));

    QSignalSpy seatRemovedSpy(&registry, SIGNAL(seatRemoved(quint32)));
    QVERIFY(seatRemovedSpy.isValid());

    delete m_seat;
    QVERIFY(seatRemovedSpy.wait());
    QCOMPARE(seatRemovedSpy.first().first(), seatAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWin::Wayland::Registry::Interface::Seat));

    QSignalSpy shellRemovedSpy(&registry, SIGNAL(shellRemoved(quint32)));
    QVERIFY(shellRemovedSpy.isValid());

    delete m_shell;
    QVERIFY(shellRemovedSpy.wait());
    QCOMPARE(shellRemovedSpy.first().first(), shellAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWin::Wayland::Registry::Interface::Shell));

    QSignalSpy outputRemovedSpy(&registry, SIGNAL(outputRemoved(quint32)));
    QVERIFY(outputRemovedSpy.isValid());

    delete m_output;
    QVERIFY(outputRemovedSpy.wait());
    QCOMPARE(outputRemovedSpy.first().first(), outputAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWin::Wayland::Registry::Interface::Output));

    QSignalSpy compositorRemovedSpy(&registry, SIGNAL(compositorRemoved(quint32)));
    QVERIFY(compositorRemovedSpy.isValid());

    delete m_compositor;
    QVERIFY(compositorRemovedSpy.wait());
    QCOMPARE(compositorRemovedSpy.first().first(), compositorAnnouncedSpy.first().first());
    QVERIFY(!registry.hasInterface(KWin::Wayland::Registry::Interface::Compositor));

    // cannot test shmRemoved as there is no functionality for it
}

QTEST_MAIN(TestWaylandRegistry)
#include "test_wayland_registry.moc"
