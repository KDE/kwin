/********************************************************************
Copyright 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

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
#include <QHash>
#include <QThread>
#include <QtTest>
// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/tablet_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"

#include "qwayland-tablet-unstable-v2.h"

using namespace KWayland::Server;

class Tablet : public QtWayland::zwp_tablet_v2
{
public:
    Tablet(::zwp_tablet_v2 *t)
        : QtWayland::zwp_tablet_v2(t)
    {
    }
};

class Tool : public QObject, public QtWayland::zwp_tablet_tool_v2
{
    Q_OBJECT
public:
    Tool(::zwp_tablet_tool_v2 *t)
        : QtWayland::zwp_tablet_tool_v2(t)
    {
    }

    void zwp_tablet_tool_v2_proximity_in(uint32_t /*serial*/, struct ::zwp_tablet_v2 * /*tablet*/, struct ::wl_surface *surface) override
    {
        surfaceApproximated[surface]++;
    }

    void zwp_tablet_tool_v2_frame(uint32_t time) override
    {
        Q_EMIT frame(time);
    }

    QHash<struct ::wl_surface *, int> surfaceApproximated;
Q_SIGNALS:
    void frame(quint32 time);
};

class TabletSeat : public QObject, public QtWayland::zwp_tablet_seat_v2
{
    Q_OBJECT
public:
    TabletSeat(::zwp_tablet_seat_v2 *seat)
        : QtWayland::zwp_tablet_seat_v2(seat)
    {
    }

    void zwp_tablet_seat_v2_tablet_added(struct ::zwp_tablet_v2 *id) override
    {
        m_tablets << new Tablet(id);
        Q_EMIT tabletAdded();
    }
    void zwp_tablet_seat_v2_tool_added(struct ::zwp_tablet_tool_v2 *id) override
    {
        m_tools << new Tool(id);
        Q_EMIT toolAdded();
    }

    QVector<Tablet *> m_tablets;
    QVector<Tool *> m_tools;

Q_SIGNALS:
    void toolAdded();
    void tabletAdded();
};

class TestTabletInterface : public QObject
{
    Q_OBJECT
public:
    TestTabletInterface()
    {
    }
    ~TestTabletInterface() override;

private Q_SLOTS:
    void initTestCase();
    void testAdd();
    void testInteractSimple();
    void testInteractSurfaceChange();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    QThread *m_thread;
    Display m_display;
    SeatInterface *m_seat;
    CompositorInterface *m_serverCompositor;

    TabletSeat *m_tabletSeatClient = nullptr;
    TabletManagerInterface *m_tabletManager;

    TabletInterface *m_tablet;
    TabletToolInterface *m_tool;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-tablet-test-0");

void TestTabletInterface::initTestCase()
{
    m_display.setSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = m_display.createSeat(this);
    m_seat->create();
    m_serverCompositor = m_display.createCompositor(this);
    m_serverCompositor->create();
    m_tabletManager = m_display.createTabletManagerInterface(this);
    QVERIFY(m_serverCompositor->isValid());
    connect(m_serverCompositor, &CompositorInterface::surfaceCreated, this, [this](SurfaceInterface *surface) {
        m_surfaces += surface;
    });

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwp_tablet_manager_v2") {
            auto tabletClient = new QtWayland::zwp_tablet_manager_v2(registry->registry(), name, version);
            auto _seat = tabletClient->get_tablet_seat(*m_clientSeat);
            m_tabletSeatClient = new TabletSeat(_seat);
        }
    });
    connect(registry, &KWayland::Client::Registry::seatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_clientSeat = registry->createSeat(name, version);
    });
    registry->setEventQueue(m_queue);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    wl_display_flush(m_connection->display());

    QVERIFY(compositorSpy.wait());
    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());

    QSignalSpy surfaceSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    for (int i = 0; i < 3; ++i) {
        m_clientCompositor->createSurface(this);
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_surfaces.count() == 3);
    QVERIFY(m_tabletSeatClient);
}

