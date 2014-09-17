/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../wayland_client/compositor.h"
#include "../../wayland_client/connection_thread.h"
#include "../../wayland_client/keyboard.h"
#include "../../wayland_client/pointer.h"
#include "../../wayland_client/surface.h"
#include "../../wayland_client/registry.h"
#include "../../wayland_client/seat.h"
#include "../../wayland_client/shm_pool.h"
#include "../../wayland_server/buffer_interface.h"
#include "../../wayland_server/compositor_interface.h"
#include "../../wayland_server/display.h"
#include "../../wayland_server/seat_interface.h"
#include "../../wayland_server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

#include <linux/input.h>

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
    void testKeyboard();
    // TODO: add test for keymap

private:
    KWin::WaylandServer::Display *m_display;
    KWin::WaylandServer::CompositorInterface *m_compositorInterface;
    KWin::WaylandServer::SeatInterface *m_seatInterface;
    KWin::Wayland::ConnectionThread *m_connection;
    KWin::Wayland::Compositor *m_compositor;
    KWin::Wayland::Seat *m_seat;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-seat-0");

TestWaylandSeat::TestWaylandSeat(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_seatInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_seat(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandSeat::init()
{
    using namespace KWin::WaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    // setup connection
    m_connection = new KWin::Wayland::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, m_connection,
        [this]() {
            if (m_connection->display()) {
                wl_display_flush(m_connection->display());
            }
        }
    );

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    // TODO: we should destroy the queue
    wl_event_queue *queue = wl_display_create_queue(m_connection->display());
    connect(m_connection, &KWin::Wayland::ConnectionThread::eventsRead, this,
        [this, queue]() {
            wl_display_dispatch_queue_pending(m_connection->display(), queue);
            wl_display_flush(m_connection->display());
        },
        Qt::QueuedConnection);

    KWin::Wayland::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_proxy_set_queue((wl_proxy*)registry.registry(), queue);
    QVERIFY(compositorSpy.wait());

    m_seatInterface = m_display->createSeat();
    QVERIFY(m_seatInterface);
    m_seatInterface->setName(QStringLiteral("seat0"));
    m_seatInterface->create();
    QVERIFY(m_seatInterface->isValid());
    QVERIFY(seatSpy.wait());

    m_compositor = new KWin::Wayland::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());

    m_seat = new KWin::Wayland::Seat(this);
    QSignalSpy nameSpy(m_seat, SIGNAL(nameChanged(QString)));
    m_seat->setup(registry.bindSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>()));
    QVERIFY(nameSpy.wait());
}

void TestWaylandSeat::cleanup()
{
    if (m_seat) {
        delete m_seat;
        m_seat = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_seatInterface;
    m_seatInterface = nullptr;

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
    using namespace KWin::Wayland;
    using namespace KWin::WaylandServer;

    QSignalSpy pointerSpy(m_seat, SIGNAL(hasPointerChanged(bool)));
    QVERIFY(pointerSpy.isValid());
    m_seatInterface->setHasPointer(true);
    QVERIFY(pointerSpy.wait());

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWin::WaylandServer::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWin::WaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    PointerInterface *serverPointer = m_seatInterface->pointer();
    serverPointer->setGlobalPos(QPoint(20, 18));
    serverPointer->setFocusedSurface(serverSurface, QPoint(10, 15));
    // no pointer yet - won't be set
    QVERIFY(!serverPointer->focusedSurface());

    Pointer *p = m_seat->createPointer(m_seat);
    wl_display_flush(m_connection->display());
    QTest::qWait(100);

    QSignalSpy enteredSpy(p, SIGNAL(entered(quint32,QPointF)));
    QVERIFY(enteredSpy.isValid());

    QSignalSpy leftSpy(p, SIGNAL(left(quint32)));
    QVERIFY(leftSpy.isValid());

    QSignalSpy motionSpy(p, SIGNAL(motion(QPointF,quint32)));
    QVERIFY(motionSpy.isValid());

    QSignalSpy axisSpy(p, SIGNAL(axisChanged(quint32,KWin::Wayland::Pointer::Axis,qreal)));
    QVERIFY(axisSpy.isValid());

    QSignalSpy buttonSpy(p, SIGNAL(buttonStateChanged(quint32,quint32,quint32,KWin::Wayland::Pointer::ButtonState)));
    QVERIFY(buttonSpy.isValid());

    serverPointer->setFocusedSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(serverPointer->focusedSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));

    // test motion
    serverPointer->updateTimestamp(1);
    serverPointer->setGlobalPos(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // test axis
    serverPointer->updateTimestamp(2);
    serverPointer->axis(Qt::Horizontal, 10);
    QVERIFY(axisSpy.wait());
    serverPointer->updateTimestamp(3);
    serverPointer->axis(Qt::Vertical, 20);
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<Pointer::Axis>(), Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<Pointer::Axis>(), Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // test button
    serverPointer->updateTimestamp(4);
    serverPointer->buttonPressed(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), m_display->serial());
    serverPointer->updateTimestamp(5);
    serverPointer->buttonPressed(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), m_display->serial());
    serverPointer->updateTimestamp(6);
    serverPointer->buttonReleased(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_display->serial());
    serverPointer->updateTimestamp(7);
    serverPointer->buttonReleased(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.count(), 4);

    // timestamp
    QCOMPARE(buttonSpy.at(0).at(1).value<quint32>(), quint32(4));
    // button
    QCOMPARE(buttonSpy.at(0).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(0).at(3).value<KWin::Wayland::Pointer::ButtonState>(), KWin::Wayland::Pointer::ButtonState::Pressed);

    // timestamp
    QCOMPARE(buttonSpy.at(1).at(1).value<quint32>(), quint32(5));
    // button
    QCOMPARE(buttonSpy.at(1).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(1).at(3).value<KWin::Wayland::Pointer::ButtonState>(), KWin::Wayland::Pointer::ButtonState::Pressed);

    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), serverPointer->buttonSerial(2));
    // timestamp
    QCOMPARE(buttonSpy.at(2).at(1).value<quint32>(), quint32(6));
    // button
    QCOMPARE(buttonSpy.at(2).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(2).at(3).value<KWin::Wayland::Pointer::ButtonState>(), KWin::Wayland::Pointer::ButtonState::Released);

    QCOMPARE(buttonSpy.at(3).at(0).value<quint32>(), serverPointer->buttonSerial(1));
    // timestamp
    QCOMPARE(buttonSpy.at(3).at(1).value<quint32>(), quint32(7));
    // button
    QCOMPARE(buttonSpy.at(3).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(3).at(3).value<KWin::Wayland::Pointer::ButtonState>(), KWin::Wayland::Pointer::ButtonState::Released);

    // leave the surface
    serverPointer->setFocusedSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());

    // enter it again
    serverPointer->setFocusedSurface(serverSurface, QPoint(0, 0));
    QVERIFY(enteredSpy.wait());

    delete s;
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    QVERIFY(!serverPointer->focusedSurface());
}

