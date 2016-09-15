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
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/datadevice.h"
#include "../../src/client/datadevicemanager.h"
#include "../../src/client/datasource.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/shm_pool.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/subsurface.h"
#include "../../src/client/touch.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/datadevicemanager_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/keyboard_interface.h"
#include "../../src/server/pointer_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/subcompositor_interface.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

#include <linux/input.h>
// System
#include <fcntl.h>
#include <unistd.h>

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
    void testKeyboardSubSurfaceTreeFromPointer();
    void testCursor();
    void testCursorDamage();
    void testKeyboard();
    void testCast();
    void testDestroy();
    void testSelection();
    void testTouch();
    void testDisconnect();
    void testPointerEnterOnUnboundSurface();
    // TODO: add test for keymap

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::SeatInterface *m_seatInterface;
    KWayland::Server::SubCompositorInterface *m_subCompositorInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Seat *m_seat;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::SubCompositor * m_subCompositor;
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
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_seat(nullptr)
    , m_shm(nullptr)
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandSeat::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    m_subCompositorInterface = m_display->createSubCompositor(m_display);
    QVERIFY(m_subCompositorInterface);
    m_subCompositorInterface->create();
    QVERIFY(m_subCompositorInterface->isValid());

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
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    QSignalSpy shmSpy(&registry, SIGNAL(shmAnnounced(quint32,quint32)));
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_seatInterface = m_display->createSeat();
    QVERIFY(m_seatInterface);
    m_seatInterface->setName(QStringLiteral("seat0"));
    m_seatInterface->create();
    QVERIFY(m_seatInterface->isValid());
    QVERIFY(seatSpy.wait());

    m_compositor = new KWayland::Client::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QSignalSpy nameSpy(m_seat, SIGNAL(nameChanged(QString)));
    QVERIFY(nameSpy.wait());

    m_shm = new KWayland::Client::ShmPool(this);
    m_shm->setup(registry.bindShm(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    QVERIFY(m_shm->isValid());

    m_subCompositor = registry.createSubCompositor(registry.interface(KWayland::Client::Registry::Interface::SubCompositor).name,
                                                   registry.interface(KWayland::Client::Registry::Interface::SubCompositor).version,
                                                   this);
    QVERIFY(m_subCompositor->isValid());
}

void TestWaylandSeat::cleanup()
{
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

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_seatInterface;
    m_seatInterface = nullptr;

    delete m_subCompositorInterface;
    m_subCompositorInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandSeat::testName()
{
    // no name set yet
    QCOMPARE(m_seat->name(), QStringLiteral("seat0"));

    QSignalSpy spy(m_seat, SIGNAL(nameChanged(QString)));
    QVERIFY(spy.isValid());

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

    QTest::newRow("none")             << false << false << false;
    QTest::newRow("pointer")          << true  << false << false;
    QTest::newRow("keyboard")         << false << true  << false;
    QTest::newRow("touch")            << false << false << true;
    QTest::newRow("pointer/keyboard") << true  << true  << false;
    QTest::newRow("pointer/touch")    << true  << false << true;
    QTest::newRow("keyboard/touch")   << false << true  << true;
    QTest::newRow("all")              << true  << true  << true;
}

void TestWaylandSeat::testCapabilities()
{
    QVERIFY(!m_seat->hasPointer());
    QVERIFY(!m_seat->hasKeyboard());
    QVERIFY(!m_seat->hasTouch());

    QFETCH(bool, pointer);
    QFETCH(bool, keyboard);
    QFETCH(bool, touch);

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    QSignalSpy keyboardSpy(m_seat, SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
    QSignalSpy touchSpy(m_seat, SIGNAL(hasTouchChanged(bool)));
    QVERIFY(touchSpy.isValid());

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
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy focusedPointerChangedSpy(m_seatInterface, &SeatInterface::focusedPointerChanged);
    QVERIFY(focusedPointerChangedSpy.isValid());

    m_seatInterface->setPointerPos(QPoint(20, 18));
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(focusedPointerChangedSpy.count(), 1);
    QVERIFY(!focusedPointerChangedSpy.first().first().value<PointerInterface*>());
    // no pointer yet
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(!m_seatInterface->focusedPointer());

    Pointer *p = m_seat->createPointer(m_seat);
    const Pointer &cp = *p;
    QVERIFY(p->isValid());
    QSignalSpy pointerCreatedSpy(m_seatInterface, SIGNAL(pointerCreated(KWayland::Server::PointerInterface*)));
    QVERIFY(pointerCreatedSpy.isValid());
    // once the pointer is created it should be set as the focused pointer
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(m_seatInterface->focusedPointer());
    QCOMPARE(pointerCreatedSpy.first().first().value<PointerInterface*>(), m_seatInterface->focusedPointer());
    QCOMPARE(focusedPointerChangedSpy.count(), 2);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<PointerInterface*>(), m_seatInterface->focusedPointer());

    m_seatInterface->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 3);
    QVERIFY(!focusedPointerChangedSpy.last().first().value<PointerInterface*>());
    serverSurface->client()->flush();
    QTest::qWait(100);

    QSignalSpy enteredSpy(p, SIGNAL(entered(quint32,QPointF)));
    QVERIFY(enteredSpy.isValid());

    QSignalSpy leftSpy(p, SIGNAL(left(quint32)));
    QVERIFY(leftSpy.isValid());

    QSignalSpy motionSpy(p, SIGNAL(motion(QPointF,quint32)));
    QVERIFY(motionSpy.isValid());

    QSignalSpy axisSpy(p, SIGNAL(axisChanged(quint32,KWayland::Client::Pointer::Axis,qreal)));
    QVERIFY(axisSpy.isValid());

    QSignalSpy buttonSpy(p, SIGNAL(buttonStateChanged(quint32,quint32,quint32,KWayland::Client::Pointer::ButtonState)));
    QVERIFY(buttonSpy.isValid());

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));
    PointerInterface *serverPointer = m_seatInterface->focusedPointer();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);
    QCOMPARE(focusedPointerChangedSpy.count(), 4);
    QCOMPARE(focusedPointerChangedSpy.last().first().value<PointerInterface*>(), serverPointer);

    // test motion
    m_seatInterface->setTimestamp(1);
    m_seatInterface->setPointerPos(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // test axis
    m_seatInterface->setTimestamp(2);
    m_seatInterface->pointerAxis(Qt::Horizontal, 10);
    QVERIFY(axisSpy.wait());
    m_seatInterface->setTimestamp(3);
    m_seatInterface->pointerAxis(Qt::Vertical, 20);
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<Pointer::Axis>(), Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<Pointer::Axis>(), Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // test button
    m_seatInterface->setTimestamp(4);
    m_seatInterface->pointerButtonPressed(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(5);
    m_seatInterface->pointerButtonPressed(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(6);
    m_seatInterface->pointerButtonReleased(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(7);
    m_seatInterface->pointerButtonReleased(1);
    QVERIFY(buttonSpy.wait());
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
    m_seatInterface->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 5);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // enter it again
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(0, 0));
    QCOMPARE(focusedPointerChangedSpy.count(), 6);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // destroy the focused pointer
    QSignalSpy unboundSpy(serverPointer, &Resource::unbound);
    QVERIFY(unboundSpy.isValid());
    QSignalSpy destroyedSpy(serverPointer, &Resource::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete p;
    QVERIFY(unboundSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);
    QCOMPARE(destroyedSpy.count(), 0);
    // now test that calling into the methods in Seat does not crash
    QCOMPARE(m_seatInterface->focusedPointer(), serverPointer);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    m_seatInterface->setTimestamp(8);
    m_seatInterface->setPointerPos(QPoint(10, 15));
    m_seatInterface->setTimestamp(9);
    m_seatInterface->pointerButtonPressed(1);
    m_seatInterface->setTimestamp(10);
    m_seatInterface->pointerButtonReleased(1);
    m_seatInterface->setTimestamp(11);
    m_seatInterface->pointerAxis(Qt::Horizontal, 10);
    m_seatInterface->setTimestamp(12);
    m_seatInterface->pointerAxis(Qt::Vertical, 20);
    m_seatInterface->setFocusedPointerSurface(nullptr);
    QCOMPARE(focusedPointerChangedSpy.count(), 7);
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QCOMPARE(focusedPointerChangedSpy.count(), 8);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedPointer());

    // and now destroy
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);
    QCOMPARE(destroyedSpy.count(), 1);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedPointer());

    // create a pointer again
    p = m_seat->createPointer(m_seat);
    QVERIFY(focusedPointerChangedSpy.wait());
    QCOMPARE(focusedPointerChangedSpy.count(), 9);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    serverPointer = m_seatInterface->focusedPointer();
    QVERIFY(serverPointer);

    QSignalSpy entered2Spy(p, &Pointer::entered);
    QVERIFY(entered2Spy.wait());
    QCOMPARE(p->enteredSurface(), s);
    delete s;
    QVERIFY(!p->enteredSurface());
    wl_display_flush(m_connection->display());
    QVERIFY(focusedPointerChangedSpy.wait());
    QCOMPARE(focusedPointerChangedSpy.count(), 10);
    QVERIFY(!m_seatInterface->focusedPointerSurface());
    QVERIFY(!m_seatInterface->focusedPointer());
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
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy pointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_seatInterface->setPointerPos(QPoint(20, 18));
    QFETCH(QMatrix4x4, enterTransformation);
    m_seatInterface->setFocusedPointerSurface(serverSurface, enterTransformation);
    QCOMPARE(m_seatInterface->focusedPointerSurfaceTransformation(), enterTransformation);
    // no pointer yet
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(!m_seatInterface->focusedPointer());

    Pointer *p = m_seat->createPointer(m_seat);
    const Pointer &cp = *p;
    QVERIFY(p->isValid());
    QSignalSpy pointerCreatedSpy(m_seatInterface, &SeatInterface::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());
    // once the pointer is created it should be set as the focused pointer
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(m_seatInterface->focusedPointer());
    QCOMPARE(pointerCreatedSpy.first().first().value<PointerInterface*>(), m_seatInterface->focusedPointer());

    m_seatInterface->setFocusedPointerSurface(nullptr);
    serverSurface->client()->flush();
    QTest::qWait(100);

    QSignalSpy enteredSpy(p, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    QSignalSpy leftSpy(p, &Pointer::left);
    QVERIFY(leftSpy.isValid());

    QSignalSpy motionSpy(p, &Pointer::motion);
    QVERIFY(motionSpy.isValid());

    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());
    m_seatInterface->setFocusedPointerSurface(serverSurface, enterTransformation);
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QTEST(enteredSpy.first().last().toPointF(), "expectedEnterPoint");
    PointerInterface *serverPointer = m_seatInterface->focusedPointer();
    QVERIFY(serverPointer);
    QCOMPARE(p->enteredSurface(), s);
    QCOMPARE(cp.enteredSurface(), s);

    // test motion
    m_seatInterface->setTimestamp(1);
    m_seatInterface->setPointerPos(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QTEST(motionSpy.first().first().toPointF(), "expectedMovePoint");
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // leave the surface
    m_seatInterface->setFocusedPointerSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());
    QVERIFY(!p->enteredSurface());
    QVERIFY(!cp.enteredSurface());

    // enter it again
    m_seatInterface->setFocusedPointerSurface(serverSurface);
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

    QTest::newRow("left")    << Qt::LeftButton    << quint32(BTN_LEFT);
    QTest::newRow("right")   << Qt::RightButton   << quint32(BTN_RIGHT);
    QTest::newRow("mid")     << Qt::MidButton     << quint32(BTN_MIDDLE);
    QTest::newRow("middle")  << Qt::MiddleButton  << quint32(BTN_MIDDLE);
    QTest::newRow("back")    << Qt::BackButton    << quint32(BTN_BACK);
    QTest::newRow("x1")      << Qt::XButton1      << quint32(BTN_BACK);
    QTest::newRow("extra1")  << Qt::ExtraButton1  << quint32(BTN_BACK);
    QTest::newRow("forward") << Qt::ForwardButton << quint32(BTN_FORWARD);
    QTest::newRow("x2")      << Qt::XButton2      << quint32(BTN_FORWARD);
    QTest::newRow("extra2")  << Qt::ExtraButton2  << quint32(BTN_FORWARD);
    QTest::newRow("task")    << Qt::TaskButton    << quint32(BTN_TASK);
    QTest::newRow("extra3")  << Qt::ExtraButton3  << quint32(BTN_TASK);
    QTest::newRow("extra4")  << Qt::ExtraButton4  << quint32(BTN_EXTRA);
    QTest::newRow("extra5")  << Qt::ExtraButton5  << quint32(BTN_SIDE);
    QTest::newRow("extra6")  << Qt::ExtraButton6  << quint32(0x118);
    QTest::newRow("extra7")  << Qt::ExtraButton7  << quint32(0x119);
    QTest::newRow("extra8")  << Qt::ExtraButton8  << quint32(0x11a);
    QTest::newRow("extra9")  << Qt::ExtraButton9  << quint32(0x11b);
    QTest::newRow("extra10") << Qt::ExtraButton10 << quint32(0x11c);
    QTest::newRow("extra11") << Qt::ExtraButton11 << quint32(0x11d);
    QTest::newRow("extra12") << Qt::ExtraButton12 << quint32(0x11e);
    QTest::newRow("extra13") << Qt::ExtraButton13 << quint32(0x11f);
}

