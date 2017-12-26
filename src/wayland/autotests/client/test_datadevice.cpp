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
// KWayland
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/compositor.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/datasource_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/pointer_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client.h>

class TestDataDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testDrag();
    void testDragInternally();
    void testSetSelection();
    void testSendSelectionOnSeat();
    void testReplaceSource();
    void testDestroy();

private:
    KWayland::Server::Display *m_display = nullptr;
    KWayland::Server::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    KWayland::Server::CompositorInterface *m_compositorInterface = nullptr;
    KWayland::Server::SeatInterface *m_seatInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::DataDeviceManager *m_dataDeviceManager = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-datadevice-0");

void TestDataDevice::init()
{
    qRegisterMetaType<KWayland::Server::DataSourceInterface*>();
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    KWayland::Client::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry, SIGNAL(dataDeviceManagerAnnounced(quint32,quint32)));
    QVERIFY(dataDeviceManagerSpy.isValid());
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    QVERIFY(seatSpy.isValid());
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorSpy.isValid());
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_dataDeviceManagerInterface = m_display->createDataDeviceManager(m_display);
    m_dataDeviceManagerInterface->create();
    QVERIFY(m_dataDeviceManagerInterface->isValid());

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager = registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                                           dataDeviceManagerSpy.first().last().value<quint32>(), this);

    m_seatInterface = m_display->createSeat(m_display);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->create();
    QVERIFY(m_seatInterface->isValid());

    QVERIFY(seatSpy.wait());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(),
                                 seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(),
                                             compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
}

void TestDataDevice::cleanup()
{
    if (m_dataDeviceManager) {
        delete m_dataDeviceManager;
        m_dataDeviceManager = nullptr;
    }
    if (m_seat) {
        delete m_seat;
        m_seat = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
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
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestDataDevice::testCreate()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(KWayland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);
    QCOMPARE(deviceInterface->seat(), m_seatInterface);
    QVERIFY(!deviceInterface->dragSource());
    QVERIFY(!deviceInterface->origin());
    QVERIFY(!deviceInterface->icon());
    QVERIFY(!deviceInterface->selection());
    QVERIFY(deviceInterface->parentResource());

    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setSelection(deviceInterface);
    QCOMPARE(m_seatInterface->selection(), deviceInterface);

    // and destroy
    QSignalSpy destroyedSpy(deviceInterface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    dataDevice.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_seatInterface->selection());
}

void TestDataDevice::testDrag()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    QScopedPointer<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(KWayland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataSourceCreated(KWayland::Server::DataSourceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface = dataSourceCreatedSpy.first().first().value<DataSourceInterface*>();
    QVERIFY(sourceInterface);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(deviceInterface, SIGNAL(dragStarted()));
    QVERIFY(dragStartedSpy.isValid());

    // first we need to fake the pointer enter
    m_seatInterface->setFocusedPointerSurface(surfaceInterface);
    m_seatInterface->pointerButtonPressed(1);

    QCoreApplication::processEvents();

    dataDevice->startDrag(1, dataSource.data(), surface.data());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(dragStartedSpy.count(), 1);
    QCOMPARE(deviceInterface->dragSource(), sourceInterface);
    QCOMPARE(deviceInterface->origin(), surfaceInterface);
    QVERIFY(!deviceInterface->icon());
}

void TestDataDevice::testDragInternally()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    QScopedPointer<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(KWayland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 1);
    auto surfaceInterface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();

    QScopedPointer<Surface> iconSurface(m_compositor->createSurface());
    QVERIFY(iconSurface->isValid());

    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    auto iconSurfaceInterface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();

    // now we have all we need to start a drag operation
    QSignalSpy dragStartedSpy(deviceInterface, SIGNAL(dragStarted()));
    QVERIFY(dragStartedSpy.isValid());

    // first we need to fake the pointer enter
    m_seatInterface->setFocusedPointerSurface(surfaceInterface);
    m_seatInterface->pointerButtonPressed(1);

    QCoreApplication::processEvents();

    dataDevice->startDragInternally(1, surface.data(), iconSurface.data());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(dragStartedSpy.count(), 1);
    QVERIFY(!deviceInterface->dragSource());
    QCOMPARE(deviceInterface->origin(), surfaceInterface);
    QCOMPARE(deviceInterface->icon(), iconSurfaceInterface);
}

void TestDataDevice::testSetSelection()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    QScopedPointer<Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataDeviceCreated(KWayland::Server::DataDeviceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(deviceInterface);

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, SIGNAL(dataSourceCreated(KWayland::Server::DataSourceInterface*)));
    QVERIFY(dataDeviceCreatedSpy.isValid());

    QScopedPointer<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface = dataSourceCreatedSpy.first().first().value<DataSourceInterface*>();
    QVERIFY(sourceInterface);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(deviceInterface, SIGNAL(selectionChanged(KWayland::Server::DataSourceInterface*)));
    QVERIFY(selectionChangedSpy.isValid());
    QSignalSpy selectionClearedSpy(deviceInterface, SIGNAL(selectionCleared()));
    QVERIFY(selectionClearedSpy.isValid());

    QVERIFY(!deviceInterface->selection());
    dataDevice->setSelection(1, dataSource.data());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 0);
    QCOMPARE(selectionChangedSpy.first().first().value<DataSourceInterface*>(), sourceInterface);
    QCOMPARE(deviceInterface->selection(), sourceInterface);

    // send selection to datadevice
    QSignalSpy selectionOfferedSpy(dataDevice.data(), SIGNAL(selectionOffered(KWayland::Client::DataOffer*)));
    QVERIFY(selectionOfferedSpy.isValid());
    deviceInterface->sendSelection(deviceInterface);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);
    auto dataOffer = selectionOfferedSpy.first().first().value<DataOffer*>();
    QVERIFY(dataOffer);
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 1);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(dataOffer, SIGNAL(mimeTypeOffered(QString)));
    QVERIFY(mimeTypeAddedSpy.isValid());
    dataSource->offer(QStringLiteral("text/html"));
    QVERIFY(mimeTypeAddedSpy.wait());
    QCOMPARE(mimeTypeAddedSpy.count(), 1);
    QCOMPARE(mimeTypeAddedSpy.first().first().toString(), QStringLiteral("text/html"));
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 2);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QCOMPARE(dataOffer->offeredMimeTypes().last().name(), QStringLiteral("text/html"));

    // now clear the selection
    dataDevice->clearSelection(1);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionClearedSpy.count(), 1);
    QVERIFY(!deviceInterface->selection());

    // set another selection
    dataDevice->setSelection(2, dataSource.data());
    QVERIFY(selectionChangedSpy.wait());
    // now unbind the dataDevice
    QSignalSpy unboundSpy(deviceInterface, &DataDeviceInterface::unbound);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    // send a selection to the unbound data device
    deviceInterface->sendSelection(deviceInterface);
}