TestTabletInterface::~TestTabletInterface()
{
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_tabletSeatClient;
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestTabletInterface::testAdd()
{
    TabletSeatInterface *seatInterface = m_tabletManager->seat(m_seat);
    QVERIFY(seatInterface);

    QSignalSpy tabletSpy(m_tabletSeatClient, &TabletSeat::tabletAdded);
    m_tablet = seatInterface->addTablet(1, 2, QStringLiteral("event33"), QStringLiteral("my tablet"), {QStringLiteral("/test/event33")});
    QVERIFY(m_tablet);
    QVERIFY(tabletSpy.wait() || tabletSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tablets.count(), 1);

    QSignalSpy toolSpy(m_tabletSeatClient, &TabletSeat::toolAdded);
    m_tool = seatInterface->addTool(KWayland::Server::TabletToolInterface::Pen, 0, 0, {TabletToolInterface::Tilt, TabletToolInterface::Pressure});
    QVERIFY(m_tool);
    QVERIFY(toolSpy.wait() || toolSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tools.count(), 1);

    QVERIFY(!m_tool->isClientSupported()); //There's no surface in it yet
    m_tool->setCurrentSurface(nullptr);
    QVERIFY(!m_tool->isClientSupported()); //There's no surface in it

    QCOMPARE(m_surfaces.count(), 3);
    for (SurfaceInterface *surface : m_surfaces) {
        m_tool->setCurrentSurface(surface);
    }
    m_tool->setCurrentSurface(nullptr);
}

static uint s_serial = 0;
void TestTabletInterface::testInteractSimple()
{
    QSignalSpy frameSpy(m_tabletSeatClient->m_tools[0], &Tool::frame);

    QVERIFY(!m_tool->isClientSupported());
    m_tool->setCurrentSurface(m_surfaces[0]);
    QVERIFY(m_tool->isClientSupported() && m_tablet->isSurfaceSupported(m_surfaces[0]));
    m_tool->sendProximityIn(m_tablet);
    m_tool->sendPressure(0);
    m_tool->sendFrame(s_serial++);
    m_tool->sendMotion({3, 3});
    m_tool->sendFrame(s_serial++);
    m_tool->sendProximityOut();
    QVERIFY(m_tool->isClientSupported());
    m_tool->sendFrame(s_serial++);
    QVERIFY(!m_tool->isClientSupported());

    QVERIFY(frameSpy.wait(500));
    QCOMPARE(m_tabletSeatClient->m_tools[0]->surfaceApproximated.count(), 1);
}

void TestTabletInterface::testInteractSurfaceChange()
{
    m_tabletSeatClient->m_tools[0]->surfaceApproximated.clear();
    QSignalSpy frameSpy(m_tabletSeatClient->m_tools[0], &Tool::frame);
    QVERIFY(!m_tool->isClientSupported());
    m_tool->setCurrentSurface(m_surfaces[0]);
    QVERIFY(m_tool->isClientSupported() && m_tablet->isSurfaceSupported(m_surfaces[0]));
    m_tool->sendProximityIn(m_tablet);
    m_tool->sendPressure(0);
    m_tool->sendFrame(s_serial++);

    m_tool->setCurrentSurface(m_surfaces[1]);
    QVERIFY(m_tool->isClientSupported());

    m_tool->sendMotion({3, 3});
    m_tool->sendFrame(s_serial++);
    m_tool->sendProximityOut();
    QVERIFY(m_tool->isClientSupported());
    m_tool->sendFrame(s_serial++);
    QVERIFY(!m_tool->isClientSupported());

    QVERIFY(frameSpy.wait(500));
    QCOMPARE(m_tabletSeatClient->m_tools[0]->surfaceApproximated.count(), 2);
}

QTEST_GUILESS_MAIN(TestTabletInterface)
#include "test_tablet_interface.moc"