void TestWaylandSeat::testPointerButton()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy buttonChangedSpy(p.data(), SIGNAL(buttonStateChanged(quint32,quint32,quint32,KWayland::Client::Pointer::ButtonState)));
    QVERIFY(buttonChangedSpy.isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    m_seatInterface->setPointerPos(QPoint(20, 18));
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(m_seatInterface->focusedPointer());

    QCoreApplication::processEvents();

    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    PointerInterface *serverPointer = m_seatInterface->focusedPointer();
    QVERIFY(serverPointer);
    QFETCH(Qt::MouseButton, qtButton);
    QFETCH(quint32, waylandButton);
    quint32 msec = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), false);
    m_seatInterface->setTimestamp(msec);
    m_seatInterface->pointerButtonPressed(qtButton);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), true);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), true);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), Pointer::ButtonState::Pressed);
    msec = QDateTime::currentMSecsSinceEpoch();
    m_seatInterface->setTimestamp(QDateTime::currentMSecsSinceEpoch());
    m_seatInterface->pointerButtonReleased(qtButton);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(waylandButton), false);
    QCOMPARE(m_seatInterface->isPointerButtonPressed(qtButton), false);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 2);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), m_seatInterface->pointerButtonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), Pointer::ButtonState::Released);
}

void TestWaylandSeat::testPointerSubSurfaceTree()
{
    // this test verifies that pointer motion on a surface with sub-surfaces sends motion enter/leave to the sub-surface
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    // first create the pointer
    QSignalSpy hasPointerChangedSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Pointer> pointer(m_seat->createPointer());

    // create a sub surface tree
    // parent surface (100, 100) with one sub surface taking the half of it's size (50, 100)
    // which has two further children (50, 50) which are overlapping
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<Surface> childSurface(m_compositor->createSurface());
    QScopedPointer<Surface> grandChild1Surface(m_compositor->createSurface());
    QScopedPointer<Surface> grandChild2Surface(m_compositor->createSurface());
    QScopedPointer<SubSurface> childSubSurface(m_subCompositor->createSubSurface(childSurface.data(), parentSurface.data()));
    QScopedPointer<SubSurface> grandChild1SubSurface(m_subCompositor->createSubSurface(grandChild1Surface.data(), childSurface.data()));
    QScopedPointer<SubSurface> grandChild2SubSurface(m_subCompositor->createSubSurface(grandChild2Surface.data(), childSurface.data()));
    grandChild2SubSurface->setPosition(QPoint(0, 25));

    // let's map the surfaces
    auto render = [this] (Surface *s, const QSize &size) {
        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::black);
        s->attachBuffer(m_shm->createBuffer(image));
        s->damage(QRect(QPoint(0, 0), size));
        s->commit(Surface::CommitFlag::None);
    };
    render(grandChild2Surface.data(), QSize(50, 50));
    render(grandChild1Surface.data(), QSize(50, 50));
    render(childSurface.data(), QSize(50, 100));
    render(parentSurface.data(), QSize(100, 100));

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface->isMapped());

    // send in pointer events
    QSignalSpy enteredSpy(pointer.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer.data(), &Pointer::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy motionSpy(pointer.data(), &Pointer::motion);
    QVERIFY(motionSpy.isValid());
    // first to the grandChild2 in the overlapped area
    quint32 timestamp = 1;
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(25, 50));
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 0);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 25));
    QCOMPARE(pointer->enteredSurface(), grandChild2Surface.data());
    // a motion on grandchild2
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(25, 60));
    QVERIFY(motionSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(25, 35));
    // motion which changes to childSurface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(25, 80));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(25, 80));
    QCOMPARE(pointer->enteredSurface(), childSurface.data());
    // a leave for the whole surface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setFocusedPointerSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 1);
    // a new enter on the main surface
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(75, 50));
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 3);
    QCOMPARE(leftSpy.count(), 2);
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(enteredSpy.last().last().toPointF(), QPointF(75, 50));
    QCOMPARE(pointer->enteredSurface(), parentSurface.data());
}

