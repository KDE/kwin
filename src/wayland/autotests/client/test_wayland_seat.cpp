/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/datadevicemanager_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/display.h"
#include "wayland/keyboard_interface.h"
#include "wayland/pointer_interface.h"
#include "wayland/pointergestures_v1_interface.h"
#include "wayland/relativepointer_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/shmclientbuffer.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/surface_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/datadevice.h"
#include "KWayland/Client/datadevicemanager.h"
#include "KWayland/Client/datasource.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/pointer.h"
#include "KWayland/Client/pointergestures.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/relativepointer.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/subcompositor.h"
#include "KWayland/Client/subsurface.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/touch.h"

// Wayland
#include "qwayland-pointer-gestures-unstable-v1.h"
#include <wayland-client-protocol.h>

#include <linux/input.h>
// System
#include <fcntl.h>
#include <unistd.h>

using namespace std::literals;

class TestWaylandSeat : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandSeat(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testName();
    void testCapabilities_data();
    void testCapabilities();
    void testPointer();
    void testPointerTransformation_data();
    void testPointerTransformation();
    void testPointerButton_data();
    void testPointerButton();
    void testPointerSubSurfaceTree();
    void testPointerSwipeGesture_data();
    void testPointerSwipeGesture();
    void testPointerPinchGesture_data();
    void testPointerPinchGesture();
    void testPointerHoldGesture_data();
    void testPointerHoldGesture();
    void testPointerAxis();
    void testCursor();
    void testCursorDamage();
    void testKeyboard();
    void testSelection();
    void testDataDeviceForKeyboardSurface();
    void testTouch();
    void testKeymap();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::SeatInterface *m_seatInterface;
    KWaylandServer::SubCompositorInterface *m_subCompositorInterface;
    KWaylandServer::RelativePointerManagerV1Interface *m_relativePointerManagerV1Interface;
    KWaylandServer::PointerGesturesV1Interface *m_pointerGesturesV1Interface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Seat *m_seat;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::SubCompositor *m_subCompositor;
    KWayland::Client::RelativePointerManager *m_relativePointerManager;
    KWayland::Client::PointerGestures *m_pointerGestures;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-seat-0");

TestWaylandSeat::TestWaylandSeat(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_seatInterface(nullptr)
    , m_subCompositorInterface(nullptr)
    , m_relativePointerManagerV1Interface(nullptr)
    , m_pointerGesturesV1Interface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_seat(nullptr)
    , m_shm(nullptr)
    , m_subCompositor(nullptr)
    , m_relativePointerManager(nullptr)
    , m_pointerGestures(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandSeat::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_subCompositorInterface = new SubCompositorInterface(m_display, m_display);
    QVERIFY(m_subCompositorInterface);

    m_relativePointerManagerV1Interface = new RelativePointerManagerV1Interface(m_display, m_display);
    m_pointerGesturesV1Interface = new PointerGesturesV1Interface(m_display, m_display);

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
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy seatSpy(&registry, &KWayland::Client::Registry::seatAnnounced);
    QSignalSpy shmSpy(&registry, &KWayland::Client::Registry::shmAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_seatInterface = new SeatInterface(m_display, m_display);
    QVERIFY(m_seatInterface);
    m_seatInterface->setName(QStringLiteral("seat0"));
    QVERIFY(seatSpy.wait());

    m_compositor = new KWayland::Client::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QSignalSpy nameSpy(m_seat, &KWayland::Client::Seat::nameChanged);
    QVERIFY(nameSpy.wait());

    m_shm = new KWayland::Client::ShmPool(this);
    m_shm->setup(registry.bindShm(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    QVERIFY(m_shm->isValid());

    m_subCompositor = registry.createSubCompositor(registry.interface(KWayland::Client::Registry::Interface::SubCompositor).name,
                                                   registry.interface(KWayland::Client::Registry::Interface::SubCompositor).version,
                                                   this);
    QVERIFY(m_subCompositor->isValid());

    m_relativePointerManager =
        registry.createRelativePointerManager(registry.interface(KWayland::Client::Registry::Interface::RelativePointerManagerUnstableV1).name,
                                              registry.interface(KWayland::Client::Registry::Interface::RelativePointerManagerUnstableV1).version,
                                              this);
    QVERIFY(m_relativePointerManager->isValid());

    m_pointerGestures = registry.createPointerGestures(registry.interface(KWayland::Client::Registry::Interface::PointerGesturesUnstableV1).name,
                                                       registry.interface(KWayland::Client::Registry::Interface::PointerGesturesUnstableV1).version,
                                                       this);
    QVERIFY(m_pointerGestures->isValid());
}

void TestWaylandSeat::cleanup()
{
    if (m_pointerGestures) {
        delete m_pointerGestures;
        m_pointerGestures = nullptr;
    }
    if (m_relativePointerManager) {
        delete m_relativePointerManager;
        m_relativePointerManager = nullptr;
    }
    if (m_subCompositor) {
        delete m_subCompositor;
        m_subCompositor = nullptr;
    }
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
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
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    delete m_display;
    m_display = nullptr;

    // these are the children of the display
    m_compositorInterface = nullptr;
    m_seatInterface = nullptr;
    m_subCompositorInterface = nullptr;
    m_relativePointerManagerV1Interface = nullptr;
    m_pointerGesturesV1Interface = nullptr;
}

void TestWaylandSeat::testName()
{
    // no name set yet
    QCOMPARE(m_seat->name(), QStringLiteral("seat0"));

    QSignalSpy spy(m_seat, &KWayland::Client::Seat::nameChanged);

    const QString name = QStringLiteral("foobar");
    m_seatInterface->setName(name);
    QVERIFY(spy.wait());
    QCOMPARE(m_seat->name(), name);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), name);
}

void TestWaylandSeat::testCapabilities_data()
{
    QTest::addColumn<bool>("pointer");
    QTest::addColumn<bool>("keyboard");
    QTest::addColumn<bool>("touch");

    QTest::newRow("none") << false << false << false;
    QTest::newRow("pointer") << true << false << false;
    QTest::newRow("keyboard") << false << true << false;
    QTest::newRow("touch") << false << false << true;
    QTest::newRow("pointer/keyboard") << true << true << false;
    QTest::newRow("pointer/touch") << true << false << true;
    QTest::newRow("keyboard/touch") << false << true << true;
    QTest::newRow("all") << true << true << true;
}

void TestWaylandSeat::testCapabilities()
{
    QVERIFY(!m_seat->hasPointer());
    QVERIFY(!m_seat->hasKeyboard());
    QVERIFY(!m_seat->hasTouch());

    QFETCH(bool, pointer);
    QFETCH(bool, keyboard);
    QFETCH(bool, touch);

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    QSignalSpy keyboardSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    QSignalSpy touchSpy(m_seat, &KWayland::Client::Seat::hasTouchChanged);

    m_seatInterface->setHasPointer(pointer);
    m_seatInterface->setHasKeyboard(keyboard);
    m_seatInterface->setHasTouch(touch);

    // do processing
    QCOMPARE(pointerSpy.wait(1000), pointer);
    QCOMPARE(pointerSpy.isEmpty(), !pointer);
    if (!pointerSpy.isEmpty()) {
        QCOMPARE(pointerSpy.first().first().toBool(), pointer);
    }

    if (keyboardSpy.isEmpty()) {
        QCOMPARE(keyboardSpy.wait(1000), keyboard);
    }
    QCOMPARE(keyboardSpy.isEmpty(), !keyboard);
    if (!keyboardSpy.isEmpty()) {
        QCOMPARE(keyboardSpy.first().first().toBool(), keyboard);
    }

    if (touchSpy.isEmpty()) {
        QCOMPARE(touchSpy.wait(1000), touch);
    }
    QCOMPARE(touchSpy.isEmpty(), !touch);
    if (!touchSpy.isEmpty()) {
        QCOMPARE(touchSpy.first().first().toBool(), touch);
    }

    QCOMPARE(m_seat->hasPointer(), pointer);
    QCOMPARE(m_seat->hasKeyboard(), keyboard);
    QCOMPARE(m_seat->hasTouch(), touch);
}

void TestWaylandSeat::testPointer()
{
    using namespace KWaylandServer;

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(image.rect());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), QPointF(10, 15));

    KWayland::Client::Pointer *p = m_seat->createPointer(m_seat);
    QSignalSpy frameSpy(p, &KWayland::Client::Pointer::frame);
    const KWayland::Client::Pointer &cp = *p;
    QVERIFY(p->isValid());
    std::unique_ptr<KWayland::Client::RelativePointer> relativePointer(m_relativePointerManager->createRelativePointer(p));
    QVERIFY(relativePointer->isValid());
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 1);

    m_seatInterface->notifyPointerLeave();
    serverSurface->client()->flush();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 2);

    QSignalSpy enteredSpy(p, &KWayland::Client::Pointer::entered);

    QSignalSpy leftSpy(p, &KWayland::Client::Pointer::left);

    QSignalSpy motionSpy(p, &KWayland::Client::Pointer::motion);

    QSignalSpy axisSpy(p, &KWayland::Client::Pointer::axisChanged);

    QSignalSpy buttonSpy(p, &KWayland::Client::Pointer::buttonStateChanged);

    QSignalSpy relativeMotionSpy(relativePointer.get(), &KWayland::Client::RelativePointer::relativeMotion);

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), QPointF(10, 15));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));
    QCOMPARE(frameSpy.count(), 3);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    auto timestamp = 1ms;
    // test motion
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPoint(10, 16));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(motionSpy.wait());
    QCOMPARE(frameSpy.count(), 4);
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // test relative motion
    m_seatInterface->relativePointerMotion(QPointF(1, 2), QPointF(3, 4), 1234us);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(relativeMotionSpy.wait());
    QCOMPARE(relativeMotionSpy.count(), 1);
    QCOMPARE(frameSpy.count(), 5);
    QCOMPARE(relativeMotionSpy.first().at(0).toSizeF(), QSizeF(1, 2));
    QCOMPARE(relativeMotionSpy.first().at(1).toSizeF(), QSizeF(3, 4));
    QCOMPARE(relativeMotionSpy.first().at(2).value<quint64>(), 1234);

    // test axis
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Horizontal, 10, 120, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(axisSpy.wait());
    QCOMPARE(frameSpy.count(), 6);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Vertical, 20, 240, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(axisSpy.wait());
    QCOMPARE(frameSpy.count(), 7);
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // test button
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonSpy.wait());
    QCOMPARE(frameSpy.count(), 8);
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(2, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonSpy.wait());
    QCOMPARE(frameSpy.count(), 9);
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(2, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonSpy.wait());
    QCOMPARE(frameSpy.count(), 10);
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerButton(1, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(buttonSpy.wait());
    QCOMPARE(frameSpy.count(), 11);
    QCOMPARE(buttonSpy.count(), 4);

    // timestamp
    QCOMPARE(buttonSpy.at(0).at(1).value<quint32>(), quint32(4));
    // button
    QCOMPARE(buttonSpy.at(0).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(0).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Pressed);

    // timestamp
    QCOMPARE(buttonSpy.at(1).at(1).value<quint32>(), quint32(5));
    // button
    QCOMPARE(buttonSpy.at(1).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(1).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Pressed);

    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(2));
    // timestamp
    QCOMPARE(buttonSpy.at(2).at(1).value<quint32>(), quint32(6));
    // button
    QCOMPARE(buttonSpy.at(2).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(2).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);

    QCOMPARE(buttonSpy.at(3).at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(1));
    // timestamp
    QCOMPARE(buttonSpy.at(3).at(1).value<quint32>(), quint32(7));
    // button
    QCOMPARE(buttonSpy.at(3).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(3).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);

    // leave the surface
    m_seatInterface->notifyPointerLeave();
    QVERIFY(leftSpy.wait());
    QCOMPARE(frameSpy.count(), 12);
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // now a relative motion should not be sent to the relative pointer
    m_seatInterface->relativePointerMotion(QPointF(1, 2), QPointF(3, 4), std::chrono::milliseconds::zero());
    QVERIFY(!relativeMotionSpy.wait(500));

    // enter it again
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(10, 16), QPointF(0, 0));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(frameSpy.count(), 13);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // send another relative motion event
    m_seatInterface->relativePointerMotion(QPointF(4, 5), QPointF(6, 7), 1234us);
    QVERIFY(relativeMotionSpy.wait());
    QCOMPARE(relativeMotionSpy.count(), 2);
    QCOMPARE(relativeMotionSpy.last().at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(relativeMotionSpy.last().at(1).toSizeF(), QSizeF(6, 7));
    QCOMPARE(relativeMotionSpy.last().at(2).value<quint64>(), 1234);
}

