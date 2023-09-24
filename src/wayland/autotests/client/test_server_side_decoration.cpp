/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QSignalSpy>
#include <QTest>
// KWin
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/server_decoration.h"

#include "qwayland-server-decoration.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

class ServerSideDecorationManager : public QtWayland::org_kde_kwin_server_decoration_manager
{
};

class ServerSideDecoration : public QObject, public QtWayland::org_kde_kwin_server_decoration
{
    Q_OBJECT

public:
    ~ServerSideDecoration() override
    {
        release();
    }

Q_SIGNALS:
    void modeChanged(ServerSideDecorationManager::mode mode);

protected:
    void org_kde_kwin_server_decoration_mode(uint32_t mode) override
    {
        Q_EMIT modeChanged(ServerSideDecorationManager::mode(mode));
    }
};

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
    KWin::Display *m_display = nullptr;
    KWin::CompositorInterface *m_compositorInterface = nullptr;
    KWin::ServerSideDecorationManagerInterface *m_serverSideDecorationManagerInterface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    ServerSideDecorationManager *m_serverSideDecorationManager = nullptr;
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

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_serverSideDecorationManagerInterface = new ServerSideDecorationManagerInterface(m_display, m_display);

    m_registry = new KWayland::Client::Registry();
    connect(m_registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this](const QByteArray &interfaceName, quint32 name, quint32 version) {
        if (interfaceName == org_kde_kwin_server_decoration_manager_interface.name) {
            m_serverSideDecorationManager = new ServerSideDecorationManager();
            m_serverSideDecorationManager->init(*m_registry, name, version);
        }
    });

    QSignalSpy interfacesAnnouncedSpy(m_registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy compositorSpy(m_registry, &KWayland::Client::Registry::compositorAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    QVERIFY(m_compositor);
    QVERIFY(m_serverSideDecorationManager);
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
    using namespace KWin;
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("serverMode");
    QTest::addColumn<ServerSideDecorationManager::mode>("clientMode");

    QTest::newRow("none") << ServerSideDecorationManagerInterface::Mode::None << ServerSideDecorationManager::mode_None;
    QTest::newRow("client") << ServerSideDecorationManagerInterface::Mode::Client << ServerSideDecorationManager::mode_Client;
    QTest::newRow("server") << ServerSideDecorationManagerInterface::Mode::Server << ServerSideDecorationManager::mode_Server;
}

void TestServerSideDecoration::testCreate()
{
    using namespace KWin;
    QFETCH(KWin::ServerSideDecorationManagerInterface::Mode, serverMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(serverMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), serverMode);

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    QVERIFY(!ServerSideDecorationInterface::get(serverSurface));

    // create server side deco
    auto serverSideDecoration = std::make_unique<ServerSideDecoration>();
    serverSideDecoration->init(m_serverSideDecorationManager->create(*surface.get()));
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &ServerSideDecoration::modeChanged);

    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<ServerSideDecorationInterface *>();
    QVERIFY(serverDeco);
    QCOMPARE(serverDeco, ServerSideDecorationInterface::get(serverSurface));
    QCOMPARE(serverDeco->surface(), serverSurface);

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(modeChangedSpy.last().at(0).value<ServerSideDecorationManager::mode>(), "clientMode");

    // and destroy
    QSignalSpy destroyedSpy(serverDeco, &QObject::destroyed);
    serverSideDecoration.reset();
    QVERIFY(destroyedSpy.wait());
}

void TestServerSideDecoration::testRequest_data()
{
    using namespace KWin;
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("defaultMode");
    QTest::addColumn<ServerSideDecorationManager::mode>("clientMode");
    QTest::addColumn<ServerSideDecorationManager::mode>("clientRequestMode");
    QTest::addColumn<ServerSideDecorationManagerInterface::Mode>("serverRequestedMode");

    const auto serverNone = ServerSideDecorationManagerInterface::Mode::None;
    const auto serverClient = ServerSideDecorationManagerInterface::Mode::Client;
    const auto serverServer = ServerSideDecorationManagerInterface::Mode::Server;
    const auto clientNone = ServerSideDecorationManager::mode_None;
    const auto clientClient = ServerSideDecorationManager::mode_Client;
    const auto clientServer = ServerSideDecorationManager::mode_Server;

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
    using namespace KWin;
    QFETCH(KWin::ServerSideDecorationManagerInterface::Mode, defaultMode);
    m_serverSideDecorationManagerInterface->setDefaultMode(defaultMode);
    QCOMPARE(m_serverSideDecorationManagerInterface->defaultMode(), defaultMode);

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    // create server side deco
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());

    auto serverSideDecoration = std::make_unique<ServerSideDecoration>();
    serverSideDecoration->init(m_serverSideDecorationManager->create(*surface.get()));
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &ServerSideDecoration::modeChanged);
    QVERIFY(decorationCreated.wait());

    auto serverDeco = decorationCreated.first().first().value<ServerSideDecorationInterface *>();
    QVERIFY(serverDeco);
    QSignalSpy preferredModeChangedSpy(serverDeco, &ServerSideDecorationInterface::preferredModeChanged);

    // after binding the client should get the default mode
    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 1);
    QTEST(modeChangedSpy.last().at(0).value<ServerSideDecorationManager::mode>(), "clientMode");

    // request a change
    QFETCH(ServerSideDecorationManager::mode, clientRequestMode);
    serverSideDecoration->request_mode(clientRequestMode);
    // mode not yet changed
    QCOMPARE(modeChangedSpy.count(), 1);

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
    QCOMPARE(modeChangedSpy.last().at(0).value<ServerSideDecorationManager::mode>(), clientRequestMode);
}

void TestServerSideDecoration::testSurfaceDestroy()
{
    using namespace KWin;
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy decorationCreated(m_serverSideDecorationManagerInterface, &ServerSideDecorationManagerInterface::decorationCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    auto serverSideDecoration = std::make_unique<ServerSideDecoration>();
    serverSideDecoration->init(m_serverSideDecorationManager->create(*surface.get()));
    QSignalSpy modeChangedSpy(serverSideDecoration.get(), &ServerSideDecoration::modeChanged);
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
