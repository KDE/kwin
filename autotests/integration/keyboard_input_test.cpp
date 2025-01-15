/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "config-kwin.h"

#include "utils/socketpair.h"
#include "wayland/display.h"
#include "wayland_server.h"
#include "workspace.h"

#if KWIN_BUILD_GLOBALSHORTCUTS
#include <KGlobalAccel>
#endif

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>

#include <linux/input-event-codes.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_kbd_input-0");

struct Connection
{
    static std::unique_ptr<Connection> create()
    {
        auto socketPair = SocketPair::create(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (!socketPair) {
            return nullptr;
        }

        waylandServer()->display()->createClient(socketPair->fds[0].take());

        auto connection = std::make_unique<Connection>();
        connection->connection = new KWayland::Client::ConnectionThread;
        QSignalSpy connectedSpy(connection->connection, &KWayland::Client::ConnectionThread::connected);
        connection->connection->setSocketFd(socketPair->fds[1].take());

        connection->thread = new QThread(kwinApp());
        connection->connection->moveToThread(connection->thread);
        connection->thread->start();

        connection->connection->initConnection();
        if (!connectedSpy.wait()) {
            return nullptr;
        }

        connection->queue = new KWayland::Client::EventQueue;
        connection->queue->setup(connection->connection);
        if (!connection->queue->isValid()) {
            return nullptr;
        }

        connection->registry = new KWayland::Client::Registry;
        connection->registry->setEventQueue(connection->queue);

        QObject::connect(connection->registry, &KWayland::Client::Registry::interfaceAnnounced, [&](const QByteArray &interface, quint32 name, quint32 version) {
            if (interface == wl_compositor_interface.name) {
                connection->compositor = connection->registry->createCompositor(name, version);
            } else if (interface == wl_shm_interface.name) {
                connection->shm = connection->registry->createShmPool(name, version);
            } else if (interface == wl_seat_interface.name) {
                connection->seat = connection->registry->createSeat(name, version);
            } else if (interface == xdg_wm_base_interface.name) {
                connection->xdgShell = new Test::XdgShell();
                connection->xdgShell->init(*connection->registry, name, version);
            }
        });

        connection->registry->create(connection->connection);
        if (!connection->registry->isValid()) {
            return nullptr;
        }

        connection->registry->setup();
        QSignalSpy allAnnounced(connection->registry, &KWayland::Client::Registry::interfacesAnnounced);
        if (!allAnnounced.wait()) {
            return nullptr;
        }

        return connection;
    }

    ~Connection()
    {
        delete compositor;
        compositor = nullptr;

        delete seat;
        seat = nullptr;

        delete xdgShell;
        xdgShell = nullptr;

        delete shm;
        shm = nullptr;

        delete registry;
        registry = nullptr;

        delete queue; // Must be destroyed last

        if (thread) {
            connection->deleteLater();
            thread->quit();
            thread->wait();
            delete thread;
        }
    }

    QThread *thread = nullptr;
    KWayland::Client::ConnectionThread *connection = nullptr;
    KWayland::Client::EventQueue *queue = nullptr;
    KWayland::Client::Registry *registry = nullptr;
    Test::XdgShell *xdgShell = nullptr;
    KWayland::Client::Compositor *compositor = nullptr;
    KWayland::Client::Seat *seat = nullptr;
    KWayland::Client::ShmPool *shm = nullptr;
};

class KeyboardInputTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void implicitGrab();
    void implicitGrabByClosedWindow();
    void globalShortcut();

private:
    std::unique_ptr<Connection> m_firstConnection;
    std::unique_ptr<KWayland::Client::Keyboard> m_firstKeyboard;
    std::unique_ptr<KWayland::Client::Surface> m_firstSurface;
    std::unique_ptr<Test::XdgToplevel> m_firstShellSurface;
    QPointer<Window> m_firstWindow;

    std::unique_ptr<Connection> m_secondConnection;
    std::unique_ptr<KWayland::Client::Keyboard> m_secondKeyboard;
    std::unique_ptr<KWayland::Client::Surface> m_secondSurface;
    std::unique_ptr<Test::XdgToplevel> m_secondShellSurface;
    QPointer<Window> m_secondWindow;
};

void KeyboardInputTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->start();
}