void TestWaylandSeat::testKeyboardSubSurfaceTreeFromPointer()
{
    // this test verifies that when clicking on a sub-surface the keyboard focus passes to it
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    // first create the pointer
    QSignalSpy hasPointerChangedSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(hasPointerChangedSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(hasPointerChangedSpy.wait());
    QScopedPointer<Pointer> pointer(m_seat->createPointer());

    // and create keyboard
    QSignalSpy hasKeyboardChangedSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(hasKeyboardChangedSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(hasKeyboardChangedSpy.wait());
    QScopedPointer<Keyboard> keyboard(m_seat->createKeyboard());

    // create a sub surface tree
    // parent surface (100, 100) with one sub surface taking the half of it's size (50, 100)
    // which has two further children (50, 50) which are overlapping
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<Surface> childSurface(m_compositor->createSurface());
    QScopedPointer<Surface> grandChild1Surface(m_compositor->createSurface());
    QScopedPointer<Surface> grandChild2Surface(m_compositor->createSurface());
    QScopedPointer<SubSurface> childSubSurface(m_subCompositor->createSubSurface(childSurface.data(), parentSurface.data()));
    QScopedPointer<SubSurface> grandChild1SubSurface(m_subCompositor->createSubSurface(grandChild1Surface.data(), childSurface.data()));
    QScopedPointer<SubSurface> grandChild2SubSurface(m_subCompositor->createSubSurface(grandChild2Surface.data(), childSurface.data()));
    grandChild2SubSurface->setPosition(QPoint(0, 25));

    // let's map the surfaces
    auto render = [this] (Surface *s, const QSize &size) {
        QImage image(size, QImage::Format_ARGB32);
        image.fill(Qt::black);
        s->attachBuffer(m_shm->createBuffer(image));
        s->damage(QRect(QPoint(0, 0), size));
        s->commit(Surface::CommitFlag::None);
    };
    render(grandChild2Surface.data(), QSize(50, 50));
    render(grandChild1Surface.data(), QSize(50, 50));
    render(childSurface.data(), QSize(50, 100));
    render(parentSurface.data(), QSize(100, 100));

    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface->isMapped());

    // pass keyboard focus to the main surface
    QSignalSpy enterSpy(keyboard.data(), &Keyboard::entered);
    QVERIFY(enterSpy.isValid());
    QSignalSpy leftSpy(keyboard.data(), &Keyboard::left);
    QVERIFY(leftSpy.isValid());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(enterSpy.wait());
    QCOMPARE(enterSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(keyboard->enteredSurface(), parentSurface.data());

    // now pass also pointer focus to the surface
    QSignalSpy pointerEnterSpy(pointer.data(), &Pointer::entered);
    QVERIFY(pointerEnterSpy.isValid());
    quint32 timestamp = 1;
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->setPointerPos(QPointF(25, 50));
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QVERIFY(pointerEnterSpy.wait());
    QCOMPARE(pointerEnterSpy.count(), 1);
    // should not have affected the keyboard
    QCOMPARE(enterSpy.count(), 1);
    QCOMPARE(leftSpy.count(), 0);

    // let's click
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonReleased(Qt::LeftButton);
    QVERIFY(enterSpy.wait());
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyboard->enteredSurface(), grandChild2Surface.data());

    // click on same surface should not trigger another enter
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonPressed(Qt::LeftButton);
    m_seatInterface->setTimestamp(timestamp++);
    m_seatInterface->pointerButtonReleased(Qt::LeftButton);
    QVERIFY(!enterSpy.wait(200));
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(keyboard->enteredSurface(), grandChild2Surface.data());

    // unfocus keyboard
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(enterSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 2);
}

void TestWaylandSeat::testCursor()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QScopedPointer<Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();

    QSignalSpy enteredSpy(p.data(), SIGNAL(entered(quint32,QPointF)));
    QVERIFY(enteredSpy.isValid());

    m_seatInterface->setPointerPos(QPoint(20, 18));
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    quint32 serial = m_seatInterface->display()->serial();
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), serial);
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(m_seatInterface->focusedPointer());
    QVERIFY(!m_seatInterface->focusedPointer()->cursor());

    QSignalSpy cursorChangedSpy(m_seatInterface->focusedPointer(), SIGNAL(cursorChanged()));
    QVERIFY(cursorChangedSpy.isValid());
    // just remove the pointer
    p->setCursor(nullptr);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(cursorChangedSpy.count(), 1);
    auto cursor = m_seatInterface->focusedPointer()->cursor();
    QVERIFY(cursor);
    QVERIFY(!cursor->surface());
    QCOMPARE(cursor->hotspot(), QPoint());
    QCOMPARE(cursor->enteredSerial(), serial);
    QCOMPARE(cursor->pointer(), m_seatInterface->focusedPointer());

    QSignalSpy hotspotChangedSpy(cursor, SIGNAL(hotspotChanged()));
    QVERIFY(hotspotChangedSpy.isValid());
    QSignalSpy surfaceChangedSpy(cursor, SIGNAL(surfaceChanged()));
    QVERIFY(surfaceChangedSpy.isValid());
    QSignalSpy enteredSerialChangedSpy(cursor, SIGNAL(enteredSerialChanged()));
    QVERIFY(enteredSerialChangedSpy.isValid());
    QSignalSpy changedSpy(cursor, SIGNAL(changed()));
    QVERIFY(changedSpy.isValid());

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
    cursorSurface->commit(Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 3);
    QCOMPARE(cursorChangedSpy.count(), 4);
    QCOMPARE(surfaceChangedSpy.count(), 1);
    QCOMPARE(cursor->surface()->buffer()->data(), img);

    // and add another image to the surface
    QImage blue(QSize(10, 20), QImage::Format_ARGB32);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 20));
    cursorSurface->commit(Surface::CommitFlag::None);
    QVERIFY(changedSpy.wait());
    QCOMPARE(changedSpy.count(), 4);
    QCOMPARE(cursorChangedSpy.count(), 5);
    QCOMPARE(cursor->surface()->buffer()->data(), blue);

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
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy pointerSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    // create pointer
    QScopedPointer<Pointer> p(m_seat->createPointer());
    QVERIFY(p->isValid());
    QSignalSpy enteredSpy(p.data(), &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    // create surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // send enter to the surface
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QVERIFY(enteredSpy.wait());

    // create a signal spy for the cursor changed signal
    auto pointer = m_seatInterface->focusedPointer();
    QSignalSpy cursorChangedSpy(pointer, &PointerInterface::cursorChanged);
    QVERIFY(cursorChangedSpy.isValid());

    // now let's set the cursor
    Surface *cursorSurface = m_compositor->createSurface(m_compositor);
    QVERIFY(cursorSurface);
    QImage red(QSize(10, 10), QImage::Format_ARGB32);
    red.fill(Qt::red);
    cursorSurface->attachBuffer(m_shm->createBuffer(red));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(Surface::CommitFlag::None);
    p->setCursor(cursorSurface, QPoint(0, 0));
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(pointer->cursor()->surface()->buffer()->data(), red);

    // and damage the surface
    QImage blue(QSize(10, 10), QImage::Format_ARGB32);
    blue.fill(Qt::blue);
    cursorSurface->attachBuffer(m_shm->createBuffer(blue));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit(Surface::CommitFlag::None);
    QVERIFY(cursorChangedSpy.wait());
    QCOMPARE(pointer->cursor()->surface()->buffer()->data(), blue);
}