void TestWaylandSeat::testPointerTransformation_data()
{
    QTest::addColumn<QMatrix4x4>("enterTransformation");
    // global position at 20/18
    QTest::addColumn<QPointF>("expectedEnterPoint");
    // global position at 10/16
    QTest::addColumn<QPointF>("expectedMovePoint");

    QMatrix4x4 tm;
    tm.translate(-10, -15);
    QTest::newRow("translation") << tm << QPointF(10, 3) << QPointF(0, 1);
    QMatrix4x4 sm;
    sm.scale(2, 2);
    QTest::newRow("scale") << sm << QPointF(40, 36) << QPointF(20, 32);
    QMatrix4x4 rotate;
    rotate.rotate(90, 0, 0, 1);
    QTest::newRow("rotate") << rotate << QPointF(-18, 20) << QPointF(-16, 10);
}

void TestWaylandSeat::testPointerTransformation()
{
    using namespace KWaylandServer;

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(image.rect());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    QFETCH(QMatrix4x4, enterTransformation);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), enterTransformation);
    QCOMPARE(m_seatInterface->focusedPointerSurfaceTransformation(), enterTransformation);
    // no pointer yet
    QVERIFY(m_seatInterface->focusedPointerSurface());

    KWayland::Client::Pointer *p = m_seat->createPointer(m_seat);
    QVERIFY(p->isValid());
    QSignalSpy frameSpy(p, &KWayland::Client::Pointer::frame);
    QVERIFY(frameSpy.wait());
    const KWayland::Client::Pointer &cp = *p;

    m_seatInterface->notifyPointerLeave();
    serverSurface->client()->flush();
    QTest::qWait(100);

    QSignalSpy enteredSpy(p, &KWayland::Client::Pointer::entered);

    QSignalSpy leftSpy(p, &KWayland::Client::Pointer::left);

    QSignalSpy motionSpy(p, &KWayland::Client::Pointer::motion);

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), enterTransformation);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QTEST(enteredSpy.first().last().toPointF(), "expectedEnterPoint");
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // test motion
    m_seatInterface->setTimestamp(std::chrono::milliseconds(1));
    m_seatInterface->notifyPointerMotion(QPoint(10, 16));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(motionSpy.wait());
    QTEST(motionSpy.first().first().toPointF(), "expectedMovePoint");
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // leave the surface
    m_seatInterface->notifyPointerLeave();
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // enter it again
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(10, 16));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    delete s;
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    QVERIFY(!m_seatInterface->focusedPointerSurface());
}

