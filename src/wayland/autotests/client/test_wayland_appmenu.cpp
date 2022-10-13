/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/appmenu_interface.h"
#include "wayland/compositor_interface.h"
#include "wayland/display.h"

#include "KWayland/Client/appmenu.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

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
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositorInterface;
    std::unique_ptr<KWaylandServer::AppMenuManagerInterface> m_appmenuManagerInterface;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::AppMenuManager> m_appmenuManager;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<QThread> m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-appmenu-0");

TestAppmenu::TestAppmenu(QObject *parent)
    : QObject(parent)
{
}

void TestAppmenu::init()
{
    using namespace KWaylandServer;
    qRegisterMetaType<AppMenuInterface::InterfaceAddress>();
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

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    Registry registry;
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);

    QSignalSpy appmenuSpy(&registry, &Registry::appMenuAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue.get());
    QCOMPARE(registry.eventQueue(), m_queue.get());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor.reset(registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    m_appmenuManagerInterface = std::make_unique<AppMenuManagerInterface>(m_display.get());

    QVERIFY(appmenuSpy.wait());
    m_appmenuManager.reset(registry.createAppMenuManager(appmenuSpy.first().first().value<quint32>(), appmenuSpy.first().last().value<quint32>()));
}

void TestAppmenu::cleanup()
{
    m_compositor.reset();
    m_appmenuManager.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();
    m_compositorInterface.reset();
}

void TestAppmenu::testCreateAndSet()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy appMenuCreated(m_appmenuManagerInterface.get(), &KWaylandServer::AppMenuManagerInterface::appMenuCreated);

    QVERIFY(!m_appmenuManagerInterface->appMenuForSurface(serverSurface));

    auto appmenu = m_appmenuManager->create(surface.get(), surface.get());
    QVERIFY(appMenuCreated.wait());
    auto appMenuInterface = appMenuCreated.first().first().value<KWaylandServer::AppMenuInterface *>();
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
    delete appmenu;
    QVERIFY(destroyedSpy.wait());
    QVERIFY(!m_appmenuManagerInterface->appMenuForSurface(serverSurface));
}

QTEST_GUILESS_MAIN(TestAppmenu)
#include "test_wayland_appmenu.moc"
