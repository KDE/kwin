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
#include "../../src/client/event_queue.h"
#include "../../src/client/keyboard.h"
#include "../../src/client/pointer.h"
#include "../../src/client/surface.h"
#include "../../src/client/registry.h"
#include "../../src/client/seat.h"
#include "../../src/client/shm_pool.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/keyboard_interface.h"
#include "../../src/server/pointer_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/surface_interface.h"
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
    void testPointerButton_data();
    void testPointerButton();
    void testKeyboard();
    void testCast();
    void testDestroy();
    // TODO: add test for keymap

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::SeatInterface *m_seatInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::Seat *m_seat;
    KWayland::Client::EventQueue *m_queue;
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

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
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

    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QSignalSpy seatSpy(&registry, SIGNAL(seatAnnounced(quint32,quint32)));
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

    m_seatInterface->setPointerPos(QPoint(20, 18));
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    // no pointer yet
    QVERIFY(m_seatInterface->focusedPointerSurface());
    QVERIFY(!m_seatInterface->focusedPointer());

    Pointer *p = m_seat->createPointer(m_seat);
    QVERIFY(p->isValid());
    QSignalSpy pointerCreatedSpy(m_seatInterface, SIGNAL(pointerCreated(KWayland::Server::PointerInterface*)));
    QVERIFY(pointerCreatedSpy.isValid());
    // once the pointer is created it should be set as the focused pointer
    QVERIFY(pointerCreatedSpy.wait());
    QVERIFY(m_seatInterface->focusedPointer());
    QCOMPARE(pointerCreatedSpy.first().first().value<PointerInterface*>(), m_seatInterface->focusedPointer());

    m_seatInterface->setFocusedPointerSurface(nullptr);
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

    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(10, 15));
    QCOMPARE(m_seatInterface->focusedPointerSurface(), serverSurface);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().first().value<quint32>(), m_display->serial());
    QCOMPARE(enteredSpy.first().last().toPoint(), QPoint(10, 3));
    PointerInterface *serverPointer = m_seatInterface->focusedPointer();
    QVERIFY(serverPointer);

    // test motion
    m_seatInterface->setTimestamp(1);
    serverPointer->setGlobalPos(QPoint(10, 16));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.first().first().toPoint(), QPoint(0, 1));
    QCOMPARE(motionSpy.first().last().value<quint32>(), quint32(1));

    // test axis
    m_seatInterface->setTimestamp(2);
    serverPointer->axis(Qt::Horizontal, 10);
    QVERIFY(axisSpy.wait());
    m_seatInterface->setTimestamp(3);
    serverPointer->axis(Qt::Vertical, 20);
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.first().at(0).value<quint32>(), quint32(2));
    QCOMPARE(axisSpy.first().at(1).value<Pointer::Axis>(), Pointer::Axis::Horizontal);
    QCOMPARE(axisSpy.first().at(2).value<qreal>(), qreal(10));

    QCOMPARE(axisSpy.last().at(0).value<quint32>(), quint32(3));
    QCOMPARE(axisSpy.last().at(1).value<Pointer::Axis>(), Pointer::Axis::Vertical);
    QCOMPARE(axisSpy.last().at(2).value<qreal>(), qreal(20));

    // test button
    m_seatInterface->setTimestamp(4);
    serverPointer->buttonPressed(1);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(0).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(5);
    serverPointer->buttonPressed(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(1).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(6);
    serverPointer->buttonReleased(2);
    QVERIFY(buttonSpy.wait());
    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), m_display->serial());
    m_seatInterface->setTimestamp(7);
    serverPointer->buttonReleased(1);
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

    QCOMPARE(buttonSpy.at(2).at(0).value<quint32>(), serverPointer->buttonSerial(2));
    // timestamp
    QCOMPARE(buttonSpy.at(2).at(1).value<quint32>(), quint32(6));
    // button
    QCOMPARE(buttonSpy.at(2).at(2).value<quint32>(), quint32(2));
    QCOMPARE(buttonSpy.at(2).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);

    QCOMPARE(buttonSpy.at(3).at(0).value<quint32>(), serverPointer->buttonSerial(1));
    // timestamp
    QCOMPARE(buttonSpy.at(3).at(1).value<quint32>(), quint32(7));
    // button
    QCOMPARE(buttonSpy.at(3).at(2).value<quint32>(), quint32(1));
    QCOMPARE(buttonSpy.at(3).at(3).value<KWayland::Client::Pointer::ButtonState>(), KWayland::Client::Pointer::ButtonState::Released);

    // leave the surface
    m_seatInterface->setFocusedPointerSurface(nullptr);
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.first().first().value<quint32>(), m_display->serial());

    // enter it again
    m_seatInterface->setFocusedPointerSurface(serverSurface, QPoint(0, 0));
    QVERIFY(enteredSpy.wait());

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
    QSignalSpy buttonChangedSpy(p.data(), SIGNAL(buttonStateChanged(quint32, quint32, quint32, KWayland::Client::Pointer::ButtonState)));
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
    QCOMPARE(serverPointer->isButtonPressed(waylandButton), false);
    QCOMPARE(serverPointer->isButtonPressed(qtButton), false);
    m_seatInterface->setTimestamp(msec);
    serverPointer->buttonPressed(qtButton);
    QCOMPARE(serverPointer->isButtonPressed(waylandButton), true);
    QCOMPARE(serverPointer->isButtonPressed(qtButton), true);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 1);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), serverPointer->buttonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), serverPointer->buttonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), Pointer::ButtonState::Pressed);
    msec = QDateTime::currentMSecsSinceEpoch();
    m_seatInterface->setTimestamp(QDateTime::currentMSecsSinceEpoch());
    serverPointer->buttonReleased(qtButton);
    QCOMPARE(serverPointer->isButtonPressed(waylandButton), false);
    QCOMPARE(serverPointer->isButtonPressed(qtButton), false);
    QVERIFY(buttonChangedSpy.wait());
    QCOMPARE(buttonChangedSpy.count(), 2);
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), serverPointer->buttonSerial(waylandButton));
    QCOMPARE(buttonChangedSpy.last().at(0).value<quint32>(), serverPointer->buttonSerial(qtButton));
    QCOMPARE(buttonChangedSpy.last().at(1).value<quint32>(), msec);
    QCOMPARE(buttonChangedSpy.last().at(2).value<quint32>(), waylandButton);
    QCOMPARE(buttonChangedSpy.last().at(3).value<KWayland::Client::Pointer::ButtonState>(), Pointer::ButtonState::Released);
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

    KeyboardInterface *serverKeyboard = m_seatInterface->keyboard();

    serverKeyboard->setFocusedSurface(serverSurface);
    // no pointer yet - won't be set
    QVERIFY(!serverKeyboard->focusedSurface());

    Keyboard *keyboard = m_seat->createKeyboard(m_seat);
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

    QSignalSpy keyChangedSpy(keyboard, SIGNAL(keyChanged(quint32,KWayland::Client::Keyboard::KeyState,quint32)));
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

    delete m_compositor;
    m_compositor = nullptr;
    connect(m_connection, &ConnectionThread::connectionDied, m_seat, &Seat::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);
    QVERIFY(m_seat->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    m_compositorInterface = nullptr;
    m_seatInterface = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the seat should be destroyed;
    QVERIFY(!m_seat->isValid());
    QVERIFY(!k->isValid());
    QVERIFY(!p->isValid());

    // calling destroy again should not fail
    m_seat->destroy();
    k->destroy();
    p->destroy();
}

QTEST_GUILESS_MAIN(TestWaylandSeat)
#include "test_wayland_seat.moc"