Q_DECLARE_METATYPE(Qt::MouseButton)

void TestWaylandSeat::testPointerButton_data()
{
    QTest::addColumn<Qt::MouseButton>("qtButton");
    QTest::addColumn<quint32>("waylandButton");

    QTest::newRow("left") << Qt::LeftButton << quint32(BTN_LEFT);
    QTest::newRow("right") << Qt::RightButton << quint32(BTN_RIGHT);
    QTest::newRow("middle") << Qt::MiddleButton << quint32(BTN_MIDDLE);
    QTest::newRow("back") << Qt::BackButton << quint32(BTN_BACK);
    QTest::newRow("x1") << Qt::XButton1 << quint32(BTN_BACK);
    QTest::newRow("extra1") << Qt::ExtraButton1 << quint32(BTN_BACK);
    QTest::newRow("forward") << Qt::ForwardButton << quint32(BTN_FORWARD);
    QTest::newRow("x2") << Qt::XButton2 << quint32(BTN_FORWARD);
    QTest::newRow("extra2") << Qt::ExtraButton2 << quint32(BTN_FORWARD);
    QTest::newRow("task") << Qt::TaskButton << quint32(BTN_TASK);
    QTest::newRow("extra3") << Qt::ExtraButton3 << quint32(BTN_TASK);
    QTest::newRow("extra4") << Qt::ExtraButton4 << quint32(BTN_EXTRA);
    QTest::newRow("extra5") << Qt::ExtraButton5 << quint32(BTN_SIDE);
    QTest::newRow("extra6") << Qt::ExtraButton6 << quint32(0x118);
    QTest::newRow("extra7") << Qt::ExtraButton7 << quint32(0x119);
    QTest::newRow("extra8") << Qt::ExtraButton8 << quint32(0x11a);
    QTest::newRow("extra9") << Qt::ExtraButton9 << quint32(0x11b);
    QTest::newRow("extra10") << Qt::ExtraButton10 << quint32(0x11c);
    QTest::newRow("extra11") << Qt::ExtraButton11 << quint32(0x11d);
    QTest::newRow("extra12") << Qt::ExtraButton12 << quint32(0x11e);
    QTest::newRow("extra13") << Qt::ExtraButton13 << quint32(0x11f);
}

void TestWaylandSeat::testPointerButton()
{
    using namespace KWaylandServer;

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    s->attachBuffer(m_shm->createBuffer(image));
    s->damage(image.rect());
    s->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    std::unique_ptr<KWayland::Client::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy buttonChangedSpy(p.get(), &KWayland::Client::Pointer::buttonStateChanged);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), QPointF(10, 15));
    QVERIFY(m_seatInterface->focusedPointerSurface());

    QCoreApplication::processEvents();

    QFETCH(Qt::MouseButton, qtButton);
    QFETCH(quint32, waylandButton);
    std::chrono::milliseconds timestamp(1);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), false);
    m_seatInterface->setTimestamp(timestamp);
    m_seatInterface->notifyPointerButton(qtButton, PointerButtonState::Pressed);
    m_seatInterface->notifyPointerFrame();
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), true);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), true);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), timestamp.count());
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Pressed);
    timestamp++;
    m_seatInterface->setTimestamp(timestamp);
    m_seatInterface->notifyPointerButton(qtButton, PointerButtonState::Released);
    m_seatInterface->notifyPointerFrame();
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), false);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 2);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), timestamp.count());
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);
}

void TestWaylandSeat::testPointerSubSurfaceTree()
{
    // this test verifies that pointer motion on a surface with sub-surfaces sends motion enter/leave to the sub-surface
    using namespace KWaylandServer;

    // first create the pointer
    QSignalSpy hasPointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());

    // create a sub surface tree
    // parent surface (100, 100) with one sub surface taking the half of it's size (50, 100)
    // which has two further children (50, 50) which are overlapping
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> parentSurface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> childSurface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> grandChild1Surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> grandChild2Surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::SubSurface> childSubSurface(m_subCompositor->createSubSurface(childSurface.get(), parentSurface.get()));
    std::unique_ptr<KWayland::Client::SubSurface> grandChild1SubSurface(m_subCompositor->createSubSurface(grandChild1Surface.get(), childSurface.get()));
    std::unique_ptr<KWayland::Client::SubSurface> grandChild2SubSurface(m_subCompositor->createSubSurface(grandChild2Surface.get(), childSurface.get()));
    grandChild2SubSurface->setPosition(QPoint(0, 25));

    // let's map the surfaces
    auto render = [this](KWayland::Client::Surface *s, const QSize &size) {
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        s->attachBuffer(m_shm->createBuffer(image));
        s->damage(QRect(QPoint(0, 0), size));
        s->commit(KWayland::Client::Surface::CommitFlag::None);
    };
    render(grandChild2Surface.get(), QSize(50, 50));
    render(grandChild1Surface.get(), QSize(50, 50));
    render(childSurface.get(), QSize(50, 100));
    render(parentSurface.get(), QSize(100, 100));

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface->isMapped());

    // send in pointer events
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy motionSpy(pointer.get(), &KWayland::Client::Pointer::motion);
    // first to the grandChild2 in the overlapped area
    std::chrono::milliseconds timestamp(1);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(25, 50));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 0);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 25));
    QCOMPARE(pointer->enteredSurface(), grandChild2Surface.get());
    // a motion on grandchild2
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(25, 60));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(motionSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(25, 35));
    // motion which changes to childSurface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerMotion(QPointF(25, 80));
    m_seatInterface->notifyPointerFrame();
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(motionSpy.count(), 2);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 80));
    QCOMPARE(pointer->enteredSurface(), childSurface.get());
    // a leave for the whole surface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerLeave();
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 2);
    // a new enter on the main surface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(75, 50));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 2);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(75, 50));
    QCOMPARE(pointer->enteredSurface(), parentSurface.get());
}

