/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QHash>
#include <QThread>
#include <QtTest>
// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/inputmethod_v1_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/output.h"

#include "qwayland-input-method-unstable-v1.h"

using namespace KWaylandServer;


class InputPanelSurface : public QObject, public QtWayland::zwp_input_panel_surface_v1
{
    Q_OBJECT
public:
    InputPanelSurface(::zwp_input_panel_surface_v1 *t)
        : QtWayland::zwp_input_panel_surface_v1(t)
    {
    }
};

class InputPanel : public QtWayland::zwp_input_panel_v1
{
public:
    InputPanel(struct ::wl_registry *registry, int id, int version)
        : QtWayland::zwp_input_panel_v1(registry, id, version)
    {
    }

    InputPanelSurface* panelForSurface(KWayland::Client::Surface* surface) {
        auto panelSurface = new InputPanelSurface(get_input_panel_surface(*surface));
        QObject::connect(surface, &QObject::destroyed, panelSurface, &QObject::deleteLater);
        return panelSurface;
    }
};

class TestInputMethodInterface : public QObject
{
    Q_OBJECT
public:
    TestInputMethodInterface()
    {
    }
    ~TestInputMethodInterface() override;

private Q_SLOTS:
    void initTestCase();
    void testAdd();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;
    KWayland::Client::Output *m_output= nullptr;

    InputPanel* m_inputPanel;
    QThread *m_thread;
    Display m_display;
    SeatInterface *m_seat;
    CompositorInterface *m_serverCompositor;

    KWaylandServer::InputMethodV1Interface* m_inputMethodIface;
    KWaylandServer::InputPanelV1Interface* m_inputPanelIface;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-inputmethod-test-0");

void TestInputMethodInterface::initTestCase()
{
    m_display.setSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = m_display.createSeat(this);
    m_seat->create();
    m_serverCompositor = m_display.createCompositor(this);
    m_inputMethodIface = m_display.createInputMethodInterface(this);
    m_inputPanelIface = m_display.createInputPanelInterface(this);
    auto outputIface = m_display.createOutput(this);
    outputIface->create();

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
    QSignalSpy interfacesSpy(registry, &KWayland::Client::Registry::interfacesAnnounced);
    connect(registry, &KWayland::Client::Registry::outputAnnounced, this, [this, registry] (quint32 name, quint32 version) {
        m_output = new KWayland::Client::Output(this);
        m_output->setup(registry->bindOutput(name, version));
    });
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwp_input_panel_v1") {
            m_inputPanel = new InputPanel(registry->registry(), name, version);
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

    QVERIFY(interfacesSpy.count() || interfacesSpy.wait());

    QSignalSpy surfaceSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    for (int i = 0; i < 3; ++i) {
        m_clientCompositor->createSurface(this);
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_surfaces.count() == 3);
    QVERIFY(m_inputPanel);
    QVERIFY(m_output);
}

TestInputMethodInterface::~TestInputMethodInterface()
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
    delete m_inputPanel;
    delete m_inputMethodIface;
    delete m_inputPanelIface;
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestInputMethodInterface::testAdd()
{
    QSignalSpy panelSpy(m_inputPanelIface, &InputPanelV1Interface::inputPanelSurfaceAdded);
    QPointer<InputPanelSurfaceV1Interface> panelSurfaceIface;
    connect(m_inputPanelIface, &InputPanelV1Interface::inputPanelSurfaceAdded, this, [&panelSurfaceIface] (InputPanelSurfaceV1Interface *surface) {
        panelSurfaceIface = surface;
    });

    auto surface = m_clientCompositor->createSurface(this);
    auto panelSurface = m_inputPanel->panelForSurface(surface);

    QVERIFY(panelSpy.wait() || panelSurfaceIface);
    Q_ASSERT(panelSurfaceIface);
    Q_ASSERT(panelSurfaceIface->surface() == m_surfaces.constLast());

    QSignalSpy panelTopLevelSpy(panelSurfaceIface, &InputPanelSurfaceV1Interface::topLevel);
    panelSurface->set_toplevel(*m_output, InputPanelSurface::position_center_bottom);
    QVERIFY(panelTopLevelSpy.wait());
}


QTEST_GUILESS_MAIN(TestInputMethodInterface)
#include "test_inputmethod_interface.moc"
