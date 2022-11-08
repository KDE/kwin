/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/server_decoration_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/server_decoration.h"
#include "KWayland/Client/surface.h"

class TestServerSideDecoration : public QObject
{
    Q_OBJECT
public:
    explicit TestServerSideDecoration(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate_data();
    void testCreate();

    void testRequest_data();
    void testRequest();

    void testSurfaceDestroy();

private:
    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositorInterface = nullptr;
    KWaylandServer::ServerSideDecorationManagerInterface *m_serverSideDecorationManagerInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::ServerSideDecorationManager *m_serverSideDecorationManager = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-server-side-decoration-0");

TestServerSideDecoration::TestServerSideDecoration(QObject *parent)
    : QObject(parent)
{
}

void TestServerSideDecoration::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
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

    m_registry = new KWayland::Client::Registry();
    QSignalSpy compositorSpy(m_registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy serverSideDecoManagerSpy(m_registry, &KWayland::Client::Registry::serverSideDecorationManagerAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_serverSideDecorationManagerInterface = new ServerSideDecorationManagerInterface(m_display, m_display);

    QVERIFY(serverSideDecoManagerSpy.wait());
    m_serverSideDecorationManager = m_registry->createServerSideDecorationManager(serverSideDecoManagerSpy.first().first().value<quint32>(),
                                                                                  serverSideDecoManagerSpy.first().last().value<quint32>(),
                                                                                  this);
}

void TestServerSideDecoration::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_serverSideDecorationManager) {
        delete m_serverSideDecorationManager;
        m_serverSideDecorationManager = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
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

void TestServerSideDecoration::testCreate_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("serverMode");
    QTest::addColumn<KWayland::Client::ServerSideDecoration::Mode>("clientMode");

    QTest::newRow("none") << ServerSideDecorationManagerInterface::Mode::None << KWayland::Client::ServerSideDecoration::Mode::None;
    QTest::newRow("client") << ServerSideDecorationManagerInterface::Mode::Client << KWayland::Client::ServerSideDecoration::Mode::Client;
    QTest::newRow("server") << ServerSideDecorationManagerInterface::Mode::Server << KWayland::Client::ServerSideDecoration::Mode::Server;
}

void TestServerSideDecoration::testCreate()
{
    using namespace KWaylandServer;
    QFETCH(KWaylandServer::ServerSideDecorationManagerInterface::Mode, serverMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(serverMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), serverMode);

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    QVERIFY(!ServerSideDecorationInterface::get(serverSurface));

    // create server side deco
    std::unique_ptr<KWayland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), KWayland::Client::ServerSideDecoration::Mode::None);
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &KWayland::Client::ServerSideDecoration::modeChanged);

    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<ServerSideDecorationInterface *>();
    QVERIFY(serverDeco);
    QCOMPARE(serverDeco, ServerSideDecorationInterface::get(serverSurface));
    QCOMPARE(serverDeco->surface(), serverSurface);

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(serverSideDecoration->mode(), "clientMode");

    // and destroy
    QSignalSpy destroyedSpy(serverDeco, &QObject::destroyed);
    serverSideDecoration.reset();
    QVERIFY(destroyedSpy.wait());
}

void TestServerSideDecoration::testRequest_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("defaultMode");
    QTest::addColumn<KWayland::Client::ServerSideDecoration::Mode>("clientMode");
    QTest::addColumn<KWayland::Client::ServerSideDecoration::Mode>("clientRequestMode");
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("serverRequestedMode");

    const auto serverNone = ServerSideDecorationManagerInterface::Mode::None;
    const auto serverClient = ServerSideDecorationManagerInterface::Mode::Client;
    const auto serverServer = ServerSideDecorationManagerInterface::Mode::Server;
    const auto clientNone = KWayland::Client::ServerSideDecoration::Mode::None;
    const auto clientClient = KWayland::Client::ServerSideDecoration::Mode::Client;
    const auto clientServer = KWayland::Client::ServerSideDecoration::Mode::Server;

    QTest::newRow("none->none") << serverNone << clientNone << clientNone << serverNone;
    QTest::newRow("none->client") << serverNone << clientNone << clientClient << serverClient;
    QTest::newRow("none->server") << serverNone << clientNone << clientServer << serverServer;
    QTest::newRow("client->none") << serverClient << clientClient << clientNone << serverNone;
    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->none") << serverServer << clientServer << clientNone << serverNone;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestServerSideDecoration::testRequest()
{
    using namespace KWaylandServer;
    QFETCH(KWaylandServer::ServerSideDecorationManagerInterface::Mode, defaultMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(defaultMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), defaultMode);

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    // create server side deco
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), KWayland::Client::ServerSideDecoration::Mode::None);
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &KWayland::Client::ServerSideDecoration::modeChanged);
    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<ServerSideDecorationInterface *>();
    QVERIFY(serverDeco);
    QSignalSpy preferredModeChangedSpy(serverDeco, &ServerSideDecorationInterface::preferredModeChanged);

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(serverSideDecoration->mode(), "clientMode");

    // request a change
    QFETCH(KWayland::Client::ServerSideDecoration::Mode, clientRequestMode);
    serverSideDecoration->requestMode(clientRequestMode);
    // mode not yet changed
    QTEST(serverSideDecoration->mode(), "clientMode");

    QVERIFY(preferredModeChangedSpy.wait());
    QCOMPARE(preferredModeChangedSpy.count(), 1);
    QFETCH(ServerSideDecorationManagerInterface::Mode, serverRequestedMode);
    QCOMPARE(serverDeco->preferredMode(), serverRequestedMode);

    // mode not yet changed
    QCOMPARE(serverDeco->mode(), defaultMode);
    serverDeco->setMode(serverRequestedMode);
    QCOMPARE(serverDeco->mode(), serverRequestedMode);

    // should be sent to client
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 2);
    QCOMPARE(serverSideDecoration->mode(), clientRequestMode);
}

void TestServerSideDecoration::testSurfaceDestroy()
{
    using namespace KWaylandServer;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    std::unique_ptr<KWayland::Client::ServerSideDecoration> serverSideDecoration(m_serverSideDecorationManager->create(surface.get()));
    QCOMPARE(serverSideDecoration->mode(), KWayland::Client::ServerSideDecoration::Mode::None);
    QVERIFY(decorationCreated.wait());
    auto serverDeco = decorationCreated.first().first().value<ServerSideDecorationInterface *>();
    QVERIFY(serverDeco);

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QSignalSpy decorationDestroyedSpy(serverDeco, &QObject::destroyed);
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(decorationDestroyedSpy.isEmpty());
    // destroy the blur
    serverSideDecoration.reset();
    QVERIFY(decorationDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestServerSideDecoration)
#include "test_server_side_decoration.moc"
