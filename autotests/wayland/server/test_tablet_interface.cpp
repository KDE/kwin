/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QHash>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
// WaylandServer
#include "core/inputdevice.h"
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/tablet_v2.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-tablet-v2.h"

using namespace KWin;
using namespace std::literals;

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

    QList<Tablet *> m_tablets;
    QList<TabletPad *> m_pads;
    QList<Tool *> m_tools;

Q_SIGNALS:
    void padAdded();
    void toolAdded();
    void tabletAdded();
};

class DummyInputDevice : public InputDevice
{
    Q_OBJECT

public:
    explicit DummyInputDevice(QObject *parent = nullptr)
        : InputDevice(parent)
    {
    }

    QString name() const override
    {
        return m_name;
    }

    QString sysPath() const override
    {
        return m_sysPath;
    }

    quint32 vendor() const override
    {
        return m_vendorId;
    }

    quint32 product() const override
    {
        return m_productId;
    }

    bool isEnabled() const override
    {
        return true;
    }

    void setEnabled(bool enabled) override
    {
    }

    bool isKeyboard() const override
    {
        return false;
    }

    bool isPointer() const override
    {
        return false;
    }

    bool isTouchpad() const override
    {
        return false;
    }

    bool isTouch() const override
    {
        return false;
    }

    bool isTabletTool() const override
    {
        return m_tabletTool;
    }

    bool isTabletPad() const override
    {
        return m_tabletPad;
    }

    bool isTabletModeSwitch() const override
    {
        return false;
    }

    bool isLidSwitch() const override
    {
        return false;
    }

    int tabletPadButtonCount() const override
    {
        return 1;
    }

    void setName(const QString &name)
    {
        m_name = name;
    }

    void setSysPath(const QString &path)
    {
        m_sysPath = path;
    }

    void setVendor(quint32 vendor)
    {
        m_vendorId = vendor;
    }

    void setProduct(quint32 product)
    {
        m_productId = product;
    }

    void setTabletTool(bool tool)
    {
        m_tabletTool = tool;
    }

    void setTabletPad(bool pad)
    {
        m_tabletPad = pad;
    }

    QList<InputDeviceTabletPadModeGroup> modeGroups() const override
    {
        return {
            InputDeviceTabletPadModeGroup{
                .modeCount = 1,
                .buttons = {0},
                .rings = {0},
                .strips = {0},
            }};
    }

private:
    QString m_name;
    QString m_sysPath;
    quint32 m_vendorId = 0;
    quint32 m_productId = 0;
    bool m_tabletTool = false;
    bool m_tabletPad = false;
};

class DummyInputDeviceTabletTool : public InputDeviceTabletTool
{
    Q_OBJECT

public:
    explicit DummyInputDeviceTabletTool(QObject *parent = nullptr)
        : InputDeviceTabletTool(parent)
    {
    }

    void setSerialId(quint64 serialId)
    {
        m_serialId = serialId;
    }

    void setUniqueId(quint64 uniqueId)
    {
        m_uniqueId = uniqueId;
    }

    void setType(Type type)
    {
        m_type = type;
    }

    void setCapabilities(const QList<Capability> &capabilities)
    {
        m_capabilities = capabilities;
    }

    quint64 serialId() const override
    {
        return m_serialId;
    }

    quint64 uniqueId() const override
    {
        return m_uniqueId;
    }

    Type type() const override
    {
        return m_type;
    }

    QList<Capability> capabilities() const override
    {
        return m_capabilities;
    }

private:
    quint64 m_serialId = 0;
    quint64 m_uniqueId = 0;
    Type m_type = Type::Pen;
    QList<Capability> m_capabilities;
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
    void testInteractSimple_data();
    void testInteractSimple();
    void testInteractSurfaceChange_data();
    void testInteractSurfaceChange();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    QThread *m_thread;
    KWin::Display m_display;
    SeatInterface *m_seat;
    CompositorInterface *m_serverCompositor;

    TabletSeat *m_tabletSeatClient = nullptr;
    TabletSeat *m_tabletSeatClient2 = nullptr;

