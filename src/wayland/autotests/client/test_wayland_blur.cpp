/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/blur_interface.h"
#include "wayland/compositor_interface.h"
#include "wayland/display.h"

#include "KWayland/Client/blur.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

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

private:
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositorInterface;
    std::unique_ptr<KWaylandServer::BlurManagerInterface> m_blurManagerInterface;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::BlurManager> m_blurManager;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<QThread> m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-blur-0");

TestBlur::TestBlur(QObject *parent)
    : QObject(parent)
{
}

void TestBlur::init()
{
    using namespace KWaylandServer;
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

    QSignalSpy blurSpy(&registry, &Registry::blurAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue.get());
    QCOMPARE(registry.eventQueue(), m_queue.get());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor.reset(registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    m_blurManagerInterface = std::make_unique<BlurManagerInterface>(m_display.get());
    QVERIFY(blurSpy.wait());
    m_blurManager.reset(registry.createBlurManager(blurSpy.first().first().value<quint32>(), blurSpy.first().last().value<quint32>()));
}

void TestBlur::cleanup()
{
    m_compositor.reset();
    m_blurManager.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();

    m_display.reset();
}

void TestBlur::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy blurChanged(serverSurface, &KWaylandServer::SurfaceInterface::blurChanged);

    auto blur = m_blurManager->createBlur(surface.get(), surface.get());
    blur->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    blur->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(blurChanged.wait());
    QCOMPARE(serverSurface->blur()->region(), QRegion(0, 0, 10, 20));

    // and destroy
    QSignalSpy destroyedSpy(serverSurface->blur().data(), &QObject::destroyed);
    delete blur;
    QVERIFY(destroyedSpy.wait());
}

void TestBlur::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy blurChanged(serverSurface, &KWaylandServer::SurfaceInterface::blurChanged);

    std::unique_ptr<KWayland::Client::Blur> blur(m_blurManager->createBlur(surface.get()));
    blur->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    blur->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(blurChanged.wait());
    QCOMPARE(serverSurface->blur()->region(), QRegion(0, 0, 10, 20));

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QSignalSpy blurDestroyedSpy(serverSurface->blur().data(), &QObject::destroyed);
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(blurDestroyedSpy.isEmpty());
    // destroy the blur
    blur.reset();
    QVERIFY(blurDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestBlur)
#include "test_wayland_blur.moc"