void TestWaylandSeat::testKeyboard()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy keyboardSpy(m_seat, SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());

    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    // no keyboard yet
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedKeyboard());

    Keyboard *keyboard = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy(keyboard, &Keyboard::keyRepeatChanged);
    QVERIFY(repeatInfoSpy.isValid());
    const Keyboard &ckeyboard = *keyboard;
    QVERIFY(keyboard->isValid());
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    auto serverKeyboard = m_seatInterface->focusedKeyboard();
    QVERIFY(serverKeyboard);

    // we should get the repeat info announced
    QCOMPARE(repeatInfoSpy.count(), 1);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard->keyRepeatDelay(), 0);
    QCOMPARE(keyboard->keyRepeatRate(), 0);

    // let's change repeat in server
    m_seatInterface->setKeyRepeatInfo(25, 660);
    m_seatInterface->focusedKeyboard()->client()->flush();
    QVERIFY(repeatInfoSpy.wait());
    QCOMPARE(repeatInfoSpy.count(), 2);
    QCOMPARE(keyboard->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard->keyRepeatRate(), 25);
    QCOMPARE(keyboard->keyRepeatDelay(), 660);

    m_seatInterface->setTimestamp(1);
    m_seatInterface->keyPressed(KEY_K);
    m_seatInterface->setTimestamp(2);
    m_seatInterface->keyPressed(KEY_D);
    m_seatInterface->setTimestamp(3);
    m_seatInterface->keyPressed(KEY_E);

    QSignalSpy modifierSpy(keyboard, SIGNAL(modifiersChanged(quint32,quint32,quint32,quint32)));
    QVERIFY(modifierSpy.isValid());

    QSignalSpy enteredSpy(keyboard, SIGNAL(entered(quint32)));
    QVERIFY(enteredSpy.isValid());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboard()->focusedSurface(), serverSurface);

    // we get the modifiers sent after the enter
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.first().at(0).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(1).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(2).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(3).value<quint32>(), quint32(0));
    QCOMPARE(enteredSpy.count(), 1);
    // TODO: get through API
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial() - 1);

    QSignalSpy keyChangedSpy(keyboard, SIGNAL(keyChanged(quint32,KWayland::Client::Keyboard::KeyState,quint32)));
    QVERIFY(keyChangedSpy.isValid());

    m_seatInterface->setTimestamp(4);
    m_seatInterface->keyReleased(KEY_E);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(5);
    m_seatInterface->keyReleased(KEY_D);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(6);
    m_seatInterface->keyReleased(KEY_K);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(7);
    m_seatInterface->keyPressed(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    m_seatInterface->setTimestamp(8);
    m_seatInterface->keyReleased(KEY_F1);
    QVERIFY(keyChangedSpy.wait());

    QCOMPARE(keyChangedSpy.count(), 5);
    QCOMPARE(keyChangedSpy.at(0).at(0).value<quint32>(), quint32(KEY_E));
    QCOMPARE(keyChangedSpy.at(0).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(0).at(2).value<quint32>(), quint32(4));
    QCOMPARE(keyChangedSpy.at(1).at(0).value<quint32>(), quint32(KEY_D));
    QCOMPARE(keyChangedSpy.at(1).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(1).at(2).value<quint32>(), quint32(5));
    QCOMPARE(keyChangedSpy.at(2).at(0).value<quint32>(), quint32(KEY_K));
    QCOMPARE(keyChangedSpy.at(2).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(2).at(2).value<quint32>(), quint32(6));
    QCOMPARE(keyChangedSpy.at(3).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(3).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Pressed);
    QCOMPARE(keyChangedSpy.at(3).at(2).value<quint32>(), quint32(7));
    QCOMPARE(keyChangedSpy.at(4).at(0).value<quint32>(), quint32(KEY_F1));
    QCOMPARE(keyChangedSpy.at(4).at(1).value<Keyboard::KeyState>(), Keyboard::KeyState::Released);
    QCOMPARE(keyChangedSpy.at(4).at(2).value<quint32>(), quint32(8));

    // releasing a key which is already released should not set a key changed
    m_seatInterface->keyReleased(KEY_F1);
    QVERIFY(!keyChangedSpy.wait(200));
    // let's press it again
    m_seatInterface->keyPressed(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 6);
    // press again should be ignored
    m_seatInterface->keyPressed(KEY_F1);
    QVERIFY(!keyChangedSpy.wait(200));
    // and release
    m_seatInterface->keyReleased(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    QCOMPARE(keyChangedSpy.count(), 7);

    m_seatInterface->updateKeyboardModifiers(1, 2, 3, 4);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(modifierSpy.last().at(1).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.last().at(2).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.last().at(3).value<quint32>(), quint32(4));

    QSignalSpy leftSpy(keyboard, SIGNAL(left(quint32)));
    QVERIFY(leftSpy.isValid());
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QVERIFY(!m_seatInterface->focusedKeyboardSurface());
    QVERIFY(!m_seatInterface->focusedKeyboard());
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // TODO: get through API
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial() -1 );

    QVERIFY(!keyboard->enteredSurface());
    QVERIFY(!ckeyboard.enteredSurface());

    // enter it again
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboard()->focusedSurface(), serverSurface);
    QCOMPARE(enteredSpy.count(), 2);

    QCOMPARE(keyboard->enteredSurface(), s);
    QCOMPARE(ckeyboard.enteredSurface(), s);

    QSignalSpy serverSurfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(serverSurfaceDestroyedSpy.isValid());
    delete s;
    QVERIFY(serverSurfaceDestroyedSpy.wait());
    QVERIFY(!m_seatInterface->focusedKeyboardSurface());
    QVERIFY(!m_seatInterface->focusedKeyboard());
    QVERIFY(!serverKeyboard->focusedSurface());

    // let's create a Surface again
    QScopedPointer<Surface> s2(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    QCOMPARE(surfaceCreatedSpy.count(), 2);
    serverSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(serverSurface);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboard(), serverKeyboard);

    // delete the Keyboard
    QSignalSpy unboundSpy(serverKeyboard, &Resource::unbound);
    QVERIFY(unboundSpy.isValid());
    QSignalSpy destroyedSpy(serverKeyboard, &Resource::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete keyboard;
    QVERIFY(unboundSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);
    QCOMPARE(destroyedSpy.count(), 0);
    // verify that calling into the Keyboard related functionality doesn't crash
    m_seatInterface->setTimestamp(9);
    m_seatInterface->keyPressed(KEY_F2);
    m_seatInterface->setTimestamp(10);
    m_seatInterface->keyReleased(KEY_F2);
    m_seatInterface->setKeyRepeatInfo(30, 560);
    m_seatInterface->setKeyRepeatInfo(25, 660);
    m_seatInterface->updateKeyboardModifiers(5, 6, 7, 8);
    m_seatInterface->setKeymap(open("/dev/null", O_RDONLY), 0);
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedKeyboard());

    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);

    // create a second Keyboard to verify that repeat info is announced properly
    Keyboard *keyboard2 = m_seat->createKeyboard(m_seat);
    QSignalSpy repeatInfoSpy2(keyboard2, &Keyboard::keyRepeatChanged);
    QVERIFY(repeatInfoSpy2.isValid());
    QVERIFY(keyboard2->isValid());
    QCOMPARE(keyboard2->isKeyRepeatEnabled(), false);
    QCOMPARE(keyboard2->keyRepeatDelay(), 0);
    QCOMPARE(keyboard2->keyRepeatRate(), 0);
    wl_display_flush(m_connection->display());
    QVERIFY(repeatInfoSpy2.wait());
    QCOMPARE(keyboard2->isKeyRepeatEnabled(), true);
    QCOMPARE(keyboard2->keyRepeatRate(), 25);
    QCOMPARE(keyboard2->keyRepeatDelay(), 660);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    serverKeyboard = m_seatInterface->focusedKeyboard();
    QVERIFY(serverKeyboard);
    QSignalSpy keyboard2DestroyedSpy(serverKeyboard, &QObject::destroyed);
    QVERIFY(keyboard2DestroyedSpy.isValid());
    delete keyboard2;
    QVERIFY(keyboard2DestroyedSpy.wait());
    // this should have unset it on the server
    QVERIFY(!m_seatInterface->focusedKeyboard());
    // but not the surface
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
}

