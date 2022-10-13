/*
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/xdgdecoration_v1_interface.h"
#include "wayland/xdgshell_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgdecoration.h"
#include "KWayland/Client/xdgshell.h"

class TestXdgDecoration : public QObject
{
    Q_OBJECT
public:
    explicit TestXdgDecoration(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testDecoration_data();
    void testDecoration();

private:
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositorInterface;
    std::unique_ptr<KWaylandServer::XdgShellInterface> m_xdgShellInterface;
    std::unique_ptr<KWaylandServer::XdgDecorationManagerV1Interface> m_xdgDecorationManagerInterface;

    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::XdgShell> m_xdgShell;
    std::unique_ptr<KWayland::Client::XdgDecorationManager> m_xdgDecorationManager;
    std::unique_ptr<KWayland::Client::Registry> m_registry;

    std::unique_ptr<QThread> m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-server-side-decoration-0");

TestXdgDecoration::TestXdgDecoration(QObject *parent)
    : QObject(parent)
{
}

void TestXdgDecoration::init()
{
    using namespace KWaylandServer;
    using namespace KWayland::Client;

    qRegisterMetaType<XdgDecoration::Mode>();
    qRegisterMetaType<XdgToplevelDecorationV1Interface::Mode>();

    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = std::make_unique<EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    m_registry = std::make_unique<Registry>();
    QSignalSpy compositorSpy(m_registry.get(), &Registry::compositorAnnounced);
    QSignalSpy xdgShellSpy(m_registry.get(), &Registry::xdgShellStableAnnounced);
    QSignalSpy xdgDecorationManagerSpy(m_registry.get(), &Registry::xdgDecorationAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue.get());
    QCOMPARE(m_registry->eventQueue(), m_queue.get());
    m_registry->create(m_connection.get());
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor.reset(m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    m_xdgShellInterface = std::make_unique<XdgShellInterface>(m_display.get());
    QVERIFY(xdgShellSpy.wait());
    m_xdgShell.reset(m_registry->createXdgShell(xdgShellSpy.first().first().value<quint32>(), xdgShellSpy.first().last().value<quint32>()));

    m_xdgDecorationManagerInterface = std::make_unique<XdgDecorationManagerV1Interface>(m_display.get());

    QVERIFY(xdgDecorationManagerSpy.wait());
    m_xdgDecorationManager.reset(m_registry->createXdgDecorationManager(xdgDecorationManagerSpy.first().first().value<quint32>(),
                                                                        xdgDecorationManagerSpy.first().last().value<quint32>()));
}

void TestXdgDecoration::cleanup()
{
    m_compositor.reset();
    m_xdgShell.reset();
    m_xdgDecorationManager.reset();
    m_queue.reset();
    m_registry.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();

    m_display.reset();
}

void TestXdgDecoration::testDecoration_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWaylandServer::XdgToplevelDecorationV1Interface::Mode>("configuredMode");
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("configuredModeExp");
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("setMode");
    QTest::addColumn<KWaylandServer::XdgToplevelDecorationV1Interface::Mode>("setModeExp");

    const auto serverClient = XdgToplevelDecorationV1Interface::Mode::Client;
    const auto serverServer = XdgToplevelDecorationV1Interface::Mode::Server;
    const auto clientClient = XdgDecoration::Mode::ClientSide;
    const auto clientServer = XdgDecoration::Mode::ServerSide;

    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestXdgDecoration::testDecoration()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QFETCH(KWaylandServer::XdgToplevelDecorationV1Interface::Mode, configuredMode);
    QFETCH(KWayland::Client::XdgDecoration::Mode, configuredModeExp);
    QFETCH(KWayland::Client::XdgDecoration::Mode, setMode);
    QFETCH(KWaylandServer::XdgToplevelDecorationV1Interface::Mode, setModeExp);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface.get(), &CompositorInterface::surfaceCreated);
    QSignalSpy shellSurfaceCreatedSpy(m_xdgShellInterface.get(), &XdgShellInterface::toplevelCreated);
    QSignalSpy decorationCreatedSpy(m_xdgDecorationManagerInterface.get(), &XdgDecorationManagerV1Interface::decorationCreated);

    // create shell surface and deco object
    std::unique_ptr<Surface> surface(m_compositor->createSurface());
    std::unique_ptr<XdgShellSurface> shellSurface(m_xdgShell->createSurface(surface.get()));
    std::unique_ptr<XdgDecoration> decoration(m_xdgDecorationManager->getToplevelDecoration(shellSurface.get()));

    // and receive all these on the "server"
    QVERIFY(surfaceCreatedSpy.count() || surfaceCreatedSpy.wait());
    QVERIFY(shellSurfaceCreatedSpy.count() || shellSurfaceCreatedSpy.wait());
    QVERIFY(decorationCreatedSpy.count() || decorationCreatedSpy.wait());

    auto shellSurfaceIface = shellSurfaceCreatedSpy.first().first().value<XdgToplevelInterface *>();
    auto decorationIface = decorationCreatedSpy.first().first().value<XdgToplevelDecorationV1Interface *>();

    QVERIFY(decorationIface);
    QVERIFY(shellSurfaceIface);
    QCOMPARE(decorationIface->toplevel(), shellSurfaceIface);
    QCOMPARE(decorationIface->preferredMode(), XdgToplevelDecorationV1Interface::Mode::Undefined);

    QSignalSpy clientConfiguredSpy(decoration.get(), &XdgDecoration::modeChanged);
    QSignalSpy modeRequestedSpy(decorationIface, &XdgToplevelDecorationV1Interface::preferredModeChanged);

    // server configuring a client
    decorationIface->sendConfigure(configuredMode);
    quint32 serial = shellSurfaceIface->sendConfigure(QSize(0, 0), {});
    QVERIFY(clientConfiguredSpy.wait());
    QCOMPARE(clientConfiguredSpy.first().first().value<XdgDecoration::Mode>(), configuredModeExp);

    shellSurface->ackConfigure(serial);

    // client requesting another mode
    decoration->setMode(setMode);
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(modeRequestedSpy.first().first().value<XdgToplevelDecorationV1Interface::Mode>(), setModeExp);
    QCOMPARE(decorationIface->preferredMode(), setModeExp);
    modeRequestedSpy.clear();

    decoration->unsetMode();
    QVERIFY(modeRequestedSpy.wait());
    QCOMPARE(modeRequestedSpy.first().first().value<XdgToplevelDecorationV1Interface::Mode>(), XdgToplevelDecorationV1Interface::Mode::Undefined);
}

QTEST_GUILESS_MAIN(TestXdgDecoration)
#include "test_xdg_decoration.moc"
