/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QMimeType>
#include <QSignalSpy>
#include <QTest>

#include "wayland/compositor.h"
#include "wayland/datadevicemanager.h"
#include "wayland/datasource.h"
#include "wayland/display.h"
#include "wayland/pointer.h"
#include "wayland/seat.h"
#include "wayland/surface.h"

// KWayland
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

// Wayland
#include <wayland-client.h>

#include <unistd.h>

class TestDataDevice : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSetSelection();
    void testSendSelectionOnSeat();
    void testReplaceSource();

private:
    KWin::Display *m_display = nullptr;
    KWin::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    KWin::CompositorInterface *m_compositorInterface = nullptr;
    KWin::SeatInterface *m_seatInterface = nullptr;
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
    qRegisterMetaType<KWin::DataSourceInterface *>();
    using namespace KWin;
    delete m_display;
    m_display = new KWin::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
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
    QSignalSpy dataDeviceManagerSpy(&registry, &KWayland::Client::Registry::dataDeviceManagerAnnounced);
    QSignalSpy seatSpy(&registry, &KWayland::Client::Registry::seatAnnounced);
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_dataDeviceManagerInterface = new DataDeviceManagerInterface(m_display, m_display);

    QVERIFY(dataDeviceManagerSpy.wait());
    m_dataDeviceManager =
        registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(), dataDeviceManagerSpy.first().last().value<quint32>(), this);

    m_seatInterface = new SeatInterface(m_display, QStringLiteral("seat0"), m_display);
    m_seatInterface->setHasPointer(true);

    QVERIFY(seatSpy.wait());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy pointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.wait());

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
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
    using namespace KWin;

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &KWin::DataDeviceManagerInterface::dataDeviceCreated);

    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface *>();
    QVERIFY(deviceInterface);
    QCOMPARE(deviceInterface->seat(), m_seatInterface);
    QVERIFY(!deviceInterface->selection());

    // this will probably fail, we need to make a selection client side
    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setSelection(deviceInterface->selection(), m_seatInterface->nextSerial());
    QCOMPARE(m_seatInterface->selection(), deviceInterface->selection());

    // and destroy
    QSignalSpy destroyedSpy(deviceInterface, &QObject::destroyed);
    dataDevice.reset();
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_seatInterface->selection());
}

void TestDataDevice::testSetSelection()
{
    using namespace KWin;
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());

    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &KWin::DataDeviceManagerInterface::dataDeviceCreated);

    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());

    QVERIFY(dataDeviceCreatedSpy.wait());
    QCOMPARE(dataDeviceCreatedSpy.count(), 1);
    auto deviceInterface = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface *>();
    QVERIFY(deviceInterface);

    QSignalSpy dataSourceCreatedSpy(m_dataDeviceManagerInterface, &KWin::DataDeviceManagerInterface::dataSourceCreated);

    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));

    QVERIFY(dataSourceCreatedSpy.wait());
    QCOMPARE(dataSourceCreatedSpy.count(), 1);
    auto sourceInterface = dataSourceCreatedSpy.first().first().value<DataSourceInterface *>();
    QVERIFY(sourceInterface);

    // everything setup, now we can test setting the selection
    QSignalSpy selectionChangedSpy(deviceInterface, &KWin::DataDeviceInterface::selectionChanged);

    QVERIFY(!deviceInterface->selection());
    dataDevice->setSelection(1, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 1);
    QCOMPARE(selectionChangedSpy.first().first().value<DataSourceInterface *>(), sourceInterface);
    QCOMPARE(deviceInterface->selection(), sourceInterface);

    // send selection to datadevice
    QSignalSpy selectionOfferedSpy(dataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    deviceInterface->sendSelection(deviceInterface->selection());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);
    auto dataOffer = selectionOfferedSpy.first().first().value<KWayland::Client::DataOffer *>();
    QVERIFY(dataOffer);
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 1);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // sending a new mimetype to the selection, should be announced in the offer
    QSignalSpy mimeTypeAddedSpy(dataOffer, &KWayland::Client::DataOffer::mimeTypeOffered);
    dataSource->offer(QStringLiteral("text/html"));
    QVERIFY(mimeTypeAddedSpy.wait());
    QCOMPARE(mimeTypeAddedSpy.count(), 1);
    QCOMPARE(mimeTypeAddedSpy.first().first().toString(), QStringLiteral("text/html"));
    QCOMPARE(dataOffer->offeredMimeTypes().count(), 2);
    QCOMPARE(dataOffer->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QCOMPARE(dataOffer->offeredMimeTypes().last().name(), QStringLiteral("text/html"));

    // now clear the selection
    dataDevice->clearSelection(2);
    QVERIFY(selectionChangedSpy.wait());
    QCOMPARE(selectionChangedSpy.count(), 2);
    QVERIFY(!deviceInterface->selection());

    // set another selection
    dataDevice->setSelection(3, dataSource.get());
    QVERIFY(selectionChangedSpy.wait());
    // now unbind the dataDevice
    QSignalSpy unboundSpy(deviceInterface, &QObject::destroyed);
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
}