void TestWaylandSeat::testPointerSwipeGesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void TestWaylandSeat::testPointerSwipeGesture()
{
    using namespace KWaylandServer;

    // first create the pointer and pointer swipe gesture
    QSignalSpy hasPointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    std::unique_ptr<KWayland::Client::PointerSwipeGesture> gesture(m_pointerGestures->createSwipeGesture(pointer.get()));
    QVERIFY(gesture);
    QVERIFY(gesture->isValid());
    QVERIFY(gesture->surface().isNull());
    QCOMPARE(gesture->fingerCount(), 0u);

    QSignalSpy startSpy(gesture.get(), &KWayland::Client::PointerSwipeGesture::started);
    QSignalSpy updateSpy(gesture.get(), &KWayland::Client::PointerSwipeGesture::updated);
    QSignalSpy endSpy(gesture.get(), &KWayland::Client::PointerSwipeGesture::ended);
    QSignalSpy cancelledSpy(gesture.get(), &KWayland::Client::PointerSwipeGesture::cancelled);

    // now create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(m_seatInterface->pointer());

    // send in the start
    std::chrono::milliseconds timestamp(1);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerSwipeGesture(2);
    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 2u);
    QCOMPARE(gesture->surface().data(), surface.get());

    // another start should not be possible
    m_seatInterface->startPointerSwipeGesture(2);
    QVERIFY(!startSpy.wait(500));

    // send in some updates
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerSwipeGesture(QPointF(2, 3));
    QVERIFY(updateSpy.wait());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerSwipeGesture(QPointF(4, 5));
    QVERIFY(updateSpy.wait());
    QCOMPARE(updateSpy.count(), 2);
    QCOMPARE(updateSpy.at(0).at(0).toSizeF(), QSizeF(2, 3));
    QCOMPARE(updateSpy.at(0).at(1).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(1).at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(updateSpy.at(1).at(1).value<quint32>(), 3u);

    // now end or cancel
    QFETCH(bool, cancel);
    QSignalSpy *spy;
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerSwipeGesture();
        spy = &cancelledSpy;
    } else {
        m_seatInterface->endPointerSwipeGesture();
        spy = &endSpy;
    }
    QVERIFY(spy->wait());
    QFETCH(int, expectedEndCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);
    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // now a start should be possible again
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerSwipeGesture(2);
    QVERIFY(startSpy.wait());

    // unsetting the focused pointer surface should not change anything
    m_seatInterface->notifyPointerLeave();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerSwipeGesture(QPointF(6, 7));
    QVERIFY(updateSpy.wait());
    // and end
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerSwipeGesture();
    } else {
        m_seatInterface->endPointerSwipeGesture();
    }
    QVERIFY(spy->wait());
}

void TestWaylandSeat::testPointerPinchGesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

void TestWaylandSeat::testPointerPinchGesture()
{
    using namespace KWaylandServer;

    // first create the pointer and pointer swipe gesture
    QSignalSpy hasPointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    std::unique_ptr<KWayland::Client::PointerPinchGesture> gesture(m_pointerGestures->createPinchGesture(pointer.get()));
    QVERIFY(gesture);
    QVERIFY(gesture->isValid());
    QVERIFY(gesture->surface().isNull());
    QCOMPARE(gesture->fingerCount(), 0u);

    QSignalSpy startSpy(gesture.get(), &KWayland::Client::PointerPinchGesture::started);
    QSignalSpy updateSpy(gesture.get(), &KWayland::Client::PointerPinchGesture::updated);
    QSignalSpy endSpy(gesture.get(), &KWayland::Client::PointerPinchGesture::ended);
    QSignalSpy cancelledSpy(gesture.get(), &KWayland::Client::PointerPinchGesture::cancelled);

    // now create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(m_seatInterface->pointer());

    // send in the start
    std::chrono::milliseconds timestamp(1);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerPinchGesture(3);
    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(gesture->fingerCount(), 3u);
    QCOMPARE(gesture->surface().data(), surface.get());

    // another start should not be possible
    m_seatInterface->startPointerPinchGesture(3);
    QVERIFY(!startSpy.wait(500));

    // send in some updates
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerPinchGesture(QPointF(2, 3), 2, 45);
    QVERIFY(updateSpy.wait());
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerPinchGesture(QPointF(4, 5), 1, 90);
    QVERIFY(updateSpy.wait());
    QCOMPARE(updateSpy.count(), 2);
    QCOMPARE(updateSpy.at(0).at(0).toSizeF(), QSizeF(2, 3));
    QCOMPARE(updateSpy.at(0).at(1).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(0).at(2).value<quint32>(), 45u);
    QCOMPARE(updateSpy.at(0).at(3).value<quint32>(), 2u);
    QCOMPARE(updateSpy.at(1).at(0).toSizeF(), QSizeF(4, 5));
    QCOMPARE(updateSpy.at(1).at(1).value<quint32>(), 1u);
    QCOMPARE(updateSpy.at(1).at(2).value<quint32>(), 90u);
    QCOMPARE(updateSpy.at(1).at(3).value<quint32>(), 3u);

    // now end or cancel
    QFETCH(bool, cancel);
    QSignalSpy *spy;
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerPinchGesture();
        spy = &cancelledSpy;
    } else {
        m_seatInterface->endPointerPinchGesture();
        spy = &endSpy;
    }
    QVERIFY(spy->wait());
    QFETCH(int, expectedEndCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);
    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 4u);

    QCOMPARE(gesture->fingerCount(), 0u);
    QVERIFY(gesture->surface().isNull());

    // now a start should be possible again
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerPinchGesture(3);
    QVERIFY(startSpy.wait());

    // unsetting the focused pointer surface should not change anything
    m_seatInterface->notifyPointerLeave();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->updatePointerPinchGesture(QPointF(6, 7), 2, -45);
    QVERIFY(updateSpy.wait());
    // and end
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerPinchGesture();
    } else {
        m_seatInterface->endPointerPinchGesture();
    }
    QVERIFY(spy->wait());
}

void TestWaylandSeat::testPointerHoldGesture_data()
{
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<int>("expectedEndCount");
    QTest::addColumn<int>("expectedCancelCount");

    QTest::newRow("end") << false << 1 << 0;
    QTest::newRow("cancel") << true << 0 << 1;
}

class PointerHoldGesture : public QObject, public QtWayland::zwp_pointer_gesture_hold_v1
{
    using zwp_pointer_gesture_hold_v1::zwp_pointer_gesture_hold_v1;
    Q_OBJECT
    void zwp_pointer_gesture_hold_v1_begin(uint32_t serial, uint32_t time, wl_surface *surface, uint32_t fingers) override
    {
        Q_EMIT started(serial, time, surface, fingers);
    }

    void zwp_pointer_gesture_hold_v1_end(uint32_t serial, uint32_t time, int32_t cancelled) override
    {
        cancelled ? Q_EMIT this->cancelled(serial, time) : Q_EMIT ended(serial, time);
    }
Q_SIGNALS:
    void started(quint32 serial, quint32 time, void *surface, quint32 fingers);
    void ended(quint32 serial, quint32 time);
    void cancelled(quint32 serial, quint32 time);
};

void TestWaylandSeat::testPointerHoldGesture()
{
    using namespace KWaylandServer;

    // first create the pointer and pointer swipe gesture
    QSignalSpy hasPointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    KWayland::Client::Registry registry;
    QSignalSpy gesturesAnnoucedSpy(&registry, &KWayland::Client::Registry::pointerGesturesUnstableV1Announced);
    registry.create(m_connection);
    registry.setup();
    QVERIFY(gesturesAnnoucedSpy.wait());
    QtWayland::zwp_pointer_gestures_v1 gestures(registry, gesturesAnnoucedSpy.first().at(0).value<int>(), gesturesAnnoucedSpy.first().at(1).value<int>());
    PointerHoldGesture gesture(gestures.get_hold_gesture(*pointer));
    QVERIFY(gesture.isInitialized());

    QSignalSpy startSpy(&gesture, &PointerHoldGesture::started);
    QSignalSpy endSpy(&gesture, &PointerHoldGesture::ended);
    QSignalSpy cancelledSpy(&gesture, &PointerHoldGesture::cancelled);

    // now create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(m_seatInterface->pointer());

    // send in the start
    std::chrono::milliseconds timestamp(1);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerHoldGesture(3);
    QVERIFY(startSpy.wait());
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(startSpy.first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(startSpy.first().at(1).value<quint32>(), 1u);
    QCOMPARE(startSpy.first().at(2).value<void *>(), *surface.get());
    QCOMPARE(startSpy.first().at(3).value<quint32>(), 3);

    // another start should not be possible
    m_seatInterface->startPointerPinchGesture(3);
    QVERIFY(!startSpy.wait(500));

    // now end or cancel
    QFETCH(bool, cancel);
    QSignalSpy *spy;
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerHoldGesture();
        spy = &cancelledSpy;
    } else {
        m_seatInterface->endPointerHoldGesture();
        spy = &endSpy;
    }
    QVERIFY(spy->wait());
    QFETCH(int, expectedEndCount);
    QCOMPARE(endSpy.count(), expectedEndCount);
    QFETCH(int, expectedCancelCount);
    QCOMPARE(cancelledSpy.count(), expectedCancelCount);
    QCOMPARE(spy->count(), 1);
    QCOMPARE(spy->first().at(0).value<quint32>(), m_display->serial());
    QCOMPARE(spy->first().at(1).value<quint32>(), 2);

    // now a start should be possible again
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->startPointerHoldGesture(3);
    QVERIFY(startSpy.wait());

    // and end
    m_seatInterface->setTimestamp(timestamp++);
    if (cancel) {
        m_seatInterface->cancelPointerHoldGesture();
    } else {
        m_seatInterface->endPointerHoldGesture();
    }
    QVERIFY(spy->wait());
}

