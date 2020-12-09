/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/appmenu.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/region_interface.h"
#include "../../src/server/appmenu_interface.h"

using namespace KWayland::Client;

Q_DECLARE_METATYPE(KWaylandServer::AppMenuInterface::InterfaceAddress)

class TestAppmenu : public QObject
{
    Q_OBJECT
public:
    explicit TestAppmenu(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreateAndSet();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::AppMenuManagerInterface *m_appmenuManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::AppMenuManager *m_appmenuManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-appmenu-0");

TestAppmenu::TestAppmenu(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestAppmenu::init()
{
    using namespace KWaylandServer;
    qRegisterMetaType<AppMenuInterface::InterfaceAddress>();
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
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

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QVERIFY(compositorSpy.isValid());

    QSignalSpy appmenuSpy(&registry, &Registry::appMenuAnnounced);
    QVERIFY(appmenuSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_appmenuManagerInterface = new AppMenuManagerInterface(m_display, m_display);

    QVERIFY(appmenuSpy.wait());
    m_appmenuManager = registry.createAppMenuManager(appmenuSpy.first().first().value<quint32>(), appmenuSpy.first().last().value<quint32>(), this);
}

void TestAppmenu::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_appmenuManager)
    CLEANUP(m_queue)
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
    CLEANUP(m_compositorInterface)
    CLEANUP(m_appmenuManagerInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestAppmenu::testCreateAndSet()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface*>();
    QSignalSpy appMenuCreated(m_appmenuManagerInterface, &KWaylandServer::AppMenuManagerInterface::appMenuCreated);

    QVERIFY(!m_appmenuManagerInterface->appMenuForSurface(serverSurface));

    auto appmenu = m_appmenuManager->create(surface.data(), surface.data());
    QVERIFY(appMenuCreated.wait());
    auto appMenuInterface = appMenuCreated.first().first().value<KWaylandServer::AppMenuInterface*>();
    QCOMPARE(m_appmenuManagerInterface->appMenuForSurface(serverSurface), appMenuInterface);

    QCOMPARE(appMenuInterface->address().serviceName, QString());
    QCOMPARE(appMenuInterface->address().objectPath, QString());

    QSignalSpy appMenuChangedSpy(appMenuInterface, &KWaylandServer::AppMenuInterface::addressChanged);

    appmenu->setAddress("net.somename", "/test/path");

    QVERIFY(appMenuChangedSpy.wait());
    QCOMPARE(appMenuInterface->address().serviceName, QString("net.somename"));
    QCOMPARE(appMenuInterface->address().objectPath, QString("/test/path"));

    // and destroy
    QSignalSpy destroyedSpy(appMenuInterface, &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete appmenu;
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_appmenuManagerInterface->appMenuForSurface(serverSurface));
}

QTEST_GUILESS_MAIN(TestAppmenu)
#include "test_wayland_appmenu.moc"