    TabletManagerV2Interface *m_tabletManager;
    QList<KWayland::Client::Surface *> m_surfacesClient;

    DummyInputDevice *m_tabletDevice = nullptr;
    TabletV2Interface *m_tablet;
    DummyInputDevice *m_tabletPadDevice = nullptr;
    TabletPadV2Interface *m_tabletPad = nullptr;
    DummyInputDeviceTabletTool *m_toolDevice = nullptr;
    TabletToolV2Interface *m_tool = nullptr;

    QList<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-tablet-test-0");

void TestTabletInterface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = new SeatInterface(&m_display, QStringLiteral("seat0"), this);
    m_serverCompositor = new CompositorInterface(&m_display, this);
    m_tabletManager = new TabletManagerV2Interface(&m_display, this);

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
            auto _seat2 = tabletClient->get_tablet_seat(*m_clientSeat);
            m_tabletSeatClient2 = new TabletSeat(_seat2);
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
        m_surfacesClient += m_clientCompositor->createSurface(this);
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
    delete m_tabletSeatClient2;
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestTabletInterface::testAdd()
{
    TabletSeatV2Interface *seatInterface = m_tabletManager->seat(m_seat);
    QVERIFY(seatInterface);

    QSignalSpy tabletSpy(m_tabletSeatClient, &TabletSeat::tabletAdded);
    m_tabletDevice = new DummyInputDevice();
    m_tabletDevice->setTabletTool(true);
    m_tabletDevice->setVendor(1);
    m_tabletDevice->setProduct(2);
    m_tabletDevice->setName(QStringLiteral("my tablet"));
    m_tabletDevice->setSysPath(QStringLiteral("/test/event33"));
    m_tablet = seatInterface->addTablet(m_tabletDevice);
    QVERIFY(m_tablet);
    QVERIFY(tabletSpy.wait() || tabletSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tablets.count(), 1);

    QSignalSpy toolSpy(m_tabletSeatClient, &TabletSeat::toolAdded);
    m_toolDevice = new DummyInputDeviceTabletTool();
    m_toolDevice->setSerialId(0);
    m_toolDevice->setUniqueId(0);
    m_toolDevice->setType(InputDeviceTabletTool::Pen);
    m_toolDevice->setCapabilities({InputDeviceTabletTool::Tilt, InputDeviceTabletTool::Pressure});
    m_tool = seatInterface->addTool(m_toolDevice);
    QVERIFY(m_tool);
    QVERIFY(toolSpy.wait() || toolSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_tools.count(), 1);

    QVERIFY(!m_tool->isClientSupported()); // There's no surface in it yet
    m_tool->setCurrentSurface(nullptr, m_seat->nextSerial());
    QVERIFY(!m_tool->isClientSupported()); // There's no surface in it

    QCOMPARE(m_surfaces.count(), 3);
    for (SurfaceInterface *surface : m_surfaces) {
        m_tool->setCurrentSurface(surface, m_seat->nextSerial());
    }
    m_tool->setCurrentSurface(nullptr, m_seat->nextSerial());
}

void TestTabletInterface::testAddPad()
{
    TabletSeatV2Interface *seatInterface = m_tabletManager->seat(m_seat);
    QVERIFY(seatInterface);

    QSignalSpy tabletPadSpy(m_tabletSeatClient, &TabletSeat::padAdded);
    m_tabletPadDevice = new DummyInputDevice();
    m_tabletPadDevice->setTabletPad(true);
    m_tabletPadDevice->setName(QStringLiteral("tabletpad"));
    m_tabletPadDevice->setSysPath(QStringLiteral("/test/event33"));
    m_tabletPad = seatInterface->addPad(m_tabletPadDevice);
    QVERIFY(m_tabletPad);
    QVERIFY(tabletPadSpy.wait() || tabletPadSpy.count() == 1);
    QCOMPARE(m_tabletSeatClient->m_pads.count(), 1);
    QVERIFY(m_tabletSeatClient->m_pads[0]);

    QVERIFY(m_tabletPad->group(0)->ring(0));
    QVERIFY(m_tabletPad->group(0)->strip(0));

    QCOMPARE(m_surfaces.count(), 3);
    QVERIFY(m_tabletSeatClient->m_pads[0]->buttonStates.isEmpty());
    QSignalSpy buttonSpy(m_tabletSeatClient->m_pads[0], &TabletPad::buttonReceived);
    m_tabletPad->setCurrentSurface(m_surfaces[0], m_tablet, m_seat->nextSerial());
    m_tabletPad->sendButton(123ms, 0, QtWayland::zwp_tablet_pad_v2::button_state_pressed);
    QVERIFY(buttonSpy.count() || buttonSpy.wait(100));
    QCOMPARE(m_tabletSeatClient->m_pads[0]->doneCalled, true);
    QCOMPARE(m_tabletSeatClient->m_pads[0]->buttonStates.count(), 1);
    QCOMPARE(m_tabletSeatClient->m_pads[0]->buttonStates[*m_surfacesClient[0]][0], QtWayland::zwp_tablet_pad_v2::button_state_pressed);
}

static uint s_serial = 0;

void TestTabletInterface::testInteractSimple_data()
{
    QTest::addColumn<TabletSeat *>("tabletSeatClient");
    QTest::newRow("first client") << m_tabletSeatClient;
    QTest::newRow("second client") << m_tabletSeatClient2;
}

void TestTabletInterface::testInteractSimple()
{
    QFETCH(TabletSeat *, tabletSeatClient);
    tabletSeatClient->m_tools[0]->surfaceApproximated.clear();
    QSignalSpy frameSpy(tabletSeatClient->m_tools[0], &Tool::frame);

    QVERIFY(!m_tool->isClientSupported());
    m_tool->setCurrentSurface(m_surfaces[0], m_seat->nextSerial());
    QVERIFY(m_tool->isClientSupported() && m_tablet->isSurfaceSupported(m_surfaces[0]));
    m_tool->sendProximityIn(m_tablet, m_seat->nextSerial());
    m_tool->sendPressure(0);
    m_tool->sendFrame(s_serial++);
    m_tool->sendMotion({3, 3});
    m_tool->sendFrame(s_serial++);
    m_tool->sendProximityOut();
    QVERIFY(m_tool->isClientSupported());
    m_tool->sendFrame(s_serial++);
    QVERIFY(!m_tool->isClientSupported());

    QVERIFY(frameSpy.wait(500));
    QCOMPARE(tabletSeatClient->m_tools[0]->surfaceApproximated.count(), 1);
}

void TestTabletInterface::testInteractSurfaceChange_data()
{
    QTest::addColumn<TabletSeat *>("tabletSeatClient");
    QTest::newRow("first client") << m_tabletSeatClient;
    QTest::newRow("second client") << m_tabletSeatClient2;
}

void TestTabletInterface::testInteractSurfaceChange()
{
    QFETCH(TabletSeat *, tabletSeatClient);
    tabletSeatClient->m_tools[0]->surfaceApproximated.clear();
    QSignalSpy frameSpy(tabletSeatClient->m_tools[0], &Tool::frame);

    QVERIFY(!m_tool->isClientSupported());
    m_tool->setCurrentSurface(m_surfaces[0], m_seat->nextSerial());
    QVERIFY(m_tool->isClientSupported() && m_tablet->isSurfaceSupported(m_surfaces[0]));
    m_tool->sendProximityIn(m_tablet, m_seat->nextSerial());
    m_tool->sendPressure(0);
    m_tool->sendFrame(s_serial++);

    m_tool->setCurrentSurface(m_surfaces[1], m_seat->nextSerial());
    QVERIFY(m_tool->isClientSupported());

    m_tool->sendMotion({3, 3});
    m_tool->sendFrame(s_serial++);
    m_tool->sendProximityOut();
    QVERIFY(m_tool->isClientSupported());
    m_tool->sendFrame(s_serial++);
    QVERIFY(!m_tool->isClientSupported());

    QVERIFY(frameSpy.wait(500));
    QCOMPARE(tabletSeatClient->m_tools[0]->surfaceApproximated.count(), 2);
}

QTEST_GUILESS_MAIN(TestTabletInterface)
#include "test_tablet_interface.moc"