void TestDataDevice::testSendSelectionOnSeat()
{
    // this test verifies that the selection is sent when setting a focused keyboard
    using namespace KWin;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface *>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface, m_seatInterface->nextSerial());

    // now set the selection
    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // now unfocus the keyboard
    m_seatInterface->setFocusedKeyboardSurface(nullptr, m_seatInterface->nextSerial());
    // if setting the same surface again, we should get another offer
    m_seatInterface->setFocusedKeyboardSurface(serverSurface, m_seatInterface->nextSerial());
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);

    // now let's try to destroy the data device and set a focused keyboard just while the data device is being destroyedd
    m_seatInterface->setFocusedKeyboardSurface(nullptr, m_seatInterface->nextSerial());
    QSignalSpy unboundSpy(serverDataDevice, &QObject::destroyed);
    dataDevice.reset();
    QVERIFY(unboundSpy.wait());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface, m_seatInterface->nextSerial());
}

void TestDataDevice::testReplaceSource()
{
    // this test verifies that replacing a data source cancels the previous source
    using namespace KWin;
    // first add keyboard support to Seat
    QSignalSpy keyboardChangedSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardChangedSpy.wait());
    // now create DataDevice, Keyboard and a Surface
    QSignalSpy dataDeviceCreatedSpy(m_dataDeviceManagerInterface, &DataDeviceManagerInterface::dataDeviceCreated);
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice->isValid());
    QVERIFY(dataDeviceCreatedSpy.wait());
    auto serverDataDevice = dataDeviceCreatedSpy.first().first().value<DataDeviceInterface *>();
    QVERIFY(serverDataDevice);
    std::unique_ptr<KWayland::Client::Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(keyboard->isValid());
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());

    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface, m_seatInterface->nextSerial());

    // now set the selection
    std::unique_ptr<KWayland::Client::DataSource> dataSource(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource->isValid());
    dataSource->offer(QStringLiteral("text/plain"));
    dataDevice->setSelection(1, dataSource.get());
    QSignalSpy sourceCancelledSpy(dataSource.get(), &KWayland::Client::DataSource::cancelled);
    // we should get a selection offered for that on the data device
    QSignalSpy selectionOfferedSpy(dataDevice.get(), &KWayland::Client::DataDevice::selectionOffered);
    QVERIFY(selectionOfferedSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 1);

    // create a second data source and replace previous one
    std::unique_ptr<KWayland::Client::DataSource> dataSource2(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource2->isValid());
    dataSource2->offer(QStringLiteral("text/plain"));
    QSignalSpy sourceCancelled2Spy(dataSource2.get(), &KWayland::Client::DataSource::cancelled);
    dataDevice->setSelection(2, dataSource2.get());
    QCOMPARE(selectionOfferedSpy.count(), 1);
    QVERIFY(sourceCancelledSpy.wait());
    QCOMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // replace the data source with itself, ensure that it did not get cancelled
    dataDevice->setSelection(3, dataSource2.get());
    QVERIFY(!sourceCancelled2Spy.wait(500));
    QCOMPARE(selectionOfferedSpy.count(), 2);
    QVERIFY(sourceCancelled2Spy.isEmpty());

    // create a new DataDevice and replace previous one
    std::unique_ptr<KWayland::Client::DataDevice> dataDevice2(m_dataDeviceManager->getDataDevice(m_seat));
    QVERIFY(dataDevice2->isValid());
    std::unique_ptr<KWayland::Client::DataSource> dataSource3(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource3->isValid());
    dataSource3->offer(QStringLiteral("text/plain"));
    dataDevice2->setSelection(4, dataSource3.get());
    QVERIFY(sourceCancelled2Spy.wait());

    // try to crash by first destroying dataSource3 and setting a new DataSource
    std::unique_ptr<KWayland::Client::DataSource> dataSource4(m_dataDeviceManager->createDataSource());
    QVERIFY(dataSource4->isValid());
    dataSource4->offer(QStringLiteral("text/plain"));
    dataSource3.reset();
    dataDevice2->setSelection(5, dataSource4.get());
    QVERIFY(selectionOfferedSpy.wait());

    auto dataOffer = selectionOfferedSpy.last()[0].value<KWayland::Client::DataOffer *>();

    // try to crash by destroying the data source, then requesting data
    dataSource4.reset();
    int pipeFds[2] = {0, 0};
    QVERIFY(pipe(pipeFds) == 0);

    dataOffer->receive(QStringLiteral("text/plain"), pipeFds[1]);
    close(pipeFds[1]);

    // spin the event loop, nothing should explode
    QTest::qWait(10);

    close(pipeFds[0]);
}

QTEST_GUILESS_MAIN(TestDataDevice)
#include "test_datadevice.moc"
