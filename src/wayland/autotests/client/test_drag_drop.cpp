/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/touch.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/datasource_interface.h"
#include "../../src/server/seat_interface.h"

class TestDragAndDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testPointerDragAndDrop();
    void testTouchDragAndDrop();
    void testDragAndDropWithCancelByDestroyDataSource();
    void testPointerEventsIgnored();

private:
    KWayland::Client::Surface *createSurface();
    KWaylandServer::SurfaceInterface *getServerSurface();

    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositorInterface = nullptr;
    KWaylandServer::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    KWaylandServer::SeatInterface *m_seatInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::DataDevice *m_dataDevice = nullptr;
    KWayland::Client::DataSource *m_dataSource = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Pointer *m_pointer = nullptr;
    KWayland::Client::Touch *m_touch = nullptr;
    KWayland::Client::DataDeviceManager *m_ddm = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-drag-n-drop-0");

void TestDragAndDrop::init()
{
    using namespace KWaylandServer;
    using namespace KWayland::Client;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_seatInterface = new SeatInterface(m_display, m_display);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    m_seatInterface->create();
    QVERIFY(m_seatInterface->isValid());
    m_dataDeviceManagerInterface = new DataDeviceManagerInterface(m_display, m_display);
    m_display->createShm();

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &Registry::interfaceAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
#define CREATE(variable, factory, iface) \
    variable = m_registry->create##factory(m_registry->interface(Registry::Interface::iface).name, m_registry->interface(Registry::Interface::iface).version, this); \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_seat, Seat, Seat)
    CREATE(m_ddm, DataDeviceManager, DataDeviceManager)
    CREATE(m_shm, ShmPool, Shm)

#undef CREATE

    QSignalSpy pointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    QVERIFY(pointerSpy.wait());
    m_pointer = m_seat->createPointer(m_seat);
    QVERIFY(m_pointer->isValid());
    m_touch = m_seat->createTouch(m_seat);
    QVERIFY(m_touch->isValid());
    m_dataDevice = m_ddm->getDataDevice(m_seat, this);
    QVERIFY(m_dataDevice->isValid());
    m_dataSource = m_ddm->createDataSource(this);
    QVERIFY(m_dataSource->isValid());
    m_dataSource->offer(QStringLiteral("text/plain"));
}

void TestDragAndDrop::cleanup()
{
#define DELETE(name) \
    if (name) { \
        delete name; \
        name = nullptr; \
    }
    DELETE(m_dataSource)
    DELETE(m_dataDevice)
    DELETE(m_shm)
    DELETE(m_compositor)
    DELETE(m_ddm)
    DELETE(m_seat)
    DELETE(m_queue)
    DELETE(m_registry)
#undef DELETE
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

KWayland::Client::Surface *TestDragAndDrop::createSurface()
{
    using namespace KWayland::Client;
    auto s = m_compositor->createSurface();

    QImage img(QSize(100, 200), QImage::Format_RGB32);
    img.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(img));
    s->damage(QRect(0, 0, 100, 200));
    s->commit(Surface::CommitFlag::None);
    return s;
}

KWaylandServer::SurfaceInterface *TestDragAndDrop::getServerSurface()
{
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    if (!surfaceCreatedSpy.isValid()) {
        return nullptr;
    }
    if (!surfaceCreatedSpy.wait(500)) {
        return nullptr;
    }
    return surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
}

void TestDragAndDrop::testPointerDragAndDrop()
{
    // this test verifies the very basic drag and drop on one surface, an enter, a move and the drop
    using namespace KWaylandServer;
    using namespace KWayland::Client;
    // first create a window
    QScopedPointer<Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonPressSpy.isValid());
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    m_seatInterface->setTimestamp(2);
    m_seatInterface->pointerButtonPressed(1);
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy sourceDropSpy(m_dataSource, &DataSource::dragAndDropPerformed);
    QVERIFY(sourceDropSpy.isValid());

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.isValid());
    m_dataSource->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, s.data());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragSource()->icon());
    QCOMPARE(m_seatInterface->dragSource()->dragImplicitGrabSerial(), buttonPressSpy.first().first().value<quint32>());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.data());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move, DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(3);
    m_seatInterface->setPointerPos(QPointF(3, 3));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QSignalSpy droppedSpy(m_dataDevice, &DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_seatInterface->setTimestamp(4);
    m_seatInterface->pointerButtonReleased(1);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &DataSource::dragAndDropFinished);
    QVERIFY(finishedSpy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(pointerMotionSpy.isEmpty());
    QCOMPARE(buttonPressSpy.count(), 1);
}

