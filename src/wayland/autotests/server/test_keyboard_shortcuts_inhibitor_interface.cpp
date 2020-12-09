/*
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// Qt
#include <QHash>
#include <QThread>
#include <QtTest>
// WaylandServer
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/keyboard_shortcuts_inhibit_v1_interface.h"
#include "../../src/server/seat_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-keyboard-shortcuts-inhibit-unstable-v1.h"

using namespace KWaylandServer;

class KeyboardShortcutsInhibitManager : public QObject, public QtWayland::zwp_keyboard_shortcuts_inhibit_manager_v1
{
    Q_OBJECT
public:
    KeyboardShortcutsInhibitManager(wl_registry *registry, quint32 id, quint32 version)
        : QtWayland::zwp_keyboard_shortcuts_inhibit_manager_v1(registry, id, version)
    {
    }

};

class KeyboardShortcutsInhibitor : public QObject, public QtWayland::zwp_keyboard_shortcuts_inhibitor_v1
{
    Q_OBJECT
public:
    KeyboardShortcutsInhibitor(::zwp_keyboard_shortcuts_inhibitor_v1 *inhibitorV1)
        : QtWayland::zwp_keyboard_shortcuts_inhibitor_v1(inhibitorV1)
    {
    }

    void zwp_keyboard_shortcuts_inhibitor_v1_active() override
    {
        emit inhibitorActive();
    }

    void zwp_keyboard_shortcuts_inhibitor_v1_inactive() override
    {
        emit inhibitorInactive();
    }

Q_SIGNALS:
    void inhibitorActive();
    void inhibitorInactive();
};


class TestKeyboardShortcutsInhibitorInterface : public QObject
{
    Q_OBJECT
public:
    TestKeyboardShortcutsInhibitorInterface()
    {
    }
    ~TestKeyboardShortcutsInhibitorInterface() override;

private Q_SLOTS:
    void initTestCase();
    void testKeyboardShortcuts();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    QThread *m_thread;
    Display m_display;
    SeatInterface *m_seat;
    CompositorInterface *m_serverCompositor;

    KeyboardShortcutsInhibitManagerV1Interface *m_manager;
    QVector<SurfaceInterface *> m_surfaces;
    QVector<wl_surface *> m_clientSurfaces;
    KeyboardShortcutsInhibitManager *m_inhibitManagerClient = nullptr;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-keyboard-shortcuts-inhibitor-test-0");

void TestKeyboardShortcutsInhibitorInterface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = new SeatInterface(&m_display, this);
    m_seat->create();
    m_serverCompositor = new CompositorInterface(&m_display, this);
    m_manager = new KeyboardShortcutsInhibitManagerV1Interface(&m_display, this);

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
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == "zwp_keyboard_shortcuts_inhibit_manager_v1") {
            m_inhibitManagerClient = new KeyboardShortcutsInhibitManager(registry->registry(), id, version);
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
        KWayland::Client::Surface *s = m_clientCompositor->createSurface(this);
        m_clientSurfaces += s->operator wl_surface *();
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_surfaces.count() == 3);
    QVERIFY(m_inhibitManagerClient);
}

TestKeyboardShortcutsInhibitorInterface::~TestKeyboardShortcutsInhibitorInterface()
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
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestKeyboardShortcutsInhibitorInterface::testKeyboardShortcuts()
{
    auto clientSurface = m_clientSurfaces[0];
    auto surface = m_surfaces[0];

    // Test creation
    auto inhibitorClientV1 = m_inhibitManagerClient->inhibit_shortcuts(clientSurface, m_clientSeat->operator wl_seat *());
    auto inhibitorClient = new KeyboardShortcutsInhibitor(inhibitorClientV1);
    QSignalSpy inhibitorActiveSpy(inhibitorClient, &KeyboardShortcutsInhibitor::inhibitorActive);
    QSignalSpy inhibitorInactiveSpy(inhibitorClient, &KeyboardShortcutsInhibitor::inhibitorInactive);
    QSignalSpy inhibitorCreatedSpy(m_manager, &KeyboardShortcutsInhibitManagerV1Interface::inhibitorCreated);
    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 1);
    auto inhibitorServer = m_manager->findInhibitor(surface, m_seat);

    // Test deactivate
    inhibitorServer->setActive(false);
    QVERIFY(inhibitorInactiveSpy.wait() || inhibitorInactiveSpy.count() == 1);

    // Test activate
    inhibitorServer->setActive(true);
    QVERIFY(inhibitorInactiveSpy.wait() || inhibitorInactiveSpy.count() == 1);

    // Test creating for another surface
    m_inhibitManagerClient->inhibit_shortcuts(m_clientSurfaces[1], m_clientSeat->operator wl_seat *());
    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 2);

    // Test destroy is working
    inhibitorClient->destroy();
    m_inhibitManagerClient->inhibit_shortcuts(clientSurface, m_clientSeat->operator wl_seat *());
    QVERIFY(inhibitorCreatedSpy.wait() || inhibitorCreatedSpy.count() == 3);

    // Test creating with same surface / seat (expect error)
    QSignalSpy errorOccured(m_connection, &KWayland::Client::ConnectionThread::errorOccurred);
    m_inhibitManagerClient->inhibit_shortcuts(m_clientSurfaces[0], m_clientSeat->operator wl_seat *());
    QVERIFY(errorOccured.wait() || errorOccured.count() == 1);
}

QTEST_GUILESS_MAIN(TestKeyboardShortcutsInhibitorInterface)

#include "test_keyboard_shortcuts_inhibitor_interface.moc"
