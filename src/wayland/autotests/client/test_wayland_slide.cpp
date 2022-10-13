/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/slide_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/slide.h"
#include "KWayland/Client/surface.h"

using namespace KWayland::Client;

class TestSlide : public QObject
{
    Q_OBJECT
public:
    explicit TestSlide(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testSurfaceDestroy();

private:
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<KWaylandServer::CompositorInterface> m_compositorInterface;
    std::unique_ptr<KWaylandServer::SlideManagerInterface> m_slideManagerInterface;
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::Compositor> m_compositor;
    std::unique_ptr<KWayland::Client::SlideManager> m_slideManager;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<QThread> m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-slide-0");

TestSlide::TestSlide(QObject *parent)
    : QObject(parent)
{
}

void TestSlide::init()
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

    QSignalSpy slideSpy(&registry, &Registry::slideAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue.get());
    QCOMPARE(registry.eventQueue(), m_queue.get());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = std::make_unique<CompositorInterface>(m_display.get());
    QVERIFY(compositorSpy.wait());
    m_compositor.reset(registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));

    m_slideManagerInterface = std::make_unique<SlideManagerInterface>(m_display.get());

    QVERIFY(slideSpy.wait());
    m_slideManager.reset(registry.createSlideManager(slideSpy.first().first().value<quint32>(), slideSpy.first().last().value<quint32>()));
}

void TestSlide::cleanup()
{
    m_compositor.reset();
    m_slideManager.reset();
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_connection.reset();
    m_display.reset();
}

void TestSlide::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &KWaylandServer::CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<KWaylandServer::SurfaceInterface *>();
    QSignalSpy slideChanged(serverSurface, &KWaylandServer::SurfaceInterface::slideOnShowHideChanged);

    auto slide = m_slideManager->createSlide(surface.get(), surface.get());
    slide->setLocation(KWayland::Client::Slide::Location::Top);
    slide->setOffset(15);
    slide->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    QVERIFY(slideChanged.wait());
    QCOMPARE(serverSurface->slideOnShowHide()->location(), KWaylandServer::SlideInterface::Location::Top);
    QCOMPARE(serverSurface->slideOnShowHide()->offset(), 15);

    // and destroy
    QSignalSpy destroyedSpy(serverSurface->slideOnShowHide().data(), &QObject::destroyed);
    delete slide;
    QVERIFY(destroyedSpy.wait());
}

void TestSlide::testSurfaceDestroy()
{
    using namespace KWaylandServer;
    QSignalSpy serverSurfaceCreated(m_compositorInterface.get(), &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    QSignalSpy slideChanged(serverSurface, &SurfaceInterface::slideOnShowHideChanged);

    std::unique_ptr<Slide> slide(m_slideManager->createSlide(surface.get()));
    slide->commit();
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(slideChanged.wait());
    auto serverSlide = serverSurface->slideOnShowHide();
    QVERIFY(!serverSlide.isNull());

    // destroy the parent surface
    QSignalSpy surfaceDestroyedSpy(serverSurface, &QObject::destroyed);
    QSignalSpy slideDestroyedSpy(serverSlide.data(), &QObject::destroyed);
    surface.reset();
    QVERIFY(surfaceDestroyedSpy.wait());
    QVERIFY(slideDestroyedSpy.isEmpty());
    // destroy the slide
    slide.reset();
    QVERIFY(slideDestroyedSpy.wait());
}

QTEST_GUILESS_MAIN(TestSlide)
#include "test_wayland_slide.moc"
