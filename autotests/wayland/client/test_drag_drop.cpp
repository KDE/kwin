/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QMimeType>
#include <QSignalSpy>
#include <QTest>
// KWin
#include "wayland/compositor.h"
#include "wayland/datadevicemanager.h"
#include "wayland/datasource.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/seat_p.h"
#include "wayland/subcompositor.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/subcompositor.h"
#include "KWayland/Client/subsurface.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/touch.h"

using namespace std::literals;

class TestDragAndDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testPointerDragAndDrop();
    void testPointerSubsurfaceDragAndDrop();
    void testTouchDragAndDrop();
    void testTouchSubsurfacesDragAndDrop();
    void testDragAndDropWithCancelByDestroyDataSource();
    void testPointerEventsIgnored();

private:
    KWayland::Client::Surface *createSurface();
    KWayland::Client::SubSurface *createSubSurface(KWayland::Client::Surface *surface, KWayland::Client::Surface *parentSurface);
    KWin::SurfaceInterface *getServerSurface();

    KWin::Display *m_display = nullptr;
    KWin::CompositorInterface *m_compositorInterface = nullptr;
    KWin::SubCompositorInterface *m_subcompositorInterface = nullptr;
    KWin::DataDeviceManagerInterface *m_dataDeviceManagerInterface = nullptr;
    KWin::SeatInterface *m_seatInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::SubCompositor *m_subcompositor = nullptr;
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

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_subcompositorInterface = new SubCompositorInterface(m_display, m_display);
    m_seatInterface = new SeatInterface(m_display, QStringLiteral("seat0"), m_display);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    m_dataDeviceManagerInterface = new DataDeviceManagerInterface(m_display, m_display);
    m_display->createShm();

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    m_registry = new KWayland::Client::Registry();
    QSignalSpy interfacesAnnouncedSpy(m_registry, &KWayland::Client::Registry::interfaceAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
#define CREATE(variable, factory, iface)                                                                                                                      \
    variable =                                                                                                                                                \
        m_registry->create##factory(m_registry->interface(KWayland::Client::Registry::Interface::iface).name, m_registry->interface(KWayland::Client::Registry::Interface::iface).version, this); \
    QVERIFY(variable);

    CREATE(m_compositor, Compositor, Compositor)
    CREATE(m_subcompositor, SubCompositor, SubCompositor)
    CREATE(m_seat, Seat, Seat)
    CREATE(m_ddm, DataDeviceManager, DataDeviceManager)
    CREATE(m_shm, ShmPool, Shm)

#undef CREATE

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
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
#define DELETE(name)    \
    if (name) {         \
        delete name;    \
        name = nullptr; \
    }
    DELETE(m_dataSource)
    DELETE(m_dataDevice)
    DELETE(m_shm)
    DELETE(m_compositor)
    DELETE(m_subcompositor)
    DELETE(m_ddm)
    DELETE(m_seat)
    DELETE(m_registry)
    DELETE(m_queue)
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
    auto s = m_compositor->createSurface();

    QImage img(QSize(100, 200), QImage::Format_RGB32);
    img.fill(Qt::red);
    s->attachBuffer(m_shm->createBuffer(img));
    s->damage(QRect(0, 0, 100, 200));
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    return s;
}

KWayland::Client::SubSurface *TestDragAndDrop::createSubSurface(KWayland::Client::Surface *surface, KWayland::Client::Surface *parentSurface)
{
    auto s = m_subcompositor->createSubSurface(surface, parentSurface);
    Q_ASSERT(s->isValid());
    return s;
}

KWin::SurfaceInterface *TestDragAndDrop::getServerSurface()
{
    using namespace KWin;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    if (!surfaceCreatedSpy.isValid()) {
        return nullptr;
    }
    if (!surfaceCreatedSpy.wait(500)) {
        return nullptr;
    }
    return surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
}