void KeyboardInputTest::init()
{
    m_firstConnection = Connection::create();
    QVERIFY(Test::waitForWaylandKeyboard(m_firstConnection->seat));
    m_firstKeyboard = std::unique_ptr<KWayland::Client::Keyboard>(m_firstConnection->seat->createKeyboard());
    m_firstSurface = Test::createSurface(m_firstConnection->compositor);
    m_firstShellSurface = Test::createXdgToplevelSurface(m_firstConnection->xdgShell, m_firstSurface.get());
    m_firstWindow = Test::renderAndWaitForShown(m_firstConnection->shm, m_firstSurface.get(), QSize(100, 100), Qt::cyan);
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(firstEnteredSpy.wait());

    m_secondConnection = Connection::create();
    QVERIFY(Test::waitForWaylandKeyboard(m_secondConnection->seat));
    m_secondKeyboard = std::unique_ptr<KWayland::Client::Keyboard>(m_secondConnection->seat->createKeyboard());
    m_secondSurface = Test::createSurface(m_secondConnection->compositor);
    m_secondShellSurface = Test::createXdgToplevelSurface(m_secondConnection->xdgShell, m_secondSurface.get());
    m_secondWindow = Test::renderAndWaitForShown(m_secondConnection->shm, m_secondSurface.get(), QSize(100, 100), Qt::cyan);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QVERIFY(secondEnteredSpy.wait());
}

void KeyboardInputTest::cleanup()
{
    m_firstShellSurface.reset();
    m_firstSurface.reset();
    m_firstKeyboard.reset();
    m_firstConnection.reset();

    m_secondShellSurface.reset();
    m_secondSurface.reset();
    m_secondKeyboard.reset();
    m_secondConnection.reset();
}

void KeyboardInputTest::implicitGrab()
{
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_Q, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    QCOMPARE(secondKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(secondKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    workspace()->activateWindow(m_firstWindow);
    QVERIFY(firstEnteredSpy.wait()); // TODO: perhaps we should not receive the enter event until the q key is released
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_Q}));

    Test::keyboardKeyReleased(KEY_Q, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
    QCOMPARE(firstKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(firstKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
}

void KeyboardInputTest::implicitGrabByClosedWindow()
{
    // This test verifies that an implicit grab is preserved even after the window is closed. Note:
    // currently it is not the case, but it should be.

    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_Q, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    QCOMPARE(secondKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(secondKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Pressed);

    m_secondShellSurface.reset();
    m_secondSurface.reset();
    QVERIFY(firstEnteredSpy.wait()); // TODO: perhaps we should not receive the enter event until the q key is released
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_Q}));

    Test::keyboardKeyReleased(KEY_Q, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
    QCOMPARE(firstKeyChangedSpy.last().at(0).value<quint32>(), KEY_Q);
    QCOMPARE(firstKeyChangedSpy.last().at(1).value<KWayland::Client::Keyboard::KeyState>(), KWayland::Client::Keyboard::KeyState::Released);
}

void KeyboardInputTest::globalShortcut()
{
    // This test verifies that keys are not leaked to the clients when pressing a global shortcut.

#if KWIN_BUILD_GLOBALSHORTCUTS
    QSignalSpy firstEnteredSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy firstKeyChangedSpy(m_firstKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    QSignalSpy secondEnteredSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy secondKeyChangedSpy(m_secondKeyboard.get(), &KWayland::Client::Keyboard::keyChanged);

    auto action = std::make_unique<QAction>();
    action->setObjectName(QStringLiteral("test"));
    action->setProperty("componentName", QStringLiteral("test"));
    KGlobalAccel::self()->setShortcut(action.get(), QList<QKeySequence>{Qt::META | Qt::Key_Space}, KGlobalAccel::NoAutoloading);
    QSignalSpy actionTriggeredSpy(action.get(), &QAction::triggered);

    // the client should not see the space key being pressed or released
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    Test::keyboardKeyPressed(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    QCOMPARE(actionTriggeredSpy.count(), 1);
    Test::keyboardKeyReleased(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());

    // the space key should not be leaked even if the focused surface changes between pressing and releasing the space key
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(secondKeyChangedSpy.wait());
    Test::keyboardKeyPressed(KEY_SPACE, timestamp++);
    QVERIFY(!secondKeyChangedSpy.wait(10));
    QCOMPARE(actionTriggeredSpy.count(), 2);
    m_secondShellSurface.reset();
    m_secondSurface.reset();
    QVERIFY(firstEnteredSpy.wait());
    QCOMPARE(m_firstKeyboard->enteredKeys(), (QList<quint32>{KEY_LEFTMETA}));
    Test::keyboardKeyReleased(KEY_SPACE, timestamp++);
    QVERIFY(!firstKeyChangedSpy.wait(10));
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(firstKeyChangedSpy.wait());
#endif
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::KeyboardInputTest)
#include "keyboard_input_test.moc"