void TestWaylandSeat::testCast()
{
    using namespace KWayland::Client;
    Registry registry;
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(seatSpy.wait());
    Seat s;
    QVERIFY(!s.isValid());
    auto wlSeat = registry.bindSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>());
    QVERIFY(wlSeat);
    s.setup(wlSeat);
    QVERIFY(s.isValid());

    QCOMPARE((wl_seat*)s, wlSeat);
    const Seat &s2(s);
    QCOMPARE((wl_seat*)s2, wlSeat);
}

void TestWaylandSeat::testDestroy()
{
    using namespace KWayland::Client;
    QSignalSpy keyboardSpy(m_seat, SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());
    Keyboard *k = m_seat->createKeyboard(m_seat);
    QVERIFY(k->isValid());

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());
    Pointer *p = m_seat->createPointer(m_seat);
    QVERIFY(p->isValid());

    QSignalSpy touchSpy(m_seat, SIGNAL(hasTouchChanged(bool)));
    QVERIFY(touchSpy.isValid());
    m_seatInterface->setHasTouch(true);
    QVERIFY(touchSpy.wait());
    Touch *t = m_seat->createTouch(m_seat);
    QVERIFY(t->isValid());

    delete m_compositor;
    m_compositor = nullptr;
    connect(m_connection, &ConnectionThread::connectionDied, m_seat, &Seat::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_shm, &ShmPool::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_subCompositor, &SubCompositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);
    QVERIFY(m_seat->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    m_compositorInterface = nullptr;
    m_seatInterface = nullptr;
    m_subCompositorInterface = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the seat should be destroyed;
    QVERIFY(!m_seat->isValid());
    QVERIFY(!k->isValid());
    QVERIFY(!p->isValid());
    QVERIFY(!t->isValid());

    // calling destroy again should not fail
    m_seat->destroy();
    k->destroy();
    p->destroy();
    t->destroy();
}