void TestWaylandSeat::testPointerAxis()
{
    using namespace KWaylandServer;

    // first create the pointer
    QSignalSpy hasPointerChangedSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer());
    QVERIFY(pointer);

    // now create a surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QSignalSpy frameSpy(pointer.get(), &KWayland::Client::Pointer::frame);
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 1);

    // let's scroll vertically
    QSignalSpy axisSourceSpy(pointer.get(), &KWayland::Client::Pointer::axisSourceChanged);
    QSignalSpy axisSpy(pointer.get(), &KWayland::Client::Pointer::axisChanged);
    QSignalSpy axisDiscreteSpy(pointer.get(), &KWayland::Client::Pointer::axisDiscreteChanged);
    QSignalSpy axisStoppedSpy(pointer.get(), &KWayland::Client::Pointer::axisStopped);

    std::chrono::milliseconds timestamp(1);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Vertical, 10, 120, PointerAxisSource::Wheel);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 2);
    QCOMPARE(axisSourceSpy.count(), 1);
    QCOMPARE(axisSourceSpy.last().at(0).value<KWayland::Client::Pointer::AxisSource>(), KWayland::Client::Pointer::AxisSource::Wheel);
    QCOMPARE(axisDiscreteSpy.count(), 1);
    QCOMPARE(axisDiscreteSpy.last().at(0).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Vertical);
    QCOMPARE(axisDiscreteSpy.last().at(1).value<qint32>(), 1);
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(axisSpy.last().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 10.0);
    QCOMPARE(axisStoppedSpy.count(), 0);

    // let's scroll using fingers
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Horizontal, 42, 0, PointerAxisSource::Finger);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 3);
    QCOMPARE(axisSourceSpy.count(), 2);
    QCOMPARE(axisSourceSpy.last().at(0).value<KWayland::Client::Pointer::AxisSource>(), KWayland::Client::Pointer::AxisSource::Finger);
    QCOMPARE(axisDiscreteSpy.count(), 1);
    QCOMPARE(axisSpy.count(), 2);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.last().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 42.0);
    QCOMPARE(axisStoppedSpy.count(), 0);

    // lift the fingers off the device
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Horizontal, 0, 0, PointerAxisSource::Finger);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 4);
    QCOMPARE(axisSourceSpy.count(), 3);
    QCOMPARE(axisSourceSpy.last().at(0).value<KWayland::Client::Pointer::AxisSource>(), KWayland::Client::Pointer::AxisSource::Finger);
    QCOMPARE(axisDiscreteSpy.count(), 1);
    QCOMPARE(axisSpy.count(), 2);
    QCOMPARE(axisStoppedSpy.count(), 1);
    QCOMPARE(axisStoppedSpy.last().at(0).value<quint32>(), 3);
    QCOMPARE(axisStoppedSpy.last().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Horizontal);

    // if the device is unknown, no axis_source event should be sent
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyPointerAxis(Qt::Horizontal, 42, 120, PointerAxisSource::Unknown);
    m_seatInterface->notifyPointerFrame();
    QVERIFY(frameSpy.wait());
    QCOMPARE(frameSpy.count(), 5);
    QCOMPARE(axisSourceSpy.count(), 3);
    QCOMPARE(axisDiscreteSpy.count(), 2);
    QCOMPARE(axisDiscreteSpy.last().at(0).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Horizontal);
    QCOMPARE(axisDiscreteSpy.last().at(1).value<qint32>(), 1);
    QCOMPARE(axisSpy.count(), 3);
    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(4));
    QCOMPARE(axisSpy.last().at(1).value<KWayland::Client::Pointer::Axis>(), KWayland::Client::Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), 42.0);
    QCOMPARE(axisStoppedSpy.count(), 1);
}

void TestWaylandSeat::testCursor()
{
    using namespace KWaylandServer;

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    std::unique_ptr<KWayland::Client::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QSignalSpy enteredSpy(p.get(), &KWayland::Client::Pointer::entered);

    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(20, 18), QPointF(10, 15));
    quint32 serial = m_seatInterface->display()->serial();
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), serial);
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(!m_seatInterface->pointer()->cursor());

    QSignalSpy cursorChangedSpy(m_seatInterface->pointer(), &KWaylandServer::PointerInterface::cursorChanged);
    // just remove the pointer
    p->setCursor(nullptr);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(cursorChangedSpy.count(), 1);
    auto cursor = m_seatInterface->pointer()->cursor();
    QVERIFY(cursor);
    QVERIFY(!cursor->surface());
    QCOMPARE(cursor->hotspot(), QPoint());
    QCOMPARE(cursor->enteredSerial(), serial);
    QCOMPARE(cursor->pointer(), m_seatInterface->pointer());

    QSignalSpy hotspotChangedSpy(cursor, &KWaylandServer::Cursor::hotspotChanged);
    QSignalSpy surfaceChangedSpy(cursor, &KWaylandServer::Cursor::surfaceChanged);
    QSignalSpy enteredSerialChangedSpy(cursor, &KWaylandServer::Cursor::enteredSerialChanged);
    QSignalSpy changedSpy(cursor, &KWaylandServer::Cursor::changed);

    // test changing hotspot
    p->setCursor(nullptr, QPoint(1, 2));
    QVERIFY(hotspotChangedSpy.wait());
    QCOMPARE(hotspotChangedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(cursorChangedSpy.count(), 2);
    QCOMPARE(cursor->hotspot(), QPoint(1, 2));
    QVERIFY(enteredSerialChangedSpy.isEmpty());
    QVERIFY(surfaceChangedSpy.isEmpty());

    // set surface
    auto cursorSurface = m_compositor->createSurface(m_compositor);
    QVERIFY(cursorSurface->isValid());
    p->setCursor(cursorSurface, QPoint(1, 2));
    QVERIFY(surfaceChangedSpy.wait());
    QCOMPARE(surfaceChangedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(cursorChangedSpy.count(), 3);
    QVERIFY(enteredSerialChangedSpy.isEmpty());
    QCOMPARE(cursor->hotspot(), QPoint(1, 2));
    QVERIFY(cursor->surface());

    // and add an image to the surface
    QImage img(QSize(10, 20), QImage::Format_RGB32);
    img.fill(Qt::red);
    cursorSurface->attachBuffer(m_shm->createBuffer(img));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 3);
    QCOMPARE(cursorChangedSpy.count(), 4);
    QCOMPARE(surfaceChangedSpy.count(), 1);
    QCOMPARE(qobject_cast<ShmClientBuffer *>(cursor->surface()->buffer())->data(), img);

    // and add another image to the surface
    QImage blue(QSize(10, 20), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 4);
    QCOMPARE(cursorChangedSpy.count(), 5);
    QCOMPARE(qobject_cast<ShmClientBuffer *>(cursor->surface()->buffer())->data(), blue);

    p->hideCursor();
    QVERIFY(surfaceChangedSpy.wait());
    QCOMPARE(changedSpy.count(), 5);
    QCOMPARE(cursorChangedSpy.count(), 6);
    QCOMPARE(surfaceChangedSpy.count(), 2);
    QVERIFY(!cursor->surface());
}

