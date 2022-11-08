/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/contrast_interface.h"
#include "wayland/display.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/contrast.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

#include <wayland-util.h>

class TestContrast : public QObject
{
    Q_OBJECT
public:
    explicit TestContrast(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSurfaceDestroy();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::ContrastManagerInterface *m_contrastManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::ContrastManager *m_contrastManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-contrast-0");

TestContrast::TestContrast(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestContrast::init()
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

    KWayland::Client::Registry registry;
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);

    QSignalSpy contrastSpy(&registry, &KWayland::Client::Registry::contrastAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_contrastManagerInterface = new ContrastManagerInterface(m_display, m_display);

    QVERIFY(contrastSpy.wait());
    m_contrastManager = registry.createContrastManager(contrastSpy.first().first().value<quint32>(), contrastSpy.first().last().value<quint32>(), this);
}

void TestContrast::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_contrastManager)
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
    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_compositorInterface = nullptr;
    m_contrastManagerInterface = nullptr;
}

void TestContrast::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy contrastChanged(serverSurface, &KWaylandServer::SurfaceInterface::contrastChanged);

    auto contrast = m_contrastManager->createContrast(surface.get(), surface.get());
    contrast->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));

    contrast->setContrast(0.2);
    contrast->setIntensity(2.0);
    contrast->setSaturation(1.7);

    contrast->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(contrastChanged.wait());
    QCOMPARE(serverSurface->contrast()->region(), QRegion(0, 0, 10, 20));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->contrast()), wl_fixed_from_double(0.2));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->intensity()), wl_fixed_from_double(2.0));
    QCOMPARE(wl_fixed_from_double(serverSurface->contrast()->saturation()), wl_fixed_from_double(1.7));

    // and destroy
    QSignalSpy destroyedSpy(serverSurface->contrast().data(), &QObject::destroyed);
    delete contrast;
    QVERIFY(destroyedSpy.wait());
}

void TestContrast::testSurfaceDestroy()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy contrastChanged(serverSurface, &KWaylandServer::SurfaceInterface::contrastChanged);

    std::unique_ptr<KWayland::Client::Contrast> contrast(m_contrastManager->createContrast(surface.get()));
    contrast->setRegion(m_compositor->createRegion(QRegion(0, 0, 10, 20), nullptr));
    contrast->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(contrastChanged.wait());
    QCOMPARE(serverSurface->contrast()->region(), QRegion(0, 0, 10, 20));

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QSignalSpy contrastDestroyedSpy(serverSurface->contrast().data(), &QObject::destroyed);
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(contrastDestroyedSpy.isEmpty());
    // destroy the blur
    contrast.reset();
    QVERIFY(contrastDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestContrast)
#include "test_wayland_contrast.moc"
