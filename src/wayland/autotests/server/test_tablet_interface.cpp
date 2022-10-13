/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QHash>
#include <QThread>
#include <QtTest>
// WaylandServer
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"
#include "wayland/tablet_v2_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-tablet-unstable-v2.h"

using namespace KWaylandServer;

class Tablet : public QtWayland::zwp_tablet_v2
{
public:
    Tablet(::zwp_tablet_v2 *t)
        : QtWayland::zwp_tablet_v2(t)
    {
    }
};

class TabletPad : public QObject, public QtWayland::zwp_tablet_pad_v2
{
    Q_OBJECT
public:
    TabletPad(::zwp_tablet_pad_v2 *t)
        : QtWayland::zwp_tablet_pad_v2(t)
    {
    }

    void zwp_tablet_pad_v2_done() override
    {
        Q_ASSERT(!doneCalled);
        doneCalled = true;
    }

    void zwp_tablet_pad_v2_buttons(uint32_t buttons) override
    {
        Q_ASSERT(buttons == 1);
    }

    void zwp_tablet_pad_v2_enter(uint32_t /*serial*/, struct ::zwp_tablet_v2 * /*tablet*/, struct ::wl_surface *surface) override
    {
        m_currentSurface = surface;
    }

    void zwp_tablet_pad_v2_button(uint32_t /*time*/, uint32_t button, uint32_t state) override
    {
        buttonStates[m_currentSurface][button] = state;
        Q_EMIT buttonReceived();
    }

    ::wl_surface *m_currentSurface = nullptr;

    bool doneCalled = false;
    QHash<::wl_surface *, QHash<uint32_t, uint32_t>> buttonStates;

Q_SIGNALS:
    void buttonReceived();
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

    void zwp_tablet_seat_v2_pad_added(struct ::zwp_tablet_pad_v2 *id) override
    {
        m_pads << new TabletPad(id);
        Q_EMIT padAdded();
    }

    QVector<Tablet *> m_tablets;
    QVector<TabletPad *> m_pads;
    QVector<Tool *> m_tools;

Q_SIGNALS:
    void padAdded();
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
    void testAddPad();
    void testInteractSimple();
    void testInteractSurfaceChange();

private:
    std::unique_ptr<KWayland::Client::Registry> m_registry;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::Compositor> m_clientCompositor;
    std::unique_ptr<KWayland::Client::Seat> m_clientSeat;

    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<SeatInterface> m_seat;
    std::unique_ptr<CompositorInterface> m_serverCompositor;

    std::unique_ptr<TabletSeat> m_tabletSeatClient;
    std::unique_ptr<TabletManagerV2Interface> m_tabletManager;
    std::vector<std::unique_ptr<KWayland::Client::Surface>> m_surfacesClient;

    TabletV2Interface *m_tablet = nullptr;
    TabletPadV2Interface *m_tabletPad = nullptr;
    TabletToolV2Interface *m_tool = nullptr;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-tablet-test-0");

void TestTabletInterface::initTestCase()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_seat = std::make_unique<SeatInterface>(m_display.get());
    m_serverCompositor = std::make_unique<CompositorInterface>(m_display.get());
    m_tabletManager = std::make_unique<TabletManagerV2Interface>(m_display.get());

    connect(m_serverCompositor.get(), &CompositorInterface::surfaceCreated, this, [this](SurfaceInterface *surface) {
        m_surfaces += surface;
    });

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    m_registry = std::make_unique<KWayland::Client::Registry>();
    connect(m_registry.get(), &KWayland::Client::Registry::interfaceAnnounced, this, [this](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwp_tablet_manager_v2") {
            auto tabletClient = new QtWayland::zwp_tablet_manager_v2(m_registry->registry(), name, version);
            auto _seat = tabletClient->get_tablet_seat(*m_clientSeat);
            m_tabletSeatClient = std::make_unique<TabletSeat>(_seat);
        }
    });
    connect(m_registry.get(), &KWayland::Client::Registry::seatAnnounced, this, [this](quint32 name, quint32 version) {
        m_clientSeat.reset(m_registry->createSeat(name, version));
    });
    m_registry->setEventQueue(m_queue.get());
    QSignalSpy compositorSpy(m_registry.get(), &KWayland::Client::Registry::compositorAnnounced);
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();
    wl_display_flush(m_connection->display());

    QVERIFY(compositorSpy.wait());
    m_clientCompositor.reset(m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_clientCompositor->isValid());

    QSignalSpy surfaceSpy(m_serverCompositor.get(), &CompositorInterface::surfaceCreated);
    for (int i = 0; i < 3; ++i) {
        m_surfacesClient.push_back(std::unique_ptr<KWayland::Client::Surface>(m_clientCompositor->createSurface()));
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_surfaces.count() == 3);
    QVERIFY(m_tabletSeatClient);
}

