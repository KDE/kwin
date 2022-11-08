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
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::SlideManagerInterface *m_slideManagerInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::SlideManager *m_slideManager;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-slide-0");

TestSlide::TestSlide(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSlide::init()
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

    QSignalSpy slideSpy(&registry, &KWayland::Client::Registry::slideAnnounced);

    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    QVERIFY(compositorSpy.wait());
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_slideManagerInterface = new SlideManagerInterface(m_display, m_display);

    QVERIFY(slideSpy.wait());
    m_slideManager = registry.createSlideManager(slideSpy.first().first().value<quint32>(), slideSpy.first().last().value<quint32>(), this);
}

void TestSlide::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_compositor)
    CLEANUP(m_slideManager)
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
    m_slideManagerInterface = nullptr;
}

void TestSlide::testCreate()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);
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
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());

    auto serverSurface = serverSurfaceCreated.first().first().value<SurfaceInterface *>();
    QSignalSpy slideChanged(serverSurface, &SurfaceInterface::slideOnShowHideChanged);

    std::unique_ptr<KWayland::Client::Slide> slide(m_slideManager->createSlide(surface.get()));
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