void TestDataDevice::testSendSelectionOnSeat()
{
    // this test verifies that the selection is sent when setting a focused keyboard
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(serverDataDevice);
    QScopedPointer<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    // now set the selection
    QScopedPointer<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.data());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.data(), &DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // now unfocus the keyboard
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    // if setting the same surface again, we should get another offer
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);

    // now let's try to destroy the data device and set a focused keyboard just while the data device is being destroyedd
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QSignalSpy unboundSpy(serverDataDevice, &Resource::unbound);
    QVERIFY(unboundSpy.isValid());
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
}

void TestDataDevice::testReplaceSource()
{
    // this test verifies that replacing a data source cancels the previous source
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    QVERIFY(dataDeviceCreatedSpy.isValid());
    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface*>();
    QVERIFY(serverDataDevice);
    QScopedPointer<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    // now set the selection
    QScopedPointer<DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.data());
    QSignalSpy sourceCancelledSpy(dataSource.data(), &DataSource::cancelled);
    QVERIFY(sourceCancelledSpy.isValid());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.data(), &DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.isValid());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second data source and replace previous one
    QScopedPointer<DataSource> dataSource2(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource2->isValid());
    dataSource2->offer(QStringLiteral("text/plain"));
    QSignalSpy sourceCancelled2Spy(dataSource2.data(), &DataSource::cancelled);
    QVERIFY(sourceCancelled2Spy.isValid());
    dataDevice->setSelection(1, dataSource2.data());
    QCOMPARE(selectionOfferedSpy.count(), 1);
    QVERIFY(sourceCancelledSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // create a new DataDevice and replace previous one
    QScopedPointer<DataDevice> dataDevice2(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice2->isValid());
    QScopedPointer<DataSource> dataSource3(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource3->isValid());
    dataSource3->offer(QStringLiteral("text/plain"));
    dataDevice2->setSelection(1, dataSource3.data());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying dataSource3 and setting a new DataSource
    QScopedPointer<DataSource> dataSource4(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource4->isValid());
    dataSource4->offer(QStringLiteral("text/plain"));
    dataSource3.reset();
    dataDevice2->setSelection(1, dataSource4.data());
    QVERIFY(selectionOfferedSpy.wait());
}

void TestDataDevice::testDestroy()
{
    using namespace KWayland::Client;

    QScopedPointer<DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    connect(m_connection, &ConnectionThread::connectionDied, m_dataDeviceManager, &DataDeviceManager::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_seat, &Seat::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_compositor, &Compositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, dataDevice.data(), &DataDevice::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the data device should be destroyed;
    QVERIFY(!dataDevice->isValid());

    // calling destroy again should not fail
    dataDevice->destroy();
}

QTEST_GUILESS_MAIN(TestDataDevice)
#include "test_datadevice.moc"