void TestDragAndDrop::testPointerDragAndDrop()
{
    // this test verifies the very basic drag and drop on one surface, an enter, a move and the drop
    using namespace KWin;
    // first create a window
    std::unique_ptr<KWayland::Client::Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    auto timestamp = 2ms;

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &KWayland::Client::Pointer::buttonStateChanged);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dragMotionSpy(m_dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy pointerMotionSpy(m_pointer, &KWayland::Client::Pointer::motion);
    QSignalSpy sourceDropSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    m_dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, s.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragIcon());
    QCOMPARE(SeatInterfacePrivate::get(m_seatInterface)->drag.dragImplicitGrabSerial, buttonPressSpy.first().first().value<quint32>());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(3, 3));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QSignalSpy droppedSpy(m_dataDevice, &KWayland::Client::DataDevice::dropped);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(pointerMotionSpy.isEmpty());
}

void TestDragAndDrop::testPointerSubsurfaceDragAndDrop()
{
    // this test verifies drag and drop from a subsurface works
    using namespace KWin;

    std::unique_ptr<KWayland::Client::Surface> parentSurface(createSurface());
    parentSurface->setSize(QSize(100, 100));
    auto parentServerSurface = getServerSurface();
    QVERIFY(parentServerSurface);

    std::unique_ptr<KWayland::Client::Surface> childSurface(createSurface());
    childSurface->setSize(QSize(100, 100));
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(createSubSurface(childSurface.get(), parentSurface.get()));
    QVERIFY(subSurface);
    subSurface->setPosition({0, 0});
    auto childServerSurface = getServerSurface();
    QVERIFY(childServerSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);
    auto timestamp = 2ms;

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &KWayland::Client::Pointer::buttonStateChanged);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(parentServerSurface, QPointF(0, 0));
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dragMotionSpy(m_dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy pointerMotionSpy(m_pointer, &KWayland::Client::Pointer::motion);
    QSignalSpy sourceDropSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    m_dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, childSurface.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), parentServerSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragIcon());
    QCOMPARE(SeatInterfacePrivate::get(m_seatInterface)->drag.dragImplicitGrabSerial, buttonPressSpy.first().first().value<quint32>());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), parentSurface.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(3, 3));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QSignalSpy droppedSpy(m_dataDevice, &KWayland::Client::DataDevice::dropped);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(pointerMotionSpy.isEmpty());
}

