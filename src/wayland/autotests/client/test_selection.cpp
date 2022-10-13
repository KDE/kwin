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

using namespace KWayland::Client;
using namespace KWaylandServer;

class SelectionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void testClearOnEnter();

private:
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<CompositorInterface> m_compositorInterface;
    std::unique_ptr<SeatInterface> m_seatInterface;
    std::unique_ptr<DataDeviceManagerInterface> m_ddmInterface;

    struct Connection
    {
        std::unique_ptr<ConnectionThread> connection;
        std::unique_ptr<QThread> thread;
        std::unique_ptr<EventQueue> queue;
        std::unique_ptr<Compositor> compositor;
        std::unique_ptr<Seat> seat;
        std::unique_ptr<DataDeviceManager> ddm;
        std::unique_ptr<Keyboard> keyboard;
        std::unique_ptr<DataDevice> dataDevice;
    };
    bool setupConnection(Connection &c);
    void cleanupConnection(Connection &c);

    Connection m_client1;
    Connection m_client2;
};

static const QString s_socketName = QStringLiteral("kwayland-test-selection-0");

void SelectionTest::init()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    m_seatInterface = std::make_unique<SeatInterface>(m_display.get());
    m_seatInterface->setHasKeyboard(true);
    m_ddmInterface = std::make_unique<DataDeviceManagerInterface>(m_display.get());

    // setup connection
    setupConnection(m_client1);
    setupConnection(m_client2);
}

bool SelectionTest::setupConnection(Connection &c)
{
    c.connection = std::make_unique<ConnectionThread>();
    QSignalSpy connectedSpy(c.connection.get(), &ConnectionThread::connected);
    if (!connectedSpy.isValid()) {
        return false;
    }
    c.connection->setSocketName(s_socketName);

    c.thread = std::make_unique<QThread>();
    c.connection->moveToThread(c.thread.get());
    c.thread->start();

    c.connection->initConnection();
    if (!connectedSpy.wait(500)) {
        return false;
    }

    c.queue = std::make_unique<EventQueue>();
    c.queue->setup(c.connection.get());

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    if (!interfacesAnnouncedSpy.isValid()) {
        return false;
    }
    registry.setEventQueue(c.queue.get());
    registry.create(c.connection.get());
    if (!registry.isValid()) {
        return false;
    }
    registry.setup();
    if (!interfacesAnnouncedSpy.wait(500)) {
        return false;
    }

    c.compositor.reset(registry.createCompositor(registry.interface(Registry::Interface::Compositor).name, registry.interface(Registry::Interface::Compositor).version));
    if (!c.compositor->isValid()) {
        return false;
    }
    c.ddm.reset(registry.createDataDeviceManager(registry.interface(Registry::Interface::DataDeviceManager).name,
                                                 registry.interface(Registry::Interface::DataDeviceManager).version));
    if (!c.ddm->isValid()) {
        return false;
    }
    c.seat.reset(registry.createSeat(registry.interface(Registry::Interface::Seat).name, registry.interface(Registry::Interface::Seat).version));
    if (!c.seat->isValid()) {
        return false;
    }
    QSignalSpy keyboardSpy(c.seat.get(), &Seat::hasKeyboardChanged);
    if (!keyboardSpy.isValid()) {
        return false;
    }
    if (!keyboardSpy.wait(500)) {
        return false;
    }
    if (!c.seat->hasKeyboard()) {
        return false;
    }
    c.keyboard.reset(c.seat->createKeyboard());
    if (!c.keyboard->isValid()) {
        return false;
    }
    c.dataDevice.reset(c.ddm->getDataDevice(c.seat.get()));
    if (!c.dataDevice->isValid()) {
        return false;
    }

    return true;
}

void SelectionTest::cleanup()
{
    cleanupConnection(m_client1);
    cleanupConnection(m_client2);
    m_display.reset();
}

void SelectionTest::cleanupConnection(Connection &c)
{
    c.dataDevice.reset();
    c.keyboard.reset();
    c.ddm.reset();
    c.seat.reset();
    c.compositor.reset();
    c.queue.reset();
    if (c.thread) {
        c.thread->quit();
        c.thread->wait();
        c.thread.reset();
    }
    c.connection.reset();
}

void SelectionTest::testClearOnEnter()
{
    // this test verifies that the selection is cleared prior to keyboard enter if there is no current selection
    QSignalSpy selectionClearedClient1Spy(m_client1.dataDevice.get(), &DataDevice::selectionCleared);
    QSignalSpy keyboardEnteredClient1Spy(m_client1.keyboard.get(), &Keyboard::entered);

    // now create a Surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface.get(), &CompositorInterface::surfaceCreated);
    std::unique_ptr<Surface> s1(m_client1.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface1 = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface1);

    // pass this surface keyboard focus
    m_seatInterface->setFocusedKeyboardSurface(serverSurface1);
    // should get a clear
    QVERIFY(selectionClearedClient1Spy.wait());

    // let's set a selection
    std::unique_ptr<DataSource> dataSource(m_client1.ddm->createDataSource());
    dataSource->offer(QStringLiteral("text/plain"));
    m_client1.dataDevice->setSelection(keyboardEnteredClient1Spy.first().first().value<quint32>(), dataSource.get());

    // now let's bring in client 2
    QSignalSpy selectionOfferedClient2Spy(m_client2.dataDevice.get(), &DataDevice::selectionOffered);
    QSignalSpy selectionClearedClient2Spy(m_client2.dataDevice.get(), &DataDevice::selectionCleared);
    QSignalSpy keyboardEnteredClient2Spy(m_client2.keyboard.get(), &Keyboard::entered);
    std::unique_ptr<Surface> s2(m_client2.compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface2 = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface2);

    // entering that surface should give a selection offer
    m_seatInterface->setFocusedKeyboardSurface(serverSurface2);
    QVERIFY(selectionOfferedClient2Spy.wait());
    QVERIFY(selectionClearedClient2Spy.isEmpty());

    // set a data source but without offers
    std::unique_ptr<DataSource> dataSource2(m_client2.ddm->createDataSource());
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