void TestWaylandSeat::testCursorDamage()
{
    // this test verifies that damaging a cursor surface triggers a cursor changed on the server
    using namespace KWaylandServer;

    QSignalSpy pointerSpy(m_seat, &KWayland::Client::Seat::hasPointerChanged);
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    // create pointer
    std::unique_ptr<KWayland::Client::Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy enteredSpy(p.get(), &KWayland::Client::Pointer::entered);
    // create surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *surface = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(image.rect());
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy committedSpy(serverSurface, &KWaylandServer::SurfaceInterface::committed);
    QVERIFY(committedSpy.wait());

    // send enter to the surface
    m_seatInterface->notifyPointerEnter(serverSurface, QPointF(0, 0));
    QVERIFY(enteredSpy.wait());

    // create a signal spy for the cursor changed signal
    auto pointer = m_seatInterface->pointer();
    QSignalSpy cursorChangedSpy(pointer, &PointerInterface::cursorChanged);

    // now let's set the cursor
    KWayland::Client::Surface *cursorSurface = m_compositor->createSurface(m_compositor);
    QVERIFY(cursorSurface);
    QImage red(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);
    cursorSurface->attachBuffer(m_shm->createBuffer(red));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    p->setCursor(cursorSurface, QPoint(0, 0));
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(qobject_cast<ShmClientBuffer *>(pointer->cursor()->surface()->buffer())->data(), red);

    // and damage the surface
    QImage blue(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(qobject_cast<ShmClientBuffer *>(pointer->cursor()->surface()->buffer())->data(), blue);
}

void TestWaylandSeat::testKeyboard()
{
    using namespace KWaylandServer;

    QSignalSpy keyboardSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());

    // update modifiers before any surface focused
    m_seatInterface->notifyKeyboardModifiers(4, 3, 2, 1);

    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    KWayland::Client::Keyboard *keyboard = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy(keyboard, &KWayland::Client::Keyboard::keyRepeatChanged);
    const KWayland::Client::Keyboard &ckeyboard = *keyboard;
    QVERIFY(keyboard->isValid());
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);
    QVERIFY(repeatInfoSpy.wait());
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    auto serverKeyboard = m_seatInterface->keyboard();
    QVERIFY(serverKeyboard);

    // we should get the repeat info announced
    QCOMPARE(repeatInfoSpy.count(), 1);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);

    // let's change repeat in server
    m_seatInterface->keyboard()->setRepeatInfo(25, 660);
    QVERIFY(repeatInfoSpy.wait());
    QCOMPARE(repeatInfoSpy.count(), 2);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard->keyRepeatRate(), 25);
    QCOMPARE(keyboard->keyRepeatDelay(), 660);

    std::chrono::milliseconds time(1);

    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_K, KeyboardKeyState::Pressed);
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_D, KeyboardKeyState::Pressed);
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_E, KeyboardKeyState::Pressed);

    QSignalSpy modifierSpy(keyboard, &KWayland::Client::Keyboard::modifiersChanged);

    QSignalSpy enteredSpy(keyboard, &KWayland::Client::Keyboard::entered);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->keyboard()->focusedSurface(), serverSurface);

    // we get the modifiers sent after the enter
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.first().at(0).value<quint32>(), quint32(4));
    QCOMPARE(modifierSpy.first().at(1).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.first().at(2).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.first().at(3).value<quint32>(), quint32(1));
    QCOMPARE(enteredSpy.count(), 1);
    // TODO: get through API
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial() - 1);

    QSignalSpy keyChangedSpy(keyboard, &KWayland::Client::Keyboard::keyChanged);

    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_E, KeyboardKeyState::Released);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_D, KeyboardKeyState::Released);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_K, KeyboardKeyState::Released);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Pressed);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(time++);
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Released);
    QVERIFY(keyChangedSpy.wait());

    QCOMPARE(keyChangedSpy.count(), 5);
    QCOMPARE(keyChangedSpy.at(0).at(0).value<quint32>(), quint32(KEY_E));
    QCOMPARE(keyChangedSpy.at(0).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(0).at(2).value<quint32>(), quint32(4));
    QCOMPARE(keyChangedSpy.at(1).at(0).value<quint32>(), quint32(KEY_D));
    QCOMPARE(keyChangedSpy.at(1).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(1).at(2).value<quint32>(), quint32(5));
    QCOMPARE(keyChangedSpy.at(2).at(0).value<quint32>(), quint32(KEY_K));
    QCOMPARE(keyChangedSpy.at(2).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(2).at(2).value<quint32>(), quint32(6));
    QCOMPARE(keyChangedSpy.at(3).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(3).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(3).at(2).value<quint32>(), quint32(7));
    QCOMPARE(keyChangedSpy.at(4).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(4).at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(4).at(2).value<quint32>(), quint32(8));

    // releasing a key which is already released should not set a key changed
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Released);
    QVERIFY(!keyChangedSpy.wait(200));
    // let's press it again
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Pressed);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 6);
    // press again should be ignored
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Pressed);
    QVERIFY(!keyChangedSpy.wait(200));
    // and release
    m_seatInterface->notifyKeyboardKey(KEY_F1, KeyboardKeyState::Released);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 7);

    m_seatInterface->notifyKeyboardModifiers(1, 2, 3, 4);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(modifierSpy.last().at(1).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.last().at(2).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.last().at(3).value<quint32>(), quint32(4));

    QSignalSpy leftSpy(keyboard, &KWayland::Client::Keyboard::left);
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!m_seatInterface->focusedKeyboardSurface());
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // TODO: get through API
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial() - 1);

    QVERIFY(!keyboard->enteredSurface());
    QVERIFY(!ckeyboard.enteredSurface());

    // enter it again
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->keyboard()->focusedSurface(), serverSurface);
    QCOMPARE(enteredSpy.count(), 2);

    QCOMPARE(keyboard->enteredSurface(), s);
    QCOMPARE(ckeyboard.enteredSurface(), s);

    QSignalSpy serverSurfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QCOMPARE(keyboard->enteredSurface(), s);
    delete s;
    QVERIFY(!keyboard->enteredSurface());
    QVERIFY(leftSpy.wait());
    QCOMPARE(serverSurfaceDestroyedSpy.count(), 1);
    QVERIFY(!m_seatInterface->focusedKeyboardSurface());
    QVERIFY(!serverKeyboard->focusedSurface());

    // let's create a Surface again
    std::unique_ptr<KWayland::Client::Surface> s2(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    serverSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->keyboard(), serverKeyboard);
}