void TestWaylandSeat::testSelection()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    QScopedPointer<DataDeviceManagerInterface> ddmi(m_display->createDataDeviceManager());
    ddmi->create();
    Registry registry;
    QSignalSpy dataDeviceManagerSpy(&registry, SIGNAL(dataDeviceManagerAnnounced(quint32,quint32)));
    QVERIFY(dataDeviceManagerSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(dataDeviceManagerSpy.wait());
    QScopedPointer<DataDeviceManager> ddm(registry.createDataDeviceManager(dataDeviceManagerSpy.first().first().value<quint32>(),
                                                                           dataDeviceManagerSpy.first().last().value<quint32>()));
    QVERIFY(ddm->isValid());

    QScopedPointer<DataDevice> dd1(ddm->getDataDevice(m_seat));
    QVERIFY(dd1->isValid());
    QSignalSpy selectionSpy(dd1.data(), SIGNAL(selectionOffered(KWayland::Client::DataOffer*)));
    QVERIFY(selectionSpy.isValid());
    QSignalSpy selectionClearedSpy(dd1.data(), SIGNAL(selectionCleared()));
    QVERIFY(selectionClearedSpy.isValid());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surface->isValid());
    QVERIFY(surfaceCreatedSpy.wait());
    auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(!m_seatInterface->selection());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedKeyboardSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedKeyboard());
    QVERIFY(selectionClearedSpy.wait());
    QVERIFY(selectionSpy.isEmpty());
    QVERIFY(!selectionClearedSpy.isEmpty());
    selectionClearedSpy.clear();
    QVERIFY(!m_seatInterface->selection());

    // now let's try to set a selection - we have keyboard focus, so it should be sent to us
    QScopedPointer<DataSource> ds(ddm->createDataSource());
    QVERIFY(ds->isValid());
    ds->offer(QStringLiteral("text/plain"));
    dd1->setSelection(0, ds.data());
    QVERIFY(selectionSpy.wait());
    QCOMPARE(selectionSpy.count(), 1);
    auto ddi = m_seatInterface->selection();
    QVERIFY(ddi);
    auto df = selectionSpy.first().first().value<DataOffer*>();
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
    QVERIFY(!m_seatInterface->focusedKeyboard());
    serverSurface->client()->flush();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // try to set Selection
    dd1->setSelection(0, ds.data());
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
    // now clear it manully
    m_seatInterface->setSelection(nullptr);
    QVERIFY(selectionClearedSpy.wait());
    QCOMPARE(selectionSpy.count(), 2);
}