TestTabletInterface::~TestTabletInterface()
{
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_surfacesClient.clear();
    m_clientCompositor.reset();
    m_clientSeat.reset();
    m_tabletSeatClient.reset();
    m_registry.reset();
    m_connection.reset();
    m_display.reset();
}

void TestTabletInterface::testAdd()
{
    TabletSeatV2Interface *seatInterface = m_tabletManager->seat(m_seat.get());
    QVERIFY(seatInterface);

    QSignalSpy tabletSpy(m_tabletSeatClient.get(), &TabletSeat::tabletAdded);
    m_tablet = seatInterface->addTablet(1, 2, QStringLiteral("event33"), QStringLiteral("my tablet"), {QStringLiteral("/test/event33")});
    QVERIFY(m_tablet);
    QVERIFY(tabletSpy.wait() || tabletSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tablets.count(), 1);

    QSignalSpy toolSpy(m_tabletSeatClient.get(), &TabletSeat::toolAdded);
    m_tool = seatInterface->addTool(KWaylandServer::TabletToolV2Interface::Pen, 0, 0, {TabletToolV2Interface::Tilt, TabletToolV2Interface::Pressure});
    QVERIFY(m_tool);
    QVERIFY(toolSpy.wait() || toolSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tools.count(), 1);

    QVERIFY(!m_tool->isClientSupported()); // There's no surface in it yet
    m_tool->setCurrentSurface(nullptr);
    QVERIFY(!m_tool->isClientSupported()); // There's no surface in it

    QCOMPARE(m_surfaces.count(), 3);
    for (SurfaceInterface *surface : m_surfaces) {
        m_tool->setCurrentSurface(surface);
    }
    m_tool->setCurrentSurface(nullptr);
}

void TestTabletInterface::testAddPad()
{
    TabletSeatV2Interface *seatInterface = m_tabletManager->seat(m_seat.get());
    QVERIFY(seatInterface);

    QSignalSpy tabletPadSpy(m_tabletSeatClient.get(), &TabletSeat::padAdded);
    m_tabletPad = seatInterface->addTabletPad(QStringLiteral("my tablet pad"), QStringLiteral("tabletpad"), {QStringLiteral("/test/event33")}, 1, 1, 1, 1, 0, m_tablet);
    QVERIFY(m_tabletPad);
    QVERIFY(tabletPadSpy.wait() || tabletPadSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_pads.count(), 1);
    QVERIFY(m_tabletSeatClient->m_pads[0]);

    QVERIFY(m_tabletPad->ring(0));
    QVERIFY(m_tabletPad->strip(0));

    QCOMPARE(m_surfaces.count(), 3);
    QVERIFY(m_tabletSeatClient->m_pads[0]->buttonStates.isEmpty());
    QSignalSpy buttonSpy(m_tabletSeatClient->m_pads[0], &TabletPad::buttonReceived);
    m_tabletPad->setCurrentSurface(m_surfaces[0], m_tablet);
    m_tabletPad->sendButton(123, 0, QtWayland::zwp_tablet_pad_v2::button_state_pressed);
    QVERIFY(buttonSpy.count() || buttonSpy.wait(100));
    QCOMPARE(m_tabletSeatClient->m_pads[0]->doneCalled, true);
    QCOMPARE(m_tabletSeatClient->m_pads[0]->buttonStates.count(), 1);
    QCOMPARE(m_tabletSeatClient->m_pads[0]->buttonStates[*m_surfacesClient[0]][0], QtWayland::zwp_tablet_pad_v2::button_state_pressed);
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
