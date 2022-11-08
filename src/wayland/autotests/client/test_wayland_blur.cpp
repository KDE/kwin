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
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::BlurManagerInterface *m_blurManagerInterface;
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

    QSignalSpy blurSpy(&registry, &KWayland::Client::Registry::blurAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_blurManagerInterface = new BlurManagerInterface(m_display, m_display);
    QVERIFY(blurSpy.wait());
    m_blurManager = registry.createBlurManager(blurSpy.first().first().value<quint32>(), blurSpy.first().last().value<quint32>(), this);
}

void TestBlur::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
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
    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_compositorInterface = nullptr;
    m_blurManagerInterface = nullptr;
}

void TestBlur::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);

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
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);

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