void TestWaylandSeat::testTouch()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    QSignalSpy touchSpy(m_seat, SIGNAL(hasTouchChanged(bool)));
    QVERIFY(touchSpy.isValid());
    m_seatInterface->setHasTouch(true);
    QVERIFY(touchSpy.wait());

    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    m_seatInterface->setFocusedTouchSurface(serverSurface);
    // no keyboard yet
    QCOMPARE(m_seatInterface->focusedTouchSurface(), serverSurface);
    QVERIFY(!m_seatInterface->focusedTouch());

    QSignalSpy touchCreatedSpy(m_seatInterface, SIGNAL(touchCreated(KWayland::Server::TouchInterface*)));
    QVERIFY(touchCreatedSpy.isValid());
    Touch *touch = m_seat->createTouch(m_seat);
    QVERIFY(touch->isValid());
    QVERIFY(touchCreatedSpy.wait());
    auto serverTouch = m_seatInterface->focusedTouch();
    QVERIFY(serverTouch);
    QCOMPARE(touchCreatedSpy.first().first().value<KWayland::Server::TouchInterface*>(), m_seatInterface->focusedTouch());

    QSignalSpy sequenceStartedSpy(touch, SIGNAL(sequenceStarted(KWayland::Client::TouchPoint*)));
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy sequenceEndedSpy(touch, SIGNAL(sequenceEnded()));
    QVERIFY(sequenceEndedSpy.isValid());
    QSignalSpy sequenceCanceledSpy(touch, SIGNAL(sequenceCanceled()));
    QVERIFY(sequenceCanceledSpy.isValid());
    QSignalSpy frameEndedSpy(touch, SIGNAL(frameEnded()));
    QVERIFY(frameEndedSpy.isValid());
    QSignalSpy pointAddedSpy(touch, SIGNAL(pointAdded(KWayland::Client::TouchPoint*)));
    QVERIFY(pointAddedSpy.isValid());
    QSignalSpy pointMovedSpy(touch, SIGNAL(pointMoved(KWayland::Client::TouchPoint*)));
    QVERIFY(pointMovedSpy.isValid());
    QSignalSpy pointRemovedSpy(touch, SIGNAL(pointRemoved(KWayland::Client::TouchPoint*)));
    QVERIFY(pointRemovedSpy.isValid());

    // try a few things
    m_seatInterface->setFocusedTouchSurfacePosition(QPointF(10, 20));
    QCOMPARE(m_seatInterface->focusedTouchSurfacePosition(), QPointF(10, 20));
    m_seatInterface->setTimestamp(1);
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 0);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 0);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);
    QCOMPARE(pointRemovedSpy.count(), 0);
    TouchPoint *tp = sequenceStartedSpy.first().first().value<TouchPoint*>();
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
    m_seatInterface->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(frameEndedSpy.count(), 1);

    // move the one point
    m_seatInterface->setTimestamp(2);
    m_seatInterface->touchMove(0, QPointF(10, 20));
    m_seatInterface->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 2);
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.first().first().value<TouchPoint*>(), tp);

    QCOMPARE(tp->id(), 0);
    QVERIFY(tp->isDown());
    QCOMPARE(tp->position(), QPointF(0, 0));
    QCOMPARE(tp->positions().size(), 2);
    QCOMPARE(tp->time(), 2u);
    QCOMPARE(tp->timestamps().count(), 2);
    QCOMPARE(tp->upSerial(), 0u);
    QCOMPARE(tp->surface().data(), s);

    // add onther point
    m_seatInterface->setTimestamp(3);
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 1);
    m_seatInterface->touchFrame();
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
    TouchPoint *tp2 = pointAddedSpy.first().first().value<TouchPoint*>();
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
    m_seatInterface->setTimestamp(4);
    m_seatInterface->touchUp(1);
    m_seatInterface->touchFrame();
    QVERIFY(frameEndedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(sequenceEndedSpy.count(), 0);
    QCOMPARE(sequenceCanceledSpy.count(), 0);
    QCOMPARE(frameEndedSpy.count(), 4);
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.first().first().value<TouchPoint*>(), tp2);
    QCOMPARE(tp2->id(), 1);
    QVERIFY(!tp2->isDown());
    QCOMPARE(tp2->position(), QPointF(5, 6));
    QCOMPARE(tp2->positions().size(), 1);
    QCOMPARE(tp2->time(), 4u);
    QCOMPARE(tp2->timestamps().count(), 2);
    QCOMPARE(tp2->upSerial(), m_seatInterface->display()->serial());
    QCOMPARE(tp2->surface().data(), s);

    // send another down and up
    m_seatInterface->setTimestamp(5);
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 1);
    m_seatInterface->touchFrame();
    m_seatInterface->setTimestamp(6);
    m_seatInterface->touchUp(1);
    // and send an up for the first point
    m_seatInterface->touchUp(0);
    m_seatInterface->touchFrame();
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
    m_seatInterface->setTimestamp(7);
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 0);
    m_seatInterface->touchFrame();
    m_seatInterface->cancelTouchSequence();
    QVERIFY(sequenceCanceledSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 2);
    QCOMPARE(sequenceEndedSpy.count(), 1);
    QCOMPARE(sequenceCanceledSpy.count(), 1);
    QCOMPARE(frameEndedSpy.count(), 7);
    QCOMPARE(pointAddedSpy.count(), 2);
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(pointRemovedSpy.count(), 3);
    QCOMPARE(touch->sequence().first()->position(), QPointF(0, 0));

    // destroy touch on client side
    QSignalSpy unboundSpy(serverTouch, &TouchInterface::unbound);
    QVERIFY(unboundSpy.isValid());
    QSignalSpy destroyedSpy(serverTouch, &TouchInterface::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete touch;
    QVERIFY(unboundSpy.wait());
    QCOMPARE(unboundSpy.count(), 1);
    QCOMPARE(destroyedSpy.count(), 0);
    QVERIFY(!serverTouch->resource());
    // try to call into all the the methods of the touch interface, should not crash
    QCOMPARE(m_seatInterface->focusedTouch(), serverTouch);
    m_seatInterface->setTimestamp(8);
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 0);
    m_seatInterface->touchFrame();
    m_seatInterface->touchMove(0, QPointF(0, 0));
    QCOMPARE(m_seatInterface->touchDown(QPointF(15, 26)), 1);
    m_seatInterface->cancelTouchSequence();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(destroyedSpy.count(), 1);
    // should have unset the focused touch
    QVERIFY(!m_seatInterface->focusedTouch());
    // but not the focused touch surface
    QCOMPARE(m_seatInterface->focusedTouchSurface(), serverSurface);
}