void TestWaylandSeat::testSelection()
{
    using namespace KWaylandServer;
    std::unique_ptr<DataDeviceManagerInterface> ddmi(new DataDeviceManagerInterface(m_display));
    KWayland::Client::Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry, &KWayland::Client::Registry::dataDeviceManagerAnnounced);
    m_seatInterface->setHasKeyboard(true);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    std::unique_ptr<KWayland::Client::DataDeviceManager> ddm(
        registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(), dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    std::unique_ptr<KWayland::Client::DataDevice> dd1(ddm->getDataDevice(m_seat));
    QVERIFY(dd1->isValid());
    QSignalSpy selectionSpy(dd1.get(), &KWayland::Client::DataDevice::selectionOffered);
    QSignalSpy selectionClearedSpy(dd1.get(), &KWayland::Client::DataDevice::selectionCleared);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QVERIFY(selectionClearedSpy.wait());
    QVERIFY(selectionSpy.isEmpty());
    QVERIFY(!selectionClearedSpy.isEmpty());
    selectionClearedSpy.clear();
    QVERIFY(!m_seatInterface->selection());

    // now let's try to set a selection - we have keyboard focus, so it should be sent to us
    std::unique_ptr<KWayland::Client::DataSource> ds(ddm->createDataSource());
    QVERIFY(ds->isValid());
    ds->offer(QStringLiteral("text/plain"));
    dd1->setSelection(0, ds.get());
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 1);
    auto ddi = m_seatInterface->selection();
    QVERIFY(ddi);
    auto df = selectionSpy.first().first().value<KWayland::Client::DataOffer *>();
    QCOMPARE(df->offeredMimeTypes().count(), 1);
    QCOMPARE(df->offeredMimeTypes().first().name(), QStringLiteral("text/plain"));

    // try to clear
    dd1->setSelection(0);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionClearedSpy.count(), 1);
    QCOMPARE(selectionSpy.count(), 1);

    // unset the keyboard focus
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!m_seatInterface->focusedKeyboardSurface());
    serverSurface->client()->flush();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // try to set Selection
    dd1->setSelection(0, ds.get());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(selectionSpy.count(), 1);

    // let's unset the selection on the seat
    m_seatInterface->setSelection(nullptr);
    // and pass focus back on our surface
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    // we don't have a selection, so it should not send a selection
    QVERIFY(!selectionSpy.wait(100));
    // now let's set it manually
    m_seatInterface->setSelection(ddi);
    QCOMPARE(m_seatInterface->selection(), ddi);
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);
    // setting the same again should not change
    m_seatInterface->setSelection(ddi);
    QVERIFY(!selectionSpy.wait(100));
    // now clear it manually
    m_seatInterface->setSelection(nullptr);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);

    // create a second ddi and a data source
    std::unique_ptr<KWayland::Client::DataDevice> dd2(ddm->getDataDevice(m_seat));
    QVERIFY(dd2->isValid());
    std::unique_ptr<KWayland::Client::DataSource> ds2(ddm->createDataSource());
    QVERIFY(ds2->isValid());
    ds2->offer(QStringLiteral("text/plain"));
    dd2->setSelection(0, ds2.get());
    QVERIFY(selectionSpy.wait());
    QSignalSpy cancelledSpy(ds2.get(), &KWayland::Client::DataSource::cancelled);
    m_seatInterface->setSelection(ddi);
    QVERIFY(cancelledSpy.wait());
}

void TestWaylandSeat::testDataDeviceForKeyboardSurface()
{
    // this test verifies that the server does not crash when creating a datadevice for the focused keyboard surface
    // and the currentSelection does not have a DataSource.
    // to properly test the functionality this test requires a second client
    using namespace KWaylandServer;
    // create the DataDeviceManager
    std::unique_ptr<DataDeviceManagerInterface> ddmi(new DataDeviceManagerInterface(m_display));
    QSignalSpy ddiCreatedSpy(ddmi.get(), &DataDeviceManagerInterface::dataDeviceCreated);
    m_seatInterface->setHasKeyboard(true);

    // create a second Wayland client connection to use it for setSelection
    auto c = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(c, &KWayland::Client::ConnectionThread::connected);
    c->setSocketName(s_socketName);

    auto thread = new QThread(this);
    c->moveToThread(thread);
    thread->start();

    c->initConnection();
    QVERIFY(connectedSpy.wait());

    std::unique_ptr<KWayland::Client::EventQueue> queue(new KWayland::Client::EventQueue);
    queue->setup(c);

    std::unique_ptr<KWayland::Client::Registry> registry(new KWayland::Client::Registry);
    QSignalSpy interfacesAnnouncedSpy(registry.get(), &KWayland::Client::Registry::interfacesAnnounced);
    registry->setEventQueue(queue.get());
    registry->create(c);
    QVERIFY(registry->isValid());
    registry->setup();

    QVERIFY(interfacesAnnouncedSpy.wait());
    std::unique_ptr<KWayland::Client::Seat> seat(
        registry->createSeat(registry->interface(KWayland::Client::Registry::Interface::Seat).name, registry->interface(KWayland::Client::Registry::Interface::Seat).version));
    QVERIFY(seat->isValid());
    std::unique_ptr<KWayland::Client::DataDeviceManager> ddm1(registry->createDataDeviceManager(registry->interface(KWayland::Client::Registry::Interface::DataDeviceManager).name,
                                                                                                registry->interface(KWayland::Client::Registry::Interface::DataDeviceManager).version));
    QVERIFY(ddm1->isValid());

    // now create our first datadevice
    std::unique_ptr<KWayland::Client::DataDevice> dd1(ddm1->getDataDevice(seat.get()));
    QVERIFY(ddiCreatedSpy.wait());
    auto ddi = ddiCreatedSpy.first().first().value<DataDeviceInterface *>();
    QVERIFY(ddi);
    m_seatInterface->setSelection(ddi->selection());

    // switch to other client
    // create a surface and pass it keyboard focus
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);

    // now create a DataDevice
    KWayland::Client::Registry registry2;
    QSignalSpy dataDeviceManagerSpy(&registry2, &KWayland::Client::Registry::dataDeviceManagerAnnounced);
    registry2.setEventQueue(m_queue);
    registry2.create(m_connection->display());
    QVERIFY(registry2.isValid());
    registry2.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    std::unique_ptr<KWayland::Client::DataDeviceManager> ddm(
        registry2.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(), dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    std::unique_ptr<KWayland::Client::DataDevice> dd(ddm->getDataDevice(m_seat));
    QVERIFY(dd->isValid());
    QVERIFY(ddiCreatedSpy.wait());

    // unset surface and set again
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    // and delete the connection thread again
    dd1.reset();
    ddm1.reset();
    seat.reset();
    registry.reset();
    queue.reset();
    c->deleteLater();
    thread->quit();
    thread->wait();
    delete thread;
}

