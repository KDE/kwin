/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/viewporter_interface.h"

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
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::ShmPool *m_shm;

    QThread *m_thread;
    Display m_display;
    CompositorInterface *m_serverCompositor;
    Viewporter *m_viewporter;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-viewporter-test-0");

void TestViewporterInterface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_display.createShm();
    new ViewporterInterface(&m_display);

    m_serverCompositor = new CompositorInterface(&m_display, this);

    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == QByteArrayLiteral("wp_viewporter")) {
            m_viewporter = new Viewporter();
            m_viewporter->init(*registry, id, version);
        }
    });
    QSignalSpy allAnnouncedSpy(registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(registry, &KWayland::Client::Registry::shmAnnounced);
    registry->setEventQueue(m_queue);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                    compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());

    m_shm = registry->createShmPool(shmSpy.first().first().value<quint32>(),
                                    shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
}

TestViewporterInterface::~TestViewporterInterface()
{
    if (m_viewporter) {
        delete m_viewporter;
        m_viewporter = nullptr;
    }
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestViewporterInterface::testCropScale()
{
    // Create a test surface.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QSignalSpy serverSurfaceMappedSpy(serverSurface, &SurfaceInterface::mapped);
    QVERIFY(serverSurfaceMappedSpy.isValid());
    QSignalSpy serverSurfaceSizeChangedSpy(serverSurface, &SurfaceInterface::sizeChanged);
    QVERIFY(serverSurfaceSizeChangedSpy.isValid());
    QSignalSpy surfaceToBufferMatrixChangedSpy(serverSurface, &SurfaceInterface::surfaceToBufferMatrixChanged);
    QVERIFY(surfaceToBufferMatrixChangedSpy.isValid());

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
    QScopedPointer<Viewport> clientViewport(new Viewport);
    clientViewport->init(m_viewporter->get_viewport(*clientSurface));

    // Crop the surface.
    clientViewport->set_source(wl_fixed_from_double(10), wl_fixed_from_double(10),
                               wl_fixed_from_double(30), wl_fixed_from_double(20));
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