void TestWaylandSeat::testDisconnect()
{
    // this test verifies that disconnecting the client cleans up correctly
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    QSignalSpy keyboardCreatedSpy(m_seatInterface, &SeatInterface::keyboardCreated);
    QVERIFY(keyboardCreatedSpy.isValid());
    QSignalSpy pointerCreatedSpy(m_seatInterface, &SeatInterface::pointerCreated);
    QVERIFY(pointerCreatedSpy.isValid());
    QSignalSpy touchCreatedSpy(m_seatInterface, &SeatInterface::touchCreated);
    QVERIFY(touchCreatedSpy.isValid());

    // create the things we need
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    QSignalSpy touchSpy(m_seat, &Seat::hasTouchChanged);
    QVERIFY(touchSpy.isValid());
    QVERIFY(touchSpy.wait());

    QScopedPointer<Keyboard> keyboard(m_seat->createKeyboard());
    QVERIFY(!keyboard.isNull());
    QVERIFY(keyboardCreatedSpy.wait());
    auto serverKeyboard = keyboardCreatedSpy.first().first().value<KeyboardInterface*>();
    QVERIFY(serverKeyboard);

    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    QVERIFY(pointerCreatedSpy.wait());
    auto serverPointer = pointerCreatedSpy.first().first().value<PointerInterface*>();
    QVERIFY(serverPointer);

    QScopedPointer<Touch> touch(m_seat->createTouch());
    QVERIFY(!touch.isNull());
    QVERIFY(touchCreatedSpy.wait());
    auto serverTouch = touchCreatedSpy.first().first().value<TouchInterface*>();
    QVERIFY(serverTouch);

    // setup destroys
    QSignalSpy keyboardDestroyedSpy(serverKeyboard, &QObject::destroyed);
    QVERIFY(keyboardDestroyedSpy.isValid());
    QSignalSpy pointerDestroyedSpy(serverPointer, &QObject::destroyed);
    QVERIFY(pointerDestroyedSpy.isValid());
    QSignalSpy touchDestroyedSpy(serverTouch, &QObject::destroyed);
    QVERIFY(touchDestroyedSpy.isValid());
    QSignalSpy clientDisconnectedSpy(serverKeyboard->client(), &ClientConnection::disconnected);
    QVERIFY(clientDisconnectedSpy.isValid());

    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    QVERIFY(clientDisconnectedSpy.wait());
    QCOMPARE(clientDisconnectedSpy.count(), 1);
    QCOMPARE(keyboardDestroyedSpy.count(), 0);
    QCOMPARE(pointerDestroyedSpy.count(), 0);
    QCOMPARE(touchDestroyedSpy.count(), 0);
    QVERIFY(keyboardDestroyedSpy.wait());
    QCOMPARE(keyboardDestroyedSpy.count(), 1);
    QCOMPARE(pointerDestroyedSpy.count(), 1);
    QCOMPARE(touchDestroyedSpy.count(), 1);

    keyboard->destroy();
    pointer->destroy();
    touch->destroy();
    m_compositor->destroy();
    m_seat->destroy();
    m_shm->destroy();
    m_subCompositor->destroy();
    m_queue->destroy();
}

void TestWaylandSeat::testPointerEnterOnUnboundSurface()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    // create the things we need
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasPointer(true);
    m_seatInterface->setHasTouch(true);
    QSignalSpy pointerChangedSpy(m_seat, &Seat::hasPointerChanged);
    QVERIFY(pointerChangedSpy.isValid());
    QVERIFY(pointerChangedSpy.wait());

    // create pointer and Surface
    QScopedPointer<Pointer> pointer(m_seat->createPointer());
    QVERIFY(!pointer.isNull());
    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QScopedPointer<Surface> s(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    // unbind the surface again
    QSignalSpy surfaceUnboundSpy(serverSurface, &SurfaceInterface::unbound);
    QVERIFY(surfaceUnboundSpy.isValid());
    s.reset();
    QVERIFY(surfaceUnboundSpy.wait());
    QSignalSpy clientErrorSpy(m_connection, &ConnectionThread::errorOccurred);
    QVERIFY(clientErrorSpy.isValid());
    m_seatInterface->setFocusedPointerSurface(serverSurface);
    QVERIFY(!clientErrorSpy.wait(100));
}

QTEST_GUILESS_MAIN(TestWaylandSeat)
#include "test_wayland_seat.moc"