void TestWaylandSeat::testTouch()
{
    using namespace KWaylandServer;

    QSignalSpy touchSpy(m_seat, &KWayland::Client::Seat::hasTouchChanged);
    m_seatInterface->setHasTouch(true);
    QVERIFY(touchSpy.wait());

    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    KWayland::Client::Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_seatInterface->setFocusedTouchSurface(serverSurface);
    // no keyboard yet
    QCOMPARE(m_seatInterface->focusedTouchSurface(), serverSurface);

    KWayland::Client::Touch *touch = m_seat->createTouch(m_seat);
    QVERIFY(touch->isValid());

    // Process wl_touch bind request.
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QSignalSpy sequenceStartedSpy(touch, &KWayland::Client::Touch::sequenceStarted);
    QSignalSpy sequenceEndedSpy(touch, &KWayland::Client::Touch::sequenceEnded);
    QSignalSpy sequenceCanceledSpy(touch, &KWayland::Client::Touch::sequenceCanceled);
    QSignalSpy frameEndedSpy(touch, &KWayland::Client::Touch::frameEnded);
    QSignalSpy pointAddedSpy(touch, &KWayland::Client::Touch::pointAdded);
    QSignalSpy pointMovedSpy(touch, &KWayland::Client::Touch::pointMoved);
    QSignalSpy pointRemovedSpy(touch, &KWayland::Client::Touch::pointRemoved);

    std::chrono::milliseconds timestamp(1);

    // try a few things
    m_seatInterface->setFocusedTouchSurfacePosition(QPointF(10, 20));
    QCOMPARE(m_seatInterface->focusedTouchSurfacePosition(), QPointF(10, 20));
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchDown(0, QPointF(15, 26));
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 0);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);
    QCOMPARE(pointRemovedSpy.count(), 0);
    KWayland::Client::TouchPoint *tp = sequenceStartedSpy.first().first().value<KWayland::Client::TouchPoint *>();
    QVERIFY(tp);
    QCOMPARE(tp->downSerial(), m_seatInterface->display()->serial());
    QCOMPARE(tp->id(), 0);
    QVERIFY(tp->isDown());
    QCOMPARE(tp->position(), QPointF(5, 6));
    QCOMPARE(tp->positions().size(), 1);
    QCOMPARE(tp->time(), 1u);
    QCOMPARE(tp->timestamps().count(), 1);
    QCOMPARE(tp->upSerial(), 0u);
    QCOMPARE(tp->surface().data(), s);
    QCOMPARE(touch->sequence().count(), 1);
    QCOMPARE(touch->sequence().first(), tp);

    // let's end the frame
    m_seatInterface->notifyTouchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(frameEndedSpy.count(), 1);

    // move the one point
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchMotion(0, QPointF(10, 20));
    m_seatInterface->notifyTouchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 2);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.first().first().value<KWayland::Client::TouchPoint *>(), tp);

    QCOMPARE(tp->id(), 0);
    QVERIFY(tp->isDown());
    QCOMPARE(tp->position(), QPointF(0, 0));
    QCOMPARE(tp->positions().size(), 2);
    QCOMPARE(tp->time(), 2u);
    QCOMPARE(tp->timestamps().count(), 2);
    QCOMPARE(tp->upSerial(), 0u);
    QCOMPARE(tp->surface().data(), s);

    // add onther point
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchDown(1, QPointF(15, 26));
    m_seatInterface->notifyTouchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 3);
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 0);
    QCOMPARE(touch->sequence().count(), 2);
    QCOMPARE(touch->sequence().first(), tp);
    KWayland::Client::TouchPoint *tp2 = pointAddedSpy.first().first().value<KWayland::Client::TouchPoint *>();
    QVERIFY(tp2);
    QCOMPARE(touch->sequence().last(), tp2);
    QCOMPARE(tp2->id(), 1);
    QVERIFY(tp2->isDown());
    QCOMPARE(tp2->position(), QPointF(5, 6));
    QCOMPARE(tp2->positions().size(), 1);
    QCOMPARE(tp2->time(), 3u);
    QCOMPARE(tp2->timestamps().count(), 1);
    QCOMPARE(tp2->upSerial(), 0u);
    QCOMPARE(tp2->surface().data(), s);

    // send it an up
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchUp(1);
    m_seatInterface->notifyTouchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 4);
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.first().first().value<KWayland::Client::TouchPoint *>(), tp2);
    QCOMPARE(tp2->id(), 1);
    QVERIFY(!tp2->isDown());
    QCOMPARE(tp2->position(), QPointF(5, 6));
    QCOMPARE(tp2->positions().size(), 1);
    QCOMPARE(tp2->time(), 4u);
    QCOMPARE(tp2->timestamps().count(), 2);
    QCOMPARE(tp2->upSerial(), m_seatInterface->display()->serial());
    QCOMPARE(tp2->surface().data(), s);

    // send another down and up
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchDown(1, QPointF(15, 26));
    m_seatInterface->notifyTouchFrame();
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchUp(1);
    // and send an up for the first point
    m_seatInterface->notifyTouchUp(0);
    m_seatInterface->notifyTouchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 1);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 6);
    QCOMPARE(pointAddedSpy.count(), 2);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 3);
    QCOMPARE(touch->sequence().count(), 3);
    QVERIFY(!touch->sequence().at(0)->isDown());
    QVERIFY(!touch->sequence().at(1)->isDown());
    QVERIFY(!touch->sequence().at(2)->isDown());
    QVERIFY(!m_seatInterface->isTouchSequence());

    // try cancel
    m_seatInterface->setFocusedTouchSurface(serverSurface, QPointF(15, 26));
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->notifyTouchDown(0, QPointF(15, 26));
    m_seatInterface->notifyTouchFrame();
    m_seatInterface->notifyTouchCancel();
    QVERIFY(sequenceCanceledSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 2);
    QCOMPARE(sequenceEndedSpy.count(), 1);
    QCOMPARE(sequenceCanceledSpy.count(), 1);
    QCOMPARE(frameEndedSpy.count(), 7);
    QCOMPARE(pointAddedSpy.count(), 2);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 3);
    QCOMPARE(touch->sequence().first()->position(), QPointF(0, 0));
}

void TestWaylandSeat::testKeymap()
{
    using namespace KWaylandServer;

    m_seatInterface->setHasKeyboard(true);
    QSignalSpy keyboardChangedSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    QVERIFY(keyboardChangedSpy.wait());

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(m_seat->createKeyboard());

    // create surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);

    QSignalSpy keymapChangedSpy(keyboard.get(), &KWayland::Client::Keyboard::keymapChanged);

    m_seatInterface->keyboard()->setKeymap(QByteArrayLiteral("foo"));
    QVERIFY(keymapChangedSpy.wait());
    int fd = keymapChangedSpy.first().first().toInt();
    QVERIFY(fd != -1);
    // Account for null terminator.
    QCOMPARE(keymapChangedSpy.first().last().value<quint32>(), 4u);
    QFile file;
    QVERIFY(file.open(fd, QIODevice::ReadOnly));
    const char *address = reinterpret_cast<char *>(file.map(0, keymapChangedSpy.first().last().value<quint32>()));
    QVERIFY(address);
    QCOMPARE(qstrcmp(address, "foo"), 0);
    file.close();

    // change the keymap
    keymapChangedSpy.clear();
    m_seatInterface->keyboard()->setKeymap(QByteArrayLiteral("bar"));
    QVERIFY(keymapChangedSpy.wait());
    fd = keymapChangedSpy.first().first().toInt();
    QVERIFY(fd != -1);
    // Account for null terminator.
    QCOMPARE(keymapChangedSpy.first().last().value<quint32>(), 4u);
    QVERIFY(file.open(fd, QIODevice::ReadWrite));
    address = reinterpret_cast<char *>(file.map(0, keymapChangedSpy.first().last().value<quint32>()));
    QVERIFY(address);
    QCOMPARE(qstrcmp(address, "bar"), 0);
}

QTEST_GUILESS_MAIN(TestWaylandSeat)
#include "test_wayland_seat.moc"
