/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/surface_interface.h"
#include "wayland/viewporter_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"

#include "qwayland-viewporter.h"

using namespace KWaylandServer;

class Viewporter : public QtWayland::wp_viewporter
{
};

class Viewport : public QtWayland::wp_viewport
{
};

class TestViewporterInterface : public QObject
{
    Q_OBJECT

public:
    ~TestViewporterInterface() override;

private Q_SLOTS:
    void initTestCase();
    void testCropScale();

private:
    std::unique_ptr<KWayland::Client::Registry> m_registry;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::Compositor> m_clientCompositor;
    std::unique_ptr<KWayland::Client::ShmPool> m_shm;

    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<CompositorInterface> m_serverCompositor;
    std::unique_ptr<KWaylandServer::ViewporterInterface> m_serverViewPorter;
    std::unique_ptr<Viewporter> m_viewporter;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-viewporter-test-0");

void TestViewporterInterface::initTestCase()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_display->createShm();
    m_serverViewPorter = std::make_unique<ViewporterInterface>(m_display.get());

    m_serverCompositor = std::make_unique<CompositorInterface>(m_display.get());

    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    m_registry = std::make_unique<KWayland::Client::Registry>();
    connect(m_registry.get(), &KWayland::Client::Registry::interfaceAnnounced, this, [this](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == QByteArrayLiteral("wp_viewporter")) {
            m_viewporter = std::make_unique<Viewporter>();
            m_viewporter->init(*m_registry.get(), id, version);
        }
    });
    QSignalSpy allAnnouncedSpy(m_registry.get(), &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(m_registry.get(), &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(m_registry.get(), &KWayland::Client::Registry::shmAnnounced);
    m_registry->setEventQueue(m_queue.get());
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor.reset(m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_clientCompositor->isValid());

    m_shm.reset(m_registry->createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>()));
    QVERIFY(m_shm->isValid());
}

TestViewporterInterface::~TestViewporterInterface()
{
    m_viewporter.reset();
    m_shm.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_clientCompositor.reset();
    m_registry.reset();
    m_connection.reset();
    m_display.reset();
}

void TestViewporterInterface::testCropScale()
{
    // Create a test surface.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor.get(), &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QSignalSpy serverSurfaceMappedSpy(serverSurface, &SurfaceInterface::mapped);
    QSignalSpy serverSurfaceSizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);
    QSignalSpy surfaceToBufferMatrixChangedSpy(serverSurface, &SurfaceInterface::surfaceToBufferMatrixChanged);

    // Map the surface.
    QImage image(QSize(200, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    KWayland::Client::Buffer::Ptr buffer = m_shm->createBuffer(image);
    clientSurface->attachBuffer(buffer);
    clientSurface->setScale(2);
    clientSurface->damage(image.rect());
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceMappedSpy.wait());
    QCOMPARE(surfaceToBufferMatrixChangedSpy.count(), 1);
    QCOMPARE(serverSurface->size(), QSize(100, 50));
    QCOMPARE(serverSurface->mapToBuffer(QPointF(0, 0)), QPointF(0, 0));

    // Create a viewport for the surface.
    std::unique_ptr<Viewport> clientViewport(new Viewport);
    clientViewport->init(m_viewporter->get_viewport(*clientSurface));

    // Crop the surface.
    clientViewport->set_source(wl_fixed_from_double(10), wl_fixed_from_double(10), wl_fixed_from_double(30), wl_fixed_from_double(20));
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceSizeChangedSpy.wait());
    QCOMPARE(surfaceToBufferMatrixChangedSpy.count(), 2);
    QCOMPARE(serverSurface->size(), QSize(30, 20));
    QCOMPARE(serverSurface->mapToBuffer(QPointF(0, 0)), QPointF(20, 20));

    // Scale the surface.
    clientViewport->set_destination(500, 250);
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceSizeChangedSpy.wait());
    QCOMPARE(surfaceToBufferMatrixChangedSpy.count(), 3);
    QCOMPARE(serverSurface->size(), QSize(500, 250));
    QCOMPARE(serverSurface->mapToBuffer(QPointF(0, 0)), QPointF(20, 20));

    // If the viewport is destroyed, the crop and scale state will be unset on a next commit.
    clientViewport->destroy();
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceSizeChangedSpy.wait());
    QCOMPARE(surfaceToBufferMatrixChangedSpy.count(), 4);
    QCOMPARE(serverSurface->size(), QSize(100, 50));
    QCOMPARE(serverSurface->mapToBuffer(QPointF(0, 0)), QPointF(0, 0));
}

QTEST_GUILESS_MAIN(TestViewporterInterface)

#include "test_viewporter_interface.moc"
