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
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/server/display.h"
// Wayland
#include <wayland-client-protocol.h>
// system
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

class TestWaylandConnectionThread : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandConnectionThread(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testInitConnectionNoThread();
    void testConnectionFailure();
    void testConnectionDieing();
    void testConnectionThread();
    void testConnectFd();
    void testConnectFdNoSocketName();

private:
    KWayland::Server::Display *m_display;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-connection-0");

TestWaylandConnectionThread::TestWaylandConnectionThread(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
{
}

void TestWaylandConnectionThread::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
}

void TestWaylandConnectionThread::cleanup()
{
    delete m_display;
    m_display = nullptr;
}

void TestWaylandConnectionThread::testInitConnectionNoThread()
{
    QScopedPointer<KWayland::Client::ConnectionThread> connection(new KWayland::Client::ConnectionThread);
    QCOMPARE(connection->socketName(), QStringLiteral("wayland-0"));
    connection->setSocketName(s_socketName);
    QCOMPARE(connection->socketName(), s_socketName);

    QSignalSpy connectedSpy(connection.data(), SIGNAL(connected()));
    QSignalSpy failedSpy(connection.data(), SIGNAL(failed()));
    connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(connection->display());
}

void TestWaylandConnectionThread::testConnectionFailure()
{
    QScopedPointer<KWayland::Client::ConnectionThread> connection(new KWayland::Client::ConnectionThread);
    connection->setSocketName(QStringLiteral("kwin-test-socket-does-not-exist"));

    QSignalSpy connectedSpy(connection.data(), SIGNAL(connected()));
    QSignalSpy failedSpy(connection.data(), SIGNAL(failed()));
    connection->initConnection();
    QVERIFY(failedSpy.wait());
    QCOMPARE(connectedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(!connection->display());
}

static void registryHandleGlobal(void *data, struct wl_registry *registry,
                                 uint32_t name, const char *interface, uint32_t version)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
    Q_UNUSED(interface)
    Q_UNUSED(version)
}

static void registryHandleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

static const struct wl_registry_listener s_registryListener = {
    registryHandleGlobal,
    registryHandleGlobalRemove
};

void TestWaylandConnectionThread::testConnectionThread()
{
    KWayland::Client::ConnectionThread *connection = new KWayland::Client::ConnectionThread;
    connection->setSocketName(s_socketName);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();

    QSignalSpy connectedSpy(connection, SIGNAL(connected()));
    QVERIFY(connectedSpy.isValid());
    QSignalSpy failedSpy(connection, SIGNAL(failed()));
    QVERIFY(failedSpy.isValid());
    connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(connection->display());

    // now we have the connection ready, let's get some events
    QSignalSpy eventsSpy(connection, SIGNAL(eventsRead()));
    QVERIFY(eventsSpy.isValid());
    wl_display *display = connection->display();
    QScopedPointer<KWayland::Client::EventQueue> queue(new KWayland::Client::EventQueue);
    queue->setup(display);
    QVERIFY(queue->isValid());
    connect(connection, &KWayland::Client::ConnectionThread::eventsRead, queue.data(), &KWayland::Client::EventQueue::dispatch, Qt::QueuedConnection);

    wl_registry *registry = wl_display_get_registry(display);
    wl_proxy_set_queue((wl_proxy*)registry, *(queue.data()));

    wl_registry_add_listener(registry, &s_registryListener, this);
    wl_display_flush(display);

    if (eventsSpy.isEmpty()) {
        QVERIFY(eventsSpy.wait());
    }
    QVERIFY(!eventsSpy.isEmpty());

    wl_registry_destroy(registry);
    queue.reset();

    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;
}

void TestWaylandConnectionThread::testConnectionDieing()
{
    QScopedPointer<KWayland::Client::ConnectionThread> connection(new KWayland::Client::ConnectionThread);
    QSignalSpy connectedSpy(connection.data(), SIGNAL(connected()));
    connection->setSocketName(s_socketName);
    connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(connection->display());

    QSignalSpy diedSpy(connection.data(), SIGNAL(connectionDied()));
    m_display->terminate();
    QVERIFY(!m_display->isRunning());
    QVERIFY(diedSpy.wait());
    QCOMPARE(diedSpy.count(), 1);
    QVERIFY(!connection->display());

    connectedSpy.clear();
    QVERIFY(connectedSpy.isEmpty());
    // restarts the server
    m_display->start();
    QVERIFY(m_display->isRunning());
    if (connectedSpy.count() == 0) {
        QVERIFY(connectedSpy.wait());
    }
    QCOMPARE(connectedSpy.count(), 1);
}

void TestWaylandConnectionThread::testConnectFd()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    auto c = m_display->createClient(sv[0]);
    QVERIFY(c);
    QSignalSpy disconnectedSpy(c, &ClientConnection::disconnected);
    QVERIFY(disconnectedSpy.isValid());

    ConnectionThread *connection = new ConnectionThread;
    QSignalSpy connectedSpy(connection, SIGNAL(connected()));
    QVERIFY(connectedSpy.isValid());
    connection->setSocketFd(sv[1]);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();
    connection->initConnection();
    QVERIFY(connectedSpy.wait());

    // create the Registry
    QScopedPointer<Registry> registry(new Registry);
    QSignalSpy announcedSpy(registry.data(), SIGNAL(interfacesAnnounced()));
    QVERIFY(announcedSpy.isValid());
    registry->create(connection);
    QScopedPointer<EventQueue> queue(new EventQueue);
    queue->setup(connection);
    registry->setEventQueue(queue.data());
    registry->setup();
    QVERIFY(announcedSpy.wait());

    registry.reset();
    queue.reset();
    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;

    c->destroy();
    QCOMPARE(disconnectedSpy.count(), 1);
}

void TestWaylandConnectionThread::testConnectFdNoSocketName()
{
    delete m_display;
    m_display = nullptr;
    using namespace KWayland::Client;
    using namespace KWayland::Server;

    Display display;
    display.start(Display::StartMode::ConnectClientsOnly);
    QVERIFY(display.isRunning());

    int sv[2];
    QVERIFY(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) >= 0);
    QVERIFY(display.createClient(sv[0]));

    ConnectionThread *connection = new ConnectionThread;
    QSignalSpy connectedSpy(connection, SIGNAL(connected()));
    QVERIFY(connectedSpy.isValid());
    connection->setSocketFd(sv[1]);

    QThread *connectionThread = new QThread(this);
    connection->moveToThread(connectionThread);
    connectionThread->start();
    connection->initConnection();
    QVERIFY(connectedSpy.wait());

    // create the Registry
    QScopedPointer<Registry> registry(new Registry);
    QSignalSpy announcedSpy(registry.data(), SIGNAL(interfacesAnnounced()));
    QVERIFY(announcedSpy.isValid());
    registry->create(connection);
    QScopedPointer<EventQueue> queue(new EventQueue);
    queue->setup(connection);
    registry->setEventQueue(queue.data());
    registry->setup();
    QVERIFY(announcedSpy.wait());

    registry.reset();
    queue.reset();
    connection->deleteLater();
    connectionThread->quit();
    connectionThread->wait();
    delete connectionThread;
}

QTEST_GUILESS_MAIN(TestWaylandConnectionThread)
#include "test_wayland_connection_thread.moc"