void TestWaylandSeat::testKeyboard()
{
    using namespace KWin::Wayland;
    using namespace KWin::WaylandServer;

    QSignalSpy keyboardSpy(m_seat, SIGNAL(hasKeyboardChanged(bool)));
    QVERIFY(keyboardSpy.isValid());
    m_seatInterface->setHasKeyboard(true);
    QVERIFY(keyboardSpy.wait());

    // create the surface
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWin::WaylandServer::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());
    Surface *s = m_compositor->createSurface(m_compositor);
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWin::WaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    KeyboardInterface *serverKeyboard = m_seatInterface->keyboard();

    serverKeyboard->setFocusedSurface(serverSurface);
    // no pointer yet - won't be set
    QVERIFY(!serverKeyboard->focusedSurface());

    Keyboard *keyboard = m_seat->createKeyboard();
    QVERIFY(keyboard->isValid());
    wl_display_flush(m_connection->display());
    QTest::qWait(100);

    serverKeyboard->updateTimestamp(1);
    serverKeyboard->keyPressed(KEY_K);
    serverKeyboard->updateTimestamp(2);
    serverKeyboard->keyPressed(KEY_D);
    serverKeyboard->updateTimestamp(3);
    serverKeyboard->keyPressed(KEY_E);

    QSignalSpy modifierSpy(keyboard, SIGNAL(modifiersChanged(quint32,quint32,quint32,quint32)));
    QVERIFY(modifierSpy.isValid());

    // TODO: add a signalspy for enter
    serverKeyboard->setFocusedSurface(serverSurface);
    QCOMPARE(serverKeyboard->focusedSurface(), serverSurface);

    // we get the modifiers sent after the enter
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 1);
    QCOMPARE(modifierSpy.first().at(0).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(1).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(2).value<quint32>(), quint32(0));
    QCOMPARE(modifierSpy.first().at(3).value<quint32>(), quint32(0));

    QSignalSpy keyChangedSpy(keyboard, SIGNAL(keyChanged(quint32,KWin::Wayland::Keyboard::KeyState,quint32)));
    QVERIFY(keyChangedSpy.isValid());

    serverKeyboard->updateTimestamp(4);
    serverKeyboard->keyReleased(KEY_E);
    QVERIFY(keyChangedSpy.wait());
    serverKeyboard->updateTimestamp(5);
    serverKeyboard->keyReleased(KEY_D);
    QVERIFY(keyChangedSpy.wait());
    serverKeyboard->updateTimestamp(6);
    serverKeyboard->keyReleased(KEY_K);
    QVERIFY(keyChangedSpy.wait());
    serverKeyboard->updateTimestamp(7);
    serverKeyboard->keyPressed(KEY_F1);
    QVERIFY(keyChangedSpy.wait());
    serverKeyboard->updateTimestamp(8);
    serverKeyboard->keyReleased(KEY_F1);
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

    serverKeyboard->updateModifiers(1, 2, 3, 4);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(modifierSpy.count(), 2);
    QCOMPARE(modifierSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(modifierSpy.last().at(1).value<quint32>(), quint32(2));
    QCOMPARE(modifierSpy.last().at(2).value<quint32>(), quint32(3));
    QCOMPARE(modifierSpy.last().at(3).value<quint32>(), quint32(4));

    // TODO: add a test for leave signal
    serverKeyboard->setFocusedSurface(nullptr);
    QVERIFY(!serverKeyboard->focusedSurface());

    // enter it again
    serverKeyboard->setFocusedSurface(serverSurface);
    QVERIFY(modifierSpy.wait());
    QCOMPARE(serverKeyboard->focusedSurface(), serverSurface);

    delete s;
    wl_display_flush(m_connection->display());
    QTest::qWait(100);
    QVERIFY(!serverKeyboard->focusedSurface());
}

QTEST_MAIN(TestWaylandSeat)
#include "test_wayland_seat.moc"
