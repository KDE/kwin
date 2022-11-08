/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"
// server
#include "wayland/compositor_interface.h"
#include "wayland/datadevicemanager_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"

using namespace KWaylandServer;

class SelectionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void testClearOnEnter();

private:
    KWaylandServer::Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    DataDeviceManagerInterface *m_ddmInterface = nullptr;

    struct Connection
    {
        KWayland::Client::ConnectionThread *connection = nullptr;
        QThread *thread = nullptr;
        KWayland::Client::EventQueue *queue = nullptr;
        KWayland::Client::Compositor *compositor = nullptr;
        KWayland::Client::Seat *seat = nullptr;
        KWayland::Client::DataDeviceManager *ddm = nullptr;
        KWayland::Client::Keyboard *keyboard = nullptr;
        KWayland::Client::DataDevice *dataDevice = nullptr;
    };
    bool setupConnection(Connection *c);
    void cleanupConnection(Connection *c);

    Connection m_client1;
    Connection m_client2;
};

static const QString s_socketName = QStringLiteral("kwayland-test-selection-0");

void SelectionTest::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_seatInterface = new SeatInterface(m_display, m_display);
    m_seatInterface->setHasKeyboard(true);
    m_ddmInterface = new DataDeviceManagerInterface(m_display, m_display);

    // setup connection
    setupConnection(&m_client1);
    setupConnection(&m_client2);
}

bool SelectionTest::setupConnection(Connection *c)
{
    c->connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(c->connection, &KWayland::Client::ConnectionThread::connected);
    if (!connectedSpy.isValid()) {
        return false;
    }
    c->connection->setSocketName(s_socketName);

    c->thread = new QThread(this);
    c->connection->moveToThread(c->thread);
    c->thread->start();

    c->connection->initConnection();
    if (!connectedSpy.wait(500)) {
        return false;
    }

    c->queue = new KWayland::Client::EventQueue(this);
    c->queue->setup(c->connection);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    if (!interfacesAnnouncedSpy.isValid()) {
        return false;
    }
    registry.setEventQueue(c->queue);
    registry.create(c->connection);
    if (!registry.isValid()) {
        return false;
    }
    registry.setup();
    if (!interfacesAnnouncedSpy.wait(500)) {
        return false;
    }

    c->compositor =
        registry.createCompositor(registry.interface(KWayland::Client::Registry::Interface::Compositor).name, registry.interface(KWayland::Client::Registry::Interface::Compositor).version, this);
    if (!c->compositor->isValid()) {
        return false;
    }
    c->ddm = registry.createDataDeviceManager(registry.interface(KWayland::Client::Registry::Interface::DataDeviceManager).name,
                                              registry.interface(KWayland::Client::Registry::Interface::DataDeviceManager).version,
                                              this);
    if (!c->ddm->isValid()) {
        return false;
    }
    c->seat = registry.createSeat(registry.interface(KWayland::Client::Registry::Interface::Seat).name, registry.interface(KWayland::Client::Registry::Interface::Seat).version, this);
    if (!c->seat->isValid()) {
        return false;
    }
    QSignalSpy keyboardSpy(c->seat, &KWayland::Client::Seat::hasKeyboardChanged);
    if (!keyboardSpy.isValid()) {
        return false;
    }
    if (!keyboardSpy.wait(500)) {
        return false;
    }
    if (!c->seat->hasKeyboard()) {
        return false;
    }
    c->keyboard = c->seat->createKeyboard(c->seat);
    if (!c->keyboard->isValid()) {
        return false;
    }
    c->dataDevice = c->ddm->getDataDevice(c->seat, this);
    if (!c->dataDevice->isValid()) {
        return false;
    }

    return true;
}

void SelectionTest::cleanup()
{
    cleanupConnection(&m_client1);
    cleanupConnection(&m_client2);
#define CLEANUP(variable) \
    delete variable;      \
    variable = nullptr;

    CLEANUP(m_display)
#undef CLEANUP
    // these are the children of the display
    m_ddmInterface = nullptr;
    m_seatInterface = nullptr;
    m_compositorInterface = nullptr;
}

void SelectionTest::cleanupConnection(Connection *c)
{
    delete c->dataDevice;
    c->dataDevice = nullptr;
    delete c->keyboard;
    c->keyboard = nullptr;
    delete c->ddm;
    c->ddm = nullptr;
    delete c->seat;
    c->seat = nullptr;
    delete c->compositor;
    c->compositor = nullptr;
    delete c->queue;
    c->queue = nullptr;
    if (c->connection) {
        c->connection->deleteLater();
        c->connection = nullptr;
    }
    if (c->thread) {
        c->thread->quit();
        c->thread->wait();
        delete c->thread;
        c->thread = nullptr;
    }
}

void SelectionTest::testClearOnEnter()
{
    // this test verifies that the selection is cleared prior to keyboard enter if there is no current selection
    QSignalSpy selectionClearedClient1Spy(m_client1.dataDevice, &KWayland::Client::DataDevice::selectionCleared);
    QSignalSpy keyboardEnteredClient1Spy(m_client1.keyboard, &KWayland::Client::Keyboard::entered);

    // now create a Surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> s1(m_client1.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface1 = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface1);

    // pass this surface keyboard focus
    m_seatInterface->setFocusedKeyboardSurface(serverSurface1);
    // should get a clear
    QVERIFY(selectionClearedClient1Spy.wait());

    // let's set a selection
    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_client1.ddm->createDataSource());
    dataSource->offer(QStringLiteral("text/plain"));
    m_client1.dataDevice->setSelection(keyboardEnteredClient1Spy.first().first().value<quint32>(), dataSource.get());

    // now let's bring in client 2
    QSignalSpy selectionOfferedClient2Spy(m_client2.dataDevice, &KWayland::Client::DataDevice::selectionOffered);
    QSignalSpy selectionClearedClient2Spy(m_client2.dataDevice, &KWayland::Client::DataDevice::selectionCleared);
    QSignalSpy keyboardEnteredClient2Spy(m_client2.keyboard, &KWayland::Client::Keyboard::entered);
    std::unique_ptr<KWayland::Client::Surface> s2(m_client2.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface2 = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface2);

    // entering that surface should give a selection offer
    m_seatInterface->setFocusedKeyboardSurface(serverSurface2);
    QVERIFY(selectionOfferedClient2Spy.wait());
    QVERIFY(selectionClearedClient2Spy.isEmpty());

    // set a data source but without offers
    std::unique_ptr<KWayland::Client::DataSource> dataSource2(m_client2.ddm->createDataSource());
    m_client2.dataDevice->setSelection(keyboardEnteredClient2Spy.first().first().value<quint32>(), dataSource2.get());
    QVERIFY(selectionOfferedClient2Spy.wait());
    // and clear
    m_client2.dataDevice->clearSelection(keyboardEnteredClient2Spy.first().first().value<quint32>());
    QVERIFY(selectionClearedClient2Spy.wait());

    // now pass focus to first surface
    m_seatInterface->setFocusedKeyboardSurface(serverSurface1);
    // we should get a clear
    QVERIFY(selectionClearedClient1Spy.wait());
}

QTEST_GUILESS_MAIN(SelectionTest)
#include "test_selection.moc"