void TestDragAndDrop::testTouchSubsurfacesDragAndDrop()
{
    // this test verifies the very basic drag and drop on one surface, an enter, a move and the drop
    using namespace KWin;
    // first create a window
    std::unique_ptr<KWayland::Client::Surface> parentSurface(createSurface());
    parentSurface->setSize(QSize(100, 100));

    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::Surface> s(createSurface());
    s->setSize(QSize(100, 100));
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(createSubSurface(s.get(), parentSurface.get()));
    QVERIFY(subSurface);
    subSurface->setPosition({0, 0});

    auto serverChildSurface = getServerSurface();
    QVERIFY(serverChildSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    auto timestamp = 2ms;

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequenceStartedSpy(m_touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy pointAddedSpy(m_touch, &KWayland::Client::Touch::pointAdded);
    m_seatInterface->setTimestamp(timestamp++);
    const qint32 touchId = 0;
    m_seatInterface->notifyTouchDown(serverSurface, QPoint(0, 0), touchId, QPointF(50, 50));
    QVERIFY(sequenceStartedSpy.wait());

    std::unique_ptr<KWayland::Client::TouchPoint> tp(sequenceStartedSpy.first().at(0).value<KWayland::Client::TouchPoint *>());
    QVERIFY(tp != nullptr);
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dragMotionSpy(m_dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy touchMotionSpy(m_touch, &KWayland::Client::Touch::pointMoved);
    QSignalSpy sourceDropSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    m_dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(tp->downSerial(), m_dataSource, s.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragIcon());
    QCOMPARE(SeatInterfacePrivate::get(m_seatInterface)->drag.dragImplicitGrabSerial, tp->downSerial());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(50.0, 50.0));
    QCOMPARE(m_dataDevice->dragSurface().data(), parentSurface.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchMotion(touchId, QPointF(75, 75));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QSignalSpy droppedSpy(m_dataDevice, &KWayland::Client::DataDevice::dropped);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchUp(touchId);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
    offer->dragAndDropFinished();
    QVERIFY(finishedSpy.wait());
    delete offer;

    // verify that we did not get any further input events
    QVERIFY(touchMotionSpy.isEmpty());
    QCOMPARE(pointAddedSpy.count(), 0);
}

void TestDragAndDrop::testTouchDragAndDrop()
{
    // this test verifies the very basic drag and drop on one surface, an enter, a move and the drop
    using namespace KWin;
    // first create a window
    std::unique_ptr<KWayland::Client::Surface> s(createSurface());
    s->setSize(QSize(100, 100));
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    auto timestamp = 2ms;

    // now we need to pass touch focus to the Surface and simulate a touch down
    QSignalSpy sequenceStartedSpy(m_touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy pointAddedSpy(m_touch, &KWayland::Client::Touch::pointAdded);
    m_seatInterface->setTimestamp(timestamp++);
    const qint32 touchId = 0;
    m_seatInterface->notifyTouchDown(serverSurface, QPoint(0, 0), touchId, QPointF(50, 50));
    QVERIFY(sequenceStartedSpy.wait());

    std::unique_ptr<KWayland::Client::TouchPoint> tp(sequenceStartedSpy.first().at(0).value<KWayland::Client::TouchPoint *>());
    QVERIFY(tp != nullptr);
    QCOMPARE(tp->time(), quint32(2));

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dragMotionSpy(m_dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy touchMotionSpy(m_touch, &KWayland::Client::Touch::pointMoved);
    QSignalSpy sourceDropSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropPerformed);

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    m_dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(tp->downSerial(), m_dataSource, s.get());
    QVERIFY(dragStartedSpy.wait());
    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragIcon());
    QCOMPARE(SeatInterfacePrivate::get(m_seatInterface)->drag.dragImplicitGrabSerial, tp->downSerial());
    QVERIFY(dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(50.0, 50.0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchMotion(touchId, QPointF(75, 75));
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(75, 75));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // simulate drop
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QSignalSpy droppedSpy(m_dataDevice, &KWayland::Client::DataDevice::dropped);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchUp(touchId);
    QVERIFY(sourceDropSpy.isEmpty());
    QVERIFY(droppedSpy.wait());
    QCOMPARE(sourceDropSpy.count(), 1);
    QCOMPARE(serverDragEndedSpy.count(), 1);

    QSignalSpy finishedSpy(m_dataSource, &KWayland::Client::DataSource::dragAndDropFinished);
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
    using namespace KWin;
    // first create a window
    std::unique_ptr<KWayland::Client::Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    QSignalSpy dataSourceSelectedActionChangedSpy(m_dataSource, &KWayland::Client::DataSource::selectedDragAndDropActionChanged);

    auto timestamp = 2ms;

    // now we need to pass pointer focus to the Surface and simulate a button press
    QSignalSpy buttonPressSpy(m_pointer, &KWayland::Client::Pointer::buttonStateChanged);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonPressSpy.wait());
    QCOMPARE(buttonPressSpy.first().at(1).value<quint32>(), quint32(2));

    QSignalSpy pointerLeftSpy(m_pointer, &KWayland::Client::Pointer::left);

    // add some signal spies for client side
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy dragMotionSpy(m_dataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy pointerMotionSpy(m_pointer, &KWayland::Client::Pointer::motion);
    QSignalSpy dragLeftSpy(m_dataDevice, &KWayland::Client::DataDevice::dragLeft);

    // now we can start the drag and drop
    QSignalSpy dragStartedSpy(m_seatInterface, &SeatInterface::dragStarted);
    m_dataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    m_dataDevice->startDrag(buttonPressSpy.first().first().value<quint32>(), m_dataSource, s.get());
    QVERIFY(dragStartedSpy.wait());

    QCOMPARE(m_seatInterface->dragSurface(), serverSurface);
    QCOMPARE(m_seatInterface->dragSurfaceTransformation(), QMatrix4x4());
    QVERIFY(!m_seatInterface->dragIcon());
    QCOMPARE(SeatInterfacePrivate::get(m_seatInterface)->drag.dragImplicitGrabSerial, buttonPressSpy.first().first().value<quint32>());
    QVERIFY(pointerLeftSpy.wait());
    QVERIFY(dragEnteredSpy.count() || dragEnteredSpy.wait());
    QCOMPARE(dragEnteredSpy.count(), 1);
    QCOMPARE(dragEnteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(dragEnteredSpy.first().last().toPointF(), QPointF(0, 0));
    QCOMPARE(m_dataDevice->dragSurface().data(), s.get());
    auto offer = m_dataDevice->dragOffer();
    QVERIFY(offer);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::None);
    QSignalSpy offerActionChangedSpy(offer, &KWayland::Client::DataOffer::selectedDragAndDropActionChanged);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().count(), 1);
    QCOMPARE(m_dataDevice->dragOffer()->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));
    QTRY_COMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    offer->accept(QStringLiteral("text/plain"), dragEnteredSpy.last().at(0).toUInt());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move, KWayland::Client::DataDeviceManager::DnDAction::Move);
    QVERIFY(offerActionChangedSpy.wait());
    QCOMPARE(offerActionChangedSpy.count(), 1);
    QCOMPARE(offer->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);
    QCOMPARE(dataSourceSelectedActionChangedSpy.count(), 1);
    QCOMPARE(m_dataSource->selectedDragAndDropAction(), KWayland::Client::DataDeviceManager::DnDAction::Move);

    // simulate motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(3, 3));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(dragMotionSpy.wait());
    QCOMPARE(dragMotionSpy.count(), 1);
    QCOMPARE(dragMotionSpy.first().first().toPointF(), QPointF(3, 3));
    QCOMPARE(dragMotionSpy.first().last().toUInt(), 3u);

    // now delete the DataSource
    delete m_dataSource;
    m_dataSource = nullptr;
    QSignalSpy serverDragEndedSpy(m_seatInterface, &SeatInterface::dragEnded);
    QVERIFY(dragLeftSpy.isEmpty());
    QVERIFY(dragLeftSpy.wait());
    QTRY_COMPARE(dragLeftSpy.count(), 1);
    QTRY_COMPARE(serverDragEndedSpy.count(), 1);

    // simulate drop
    QSignalSpy droppedSpy(m_dataDevice, &KWayland::Client::DataDevice::dropped);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(!droppedSpy.wait(500));

    // verify that we did not get any further input events
    QVERIFY(pointerMotionSpy.isEmpty());
}

void TestDragAndDrop::testPointerEventsIgnored()
{
    // this test verifies that all pointer events are ignored on the focused Pointer device during drag
    using namespace KWin;
    // first create a window
    std::unique_ptr<KWayland::Client::Surface> s(createSurface());
    auto serverSurface = getServerSurface();
    QVERIFY(serverSurface);

    // pass it pointer focus
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));

    // create signal spies for all the pointer events
    QSignalSpy pointerEnteredSpy(m_pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy pointerLeftSpy(m_pointer, &KWayland::Client::Pointer::left);
    QSignalSpy pointerMotionSpy(m_pointer, &KWayland::Client::Pointer::motion);
    QSignalSpy axisSpy(m_pointer, &KWayland::Client::Pointer::axisChanged);
    QSignalSpy buttonSpy(m_pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy dragEnteredSpy(m_dataDevice, &KWayland::Client::DataDevice::dragEntered);

    // first simulate a few things
    auto timestamp = 1ms;
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(10, 10));
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Vertical, 5, 120, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    // verify that we have those
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QVERIFY(buttonSpy.isEmpty());
    QVERIFY(pointerLeftSpy.isEmpty());

    // let's start the drag
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.count(), 1);
    m_dataDevice->startDrag(buttonSpy.first().first().value<quint32>(), m_dataSource, s.get());
    QVERIFY(dragEnteredSpy.wait());

    // now simulate all the possible pointer interactions
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(2, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(2, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Vertical, 5, 1, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Vertical, 5, 1, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerLeave();
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(serverSurface, m_seatInterface->pointerPos());
    m_seatInterface->notifyPointerFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(50, 50));
    m_seatInterface->notifyPointerFrame();

    // last but not least, simulate the drop
    QSignalSpy cancelledSpy(m_dataSource, &KWayland::Client::DataSource::cancelled);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(cancelledSpy.wait());

    // all the changes should have been ignored
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(pointerMotionSpy.count(), 1);
    QCOMPARE(pointerEnteredSpy.count(), 1);
    QCOMPARE(pointerLeftSpy.count(), 1);
}

QTEST_GUILESS_MAIN(TestDragAndDrop)
#include "test_drag_drop.moc"
