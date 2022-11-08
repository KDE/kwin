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
    KWaylandServer::Display *m_display = nullptr;
    KWaylandServer::CompositorInterface *m_compositorInterface = nullptr;
    KWaylandServer::XdgShellInterface *m_xdgShellInterface = nullptr;
    KWaylandServer::XdgDecorationManagerV1Interface *m_xdgDecorationManagerInterface = nullptr;

    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::XdgShell *m_xdgShell = nullptr;
    KWayland::Client::XdgDecorationManager *m_xdgDecorationManager = nullptr;

    QThread *m_thread = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-server-side-decoration-0");

TestXdgDecoration::TestXdgDecoration(QObject *parent)
    : QObject(parent)
{
}

void TestXdgDecoration::init()
{
    using namespace KWaylandServer;

    qRegisterMetaType<KWayland::Client::XdgDecoration::Mode>();
    qRegisterMetaType<XdgToplevelDecorationV1Interface::Mode>();

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
    QSignalSpy xdgShellSpy(m_registry, &KWayland::Client::Registry::xdgShellStableAnnounced);
    QSignalSpy xdgDecorationManagerSpy(m_registry, &KWayland::Client::Registry::xdgDecorationAnnounced);

    QVERIFY(!m_registry->eventQueue());
    m_registry->setEventQueue(m_queue);
    QCOMPARE(m_registry->eventQueue(), m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_xdgShellInterface = new XdgShellInterface(m_display, m_display);
    QVERIFY(xdgShellSpy.wait());
    m_xdgShell = m_registry->createXdgShell(xdgShellSpy.first().first().value<quint32>(), xdgShellSpy.first().last().value<quint32>(), this);

    m_xdgDecorationManagerInterface = new XdgDecorationManagerV1Interface(m_display, m_display);

    QVERIFY(xdgDecorationManagerSpy.wait());
    m_xdgDecorationManager = m_registry->createXdgDecorationManager(xdgDecorationManagerSpy.first().first().value<quint32>(),
                                                                    xdgDecorationManagerSpy.first().last().value<quint32>(),
                                                                    this);
}

void TestXdgDecoration::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_xdgShell) {
        delete m_xdgShell;
        m_xdgShell = nullptr;
    }
    if (m_xdgDecorationManager) {
        delete m_xdgDecorationManager;
        m_xdgDecorationManager = nullptr;
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

void TestXdgDecoration::testDecoration_data()
{
    using namespace KWaylandServer;
    QTest::addColumn<KWaylandServer::XdgToplevelDecorationV1Interface::Mode>("configuredMode");
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("configuredModeExp");
    QTest::addColumn<KWayland::Client::XdgDecoration::Mode>("setMode");
    QTest::addColumn<KWaylandServer::XdgToplevelDecorationV1Interface::Mode>("setModeExp");

    const auto serverClient = XdgToplevelDecorationV1Interface::Mode::Client;
    const auto serverServer = XdgToplevelDecorationV1Interface::Mode::Server;
    const auto clientClient = KWayland::Client::XdgDecoration::Mode::ClientSide;
    const auto clientServer = KWayland::Client::XdgDecoration::Mode::ServerSide;

    QTest::newRow("client->client") << serverClient << clientClient << clientClient << serverClient;
    QTest::newRow("client->server") << serverClient << clientClient << clientServer << serverServer;
    QTest::newRow("server->client") << serverServer << clientServer << clientClient << serverClient;
    QTest::newRow("server->server") << serverServer << clientServer << clientServer << serverServer;
}

void TestXdgDecoration::testDecoration()
{
    using namespace KWaylandServer;

    QFETCH(KWaylandServer::XdgToplevelDecorationV1Interface::Mode, configuredMode);
    QFETCH(KWayland::Client::XdgDecoration::Mode, configuredModeExp);
    QFETCH(KWayland::Client::XdgDecoration::Mode, setMode);
    QFETCH(KWaylandServer::XdgToplevelDecorationV1Interface::Mode, setModeExp);

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QSignalSpy shellSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::toplevelCreated);
    QSignalSpy decorationCreatedSpy(m_xdgDecorationManagerInterface, &XdgDecorationManagerV1Interface::decorationCreated);

    // create shell surface and deco object
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::XdgShellSurface> shellSurface(m_xdgShell->createSurface(surface.get()));
    std::unique_ptr<KWayland::Client::XdgDecoration> decoration(m_xdgDecorationManager->getToplevelDecoration(shellSurface.get()));

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

    QSignalSpy clientConfiguredSpy(decoration.get(), &KWayland::Client::XdgDecoration::modeChanged);
    QSignalSpy modeRequestedSpy(decorationIface, &XdgToplevelDecorationV1Interface::preferredModeChanged);

    // server configuring a client
    decorationIface->sendConfigure(configuredMode);
    quint32 serial = shellSurfaceIface->sendConfigure(QSize(0, 0), {});
    QVERIFY(clientConfiguredSpy.wait());
    QCOMPARE(clientConfiguredSpy.first().first().value<KWayland::Client::XdgDecoration::Mode>(), configuredModeExp);

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