void TestDragAndDrop::testTouchDragAndDrop()
{
    // this test verifies the very basic drag and drop on one surface, an enter, a move and the drop
    using namespace KWaylandServer;
    using namespace KWayland::Client;
    // first create a window
    QScopedPointer<Surface> s(createSurface());
    s->setSize(QSize(100,100));
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy pointAddedSpy(m_touch, &Touch::pointAdded);
    QVERIFY(pointAddedSpy.isValid());
    m_seatInterface->setFocusedTouchSurface(serverSurface);
    m_seatInterface->setTimestamp(2);
    const qint32 touchId = m_seatInterface->touchDown(QPointF(50,50));
    QVERIFY(sequenceStartedSpy.wait());

    QScopedPointer<TouchPoint> tp(sequenceStartedSpy.first().at(0).value<TouchPoint*>());
    QVERIFY(!tp.isNull());
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy touchMotionSpy(m_touch, &Touch::pointMoved);
    QVERIFY(touchMotionSpy.isValid());
    QSignalSpy sourceDropSpy(m_dataSource, &DataSource::dragAndDropPerformed);
    QVERIFY(sourceDropSpy.isValid());

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.isValid());
    m_dataSource->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(tp->downSerial(), m_dataSource, s.data());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragSource()->icon());
    QCOMPARE(m_seatInterface->dragSource()->dragImplicitGrabSerial(), tp->downSerial());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.data());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move, DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(3);
    m_seatInterface->touchMove(touchId, QPointF(75, 75));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QSignalSpy droppedSpy(m_dataDevice, &DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_seatInterface->setTimestamp(4);
    m_seatInterface->touchUp(touchId);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &DataSource::dragAndDropFinished);
    QVERIFY(finishedSpy.isValid());
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(touchMotionSpy.isEmpty());
    QCOMPARE(pointAddedSpy.count(), 0);
}


void TestDragAndDrop::testDragAndDropWithCancelByDestroyDataSource()
{
    // this test simulates the problem from BUG 389221
    using namespace KWaylandServer;
    using namespace KWayland::Client;
    // first create a window
    QScopedPointer<Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &DataSource::selectedDragAndDropActionChanged);
    QVERIFY(dataSourceSelectedActionChangedSpy.isValid());

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonPressSpy.isValid());
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    m_seatInterface->setTimestamp(2);
    m_seatInterface->pointerButtonPressed(1);
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());
    QSignalSpy dragMotionSpy(m_dataDevice, &DataDevice::dragMotion);
    QVERIFY(dragMotionSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy dragLeftSpy(m_dataDevice, &DataDevice::dragLeft);
    QVERIFY(dragLeftSpy.isValid());

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    QVERIFY(dragStartedSpy.isValid());
    m_dataSource->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, s.data());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragSource()->icon());
    QCOMPARE(m_seatInterface->dragSource()->dragImplicitGrabSerial(), buttonPressSpy.first().first().value<quint32>());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.data());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &DataOffer::selectedDragAndDropActionChanged);
    QVERIFY(offerActionChangedSpy.isValid());
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(DataDeviceManager::DnDAction::Copy | DataDeviceManager::DnDAction::Move, DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(3);
    m_seatInterface->setPointerPos(QPointF(3, 3));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // now delete the DataSource
    delete m_dataSource;
    m_dataSource = nullptr;
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QVERIFY(serverDragEndedSpy.isValid());
    QVERIFY(dragLeftSpy.isEmpty());
    QVERIFY(dragLeftSpy.wait());
    QTRY_COMPARE(dragLeftSpy.count(), 1);
    QTRY_COMPARE(serverDragEndedSpy.count(), 1);

    // simulate drop
    QSignalSpy droppedSpy(m_dataDevice, &DataDevice::dropped);
    QVERIFY(droppedSpy.isValid());
    m_seatInterface->setTimestamp(4);
    m_seatInterface->pointerButtonReleased(1);
    QVERIFY(!droppedSpy.wait(500));

    // verify that we did not get any further input events
    QVERIFY(pointerMotionSpy.isEmpty());
    QCOMPARE(buttonPressSpy.count(), 2);
}

void TestDragAndDrop::testPointerEventsIgnored()
{
    // this test verifies that all pointer events are ignored on the focused Pointer device during drag
    using namespace KWaylandServer;
    using namespace KWayland::Client;
    // first create a window
    QScopedPointer<Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    // pass it pointer focus
    m_seatInterface->setFocusedPointerSurface(serverSurface);

    // create signal spies for all the pointer events
    QSignalSpy pointerEnteredSpy(m_pointer, &Pointer::entered);
    QVERIFY(pointerEnteredSpy.isValid());
    QSignalSpy pointerLeftSpy(m_pointer, &Pointer::left);
    QVERIFY(pointerLeftSpy.isValid());
    QSignalSpy pointerMotionSpy(m_pointer, &Pointer::motion);
    QVERIFY(pointerMotionSpy.isValid());
    QSignalSpy axisSpy(m_pointer, &Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());
    QSignalSpy buttonSpy(m_pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());
    QSignalSpy dragEnteredSpy(m_dataDevice, &DataDevice::dragEntered);
    QVERIFY(dragEnteredSpy.isValid());

    // first simulate a few things
    quint32 timestamp = 1;
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(10, 10));
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerAxis(Qt::Vertical, 5);
    // verify that we have those
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QVERIFY(buttonSpy.isEmpty());
    QVERIFY(pointerLeftSpy.isEmpty());

    // let's start the drag
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonPressed(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.count(), 1);
    m_dataDevice->startDrag(buttonSpy.first().first().value<quint32>(), m_dataSource, s.data());
    QVERIFY(dragEnteredSpy.wait());

    // now simulate all the possible pointer interactions
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonPressed(2);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonReleased(2);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerAxis(Qt::Vertical, 5);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerAxis(Qt::Horizontal, 5);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setFocusedPointerSurface(nullptr);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(50, 50));

    // last but not least, simulate the drop
    QSignalSpy cancelledSpy(m_dataSource, &DataSource::cancelled);
    QVERIFY(cancelledSpy.isValid());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonReleased(1);
    QVERIFY(cancelledSpy.wait());

    // all the changes should have been ignored
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QCOMPARE(buttonSpy.count(), 1);
    QVERIFY(pointerLeftSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestDragAndDrop)
#include "test_drag_drop.moc"
