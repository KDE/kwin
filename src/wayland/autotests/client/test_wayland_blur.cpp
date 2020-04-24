/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/region.h"
#include "../../src/client/registry.h"
#include "../../src/client/surface.h"
#include "../../src/client/blur.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/region_interface.h"
#include "../../src/server/blur_interface.h"

using namespace KWayland::Client;

class TestBlur : public QObject
{
    Q_OBJECT
public:
    explicit TestBlur(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSurfaceDestroy();
    void testGlobalDestroy();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::BlurManagerInterface *m_blurManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::BlurManager *m_blurManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-blur-0");

TestBlur::TestBlur(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestBlur::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
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

    QSignalSpy blurSpy(&registry, &Registry::blurAnnounced);
    QVERIFY(blurSpy.isValid());

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_blurManagerInterface = m_display->createBlurManager(m_display);
    m_blurManagerInterface->create();
    QVERIFY(m_blurManagerInterface->isValid());

    QVERIFY(blurSpy.wait());
    m_blurManager = registry.createBlurManager(blurSpy.first().first().value<quint32>(), blurSpy.first().last().value<quint32>(), this);
}

void TestBlur::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_blurManager)
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
    CLEANUP(m_blurManagerInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void TestBlur::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QSignalSpy blurChanged(serverSurface, SIGNAL(blurChanged()));

    auto blur = m_blurManager->createBlur(surface.data(), surface.data());
    blur->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    blur->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(blurChanged.wait());
    QCOMPARE(serverSurface->blur()->region(), QRegion(0, 0, 10, 20));

    // and destroy
    QSignalSpy destroyedSpy(serverSurface->blur().data(), &QObject::destroyed);
    QVERIFY(destroyedSpy.isValid());
    delete blur;
    QVERIFY(destroyedSpy.wait());
}

void TestBlur::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWayland::Server::CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());

    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QSignalSpy blurChanged(serverSurface, &KWayland::Server::SurfaceInterface::blurChanged);
    QVERIFY(blurChanged.isValid());

    QScopedPointer<KWayland::Client::Blur> blur(m_blurManager->createBlur(surface.data()));
    blur->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    blur->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(blurChanged.wait());
    QCOMPARE(serverSurface->blur()->region(), QRegion(0, 0, 10, 20));

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QVERIFY(surfaceDestroyedSpy.isValid());
    QSignalSpy blurDestroyedSpy(serverSurface->blur().data(), &QObject::destroyed);
    QVERIFY(blurDestroyedSpy.isValid());
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(blurDestroyedSpy.isEmpty());
    // destroy the blur
    blur.reset();
    QVERIFY(blurDestroyedSpy.wait());
}


void TestBlur::testGlobalDestroy()
{
    Registry registry;
    QSignalSpy blurAnnouncedSpy(&registry, &Registry::blurAnnounced);
    QSignalSpy blurRemovedSpy(&registry, &Registry::blurRemoved);

    registry.setEventQueue(m_queue);

    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(blurAnnouncedSpy.wait());

    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWayland::Server::CompositorInterface::surfaceCreated);
    QScopedPointer<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWayland::Server::SurfaceInterface*>();
    QSignalSpy blurChanged(serverSurface, &KWayland::Server::SurfaceInterface::blurChanged);

    m_blurManagerInterface->remove();

    // we've deleted our global, but the clients have some operations in flight as they haven't processed this yet
    // when we process them, we should not throw an error and kill the client

    QScopedPointer<KWayland::Client::Blur> blur(m_blurManager->createBlur(surface.data()));
    blur->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    blur->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // client finally sees the blur is gone
    QVERIFY(blurRemovedSpy.wait());
}

QTEST_GUILESS_MAIN(TestBlur)
#include "test_wayland_blur.moc"
