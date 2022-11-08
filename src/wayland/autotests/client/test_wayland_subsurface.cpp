/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/surface_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/subcompositor.h"
#include "KWayland/Client/subsurface.h"
#include "KWayland/Client/surface.h"

// Wayland
#include <wayland-client.h>

Q_DECLARE_METATYPE(KWayland::Client::SubSurface::Mode)

class TestSubSurface : public QObject
{
    Q_OBJECT
public:
    explicit TestSubSurface(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testCreate();
    void testMode();
    void testPosition_data();
    void testPosition();
    void testPlaceAbove();
    void testPlaceBelow();
    void testSyncMode();
    void testDeSyncMode();
    void testMainSurfaceFromTree();
    void testRemoveSurface();
    void testMappingOfSurfaceTree();
    void testSurfaceAt();
    void testDestroyAttachedBuffer();
    void testDestroyParentSurface();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::CompositorInterface *m_compositorInterface;
    KWaylandServer::SubCompositorInterface *m_subcompositorInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
    KWayland::Client::ShmPool *m_shm;
    KWayland::Client::SubCompositor *m_subCompositor;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-subsurface-0");

TestSubSurface::TestSubSurface(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_subcompositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_shm(nullptr)
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSubSurface::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();

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
    QSignalSpy subCompositorSpy(&registry, &KWayland::Client::Registry::subCompositorAnnounced);
    QVERIFY(!registry.eventQueue());
    registry.setEventQueue(m_queue);
    QCOMPARE(registry.eventQueue(), m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_subcompositorInterface = new SubCompositorInterface(m_display, m_display);
    QVERIFY(m_subcompositorInterface);

    QVERIFY(subCompositorSpy.wait());
    m_subCompositor = registry.createSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>(), this);

    if (compositorSpy.isEmpty()) {
        QVERIFY(compositorSpy.wait());
    }
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);

    m_shm = registry.createShmPool(registry.interface(KWayland::Client::Registry::Interface::Shm).name,
                                   registry.interface(KWayland::Client::Registry::Interface::Shm).version,
                                   this);
    QVERIFY(m_shm->isValid());
}

void TestSubSurface::cleanup()
{
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
    }
    if (m_subCompositor) {
        delete m_subCompositor;
        m_subCompositor = nullptr;
    }
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
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
    delete m_connection;
    m_connection = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestSubSurface::testCreate()
{
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &KWaylandServer::CompositorInterface::surfaceCreated);

    // create two Surfaces
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverSurface);

    surfaceCreatedSpy.clear();
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverParentSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(serverParentSurface);

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, &KWaylandServer::SubCompositorInterface::subSurfaceCreated);

    // create subSurface for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(m_subCompositor->createSubSurface(surface.get(), parent.get()));

    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface);
    QVERIFY(serverSubSurface->parentSurface());
    QCOMPARE(serverSubSurface->parentSurface(), serverParentSurface);
    QCOMPARE(serverSubSurface->surface(), serverSurface);
    QCOMPARE(serverSurface->subSurface(), serverSubSurface);
    QCOMPARE(serverSubSurface->mainSurface(), serverParentSurface);
    // children are only added after committing the surface
    QCOMPARE(serverParentSurface->below().count(), 0);
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QCOMPARE(serverParentSurface->above().count(), 0);
    // so let's commit the surface, to apply the stacking change
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->below().count(), 0);
    QCOMPARE(serverParentSurface->above().count(), 1);
    QCOMPARE(serverParentSurface->above().constFirst(), serverSubSurface);

    // and let's destroy it again
    QSignalSpy destroyedSpy(serverSubSurface, &QObject::destroyed);
    subSurface.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(serverSurface->subSurface(), QPointer<SubSurfaceInterface>());
    // only applied after next commit
    QCOMPARE(serverParentSurface->below().count(), 0);
    QEXPECT_FAIL("", "Incorrect removing of child windows to workaround QtWayland behavior", Continue);
    QCOMPARE(serverParentSurface->above().count(), 1);
    // but the surface should be invalid
    if (!serverParentSurface->above().isEmpty()) {
        QVERIFY(!serverParentSurface->above().constFirst());
    }
    // committing the state should solve it
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->below().count(), 0);
    QCOMPARE(serverParentSurface->above().count(), 0);
}

void TestSubSurface::testMode()
{
    using namespace KWaylandServer;
    // create two Surface
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, &KWaylandServer::SubCompositorInterface::subSurfaceCreated);

    // create the SubSurface for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(m_subCompositor->createSubSurface(surface.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface);

    // both client and server subsurface should be in synchronized mode
    QCOMPARE(subSurface->mode(), KWayland::Client::SubSurface::Mode::Synchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Synchronized);

    // verify that we can change to desynchronized
    QSignalSpy modeChangedSpy(serverSubSurface, &KWaylandServer::SubSurfaceInterface::modeChanged);

    subSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    QCOMPARE(subSurface->mode(), KWayland::Client::SubSurface::Mode::Desynchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.first().first().value<KWaylandServer::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Desynchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Desynchronized);

    // setting the same again won't change
    subSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    QCOMPARE(subSurface->mode(), KWayland::Client::SubSurface::Mode::Desynchronized);
    // not testing the signal, we do that after changing to synchronized

    // and change back to synchronized
    subSurface->setMode(KWayland::Client::SubSurface::Mode::Synchronized);
    QCOMPARE(subSurface->mode(), KWayland::Client::SubSurface::Mode::Synchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.count(), 2);
    QCOMPARE(modeChangedSpy.first().first().value<KWaylandServer::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Desynchronized);
    QCOMPARE(modeChangedSpy.last().first().value<KWaylandServer::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Synchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Synchronized);
}

void TestSubSurface::testPosition_data()
{
    QTest::addColumn<KWayland::Client::SubSurface::Mode>("commitMode");

    QTest::addRow("sync") << KWayland::Client::SubSurface::Mode::Synchronized;
    QTest::addRow("desync") << KWayland::Client::SubSurface::Mode::Desynchronized;
}

void TestSubSurface::testPosition()
{
    using namespace KWaylandServer;
    // create two Surface
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, &KWaylandServer::SubCompositorInterface::subSurfaceCreated);

    // create the SubSurface for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(m_subCompositor->createSubSurface(surface.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface);

    // put the subsurface in the desired commit mode
    QFETCH(KWayland::Client::SubSurface::Mode, commitMode);
    subSurface->setMode(commitMode);

    // both client and server should have a default position
    QCOMPARE(subSurface->position(), QPoint());
    QCOMPARE(serverSubSurface->position(), QPoint());

    QSignalSpy positionChangedSpy(serverSubSurface, &KWaylandServer::SubSurfaceInterface::positionChanged);

    // changing the position should not trigger a direct update on server side
    subSurface->setPosition(QPoint(10, 20));
    QCOMPARE(subSurface->position(), QPoint(10, 20));
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface->position(), QPoint());
    // changing once more
    subSurface->setPosition(QPoint(20, 30));
    QCOMPARE(subSurface->position(), QPoint(20, 30));
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface->position(), QPoint());

    // committing the parent surface should update the position
    QSignalSpy parentCommittedSpy(serverSubSurface->parentSurface(), &SurfaceInterface::committed);
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(parentCommittedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QCOMPARE(positionChangedSpy.first().first().toPoint(), QPoint(20, 30));
    QCOMPARE(serverSubSurface->position(), QPoint(20, 30));
}

void TestSubSurface::testPlaceAbove()
{
    using namespace KWaylandServer;
    // create needed Surfaces (one parent, three client
    std::unique_ptr<KWayland::Client::Surface> surface1(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> surface3(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, &KWaylandServer::SubCompositorInterface::subSurfaceCreated);

    // create the SubSurfaces for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface1(m_subCompositor->createSubSurface(surface1.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    std::unique_ptr<KWayland::Client::SubSurface> subSurface2(m_subCompositor->createSubSurface(surface2.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    std::unique_ptr<KWayland::Client::SubSurface> subSurface3(m_subCompositor->createSubSurface(surface3.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QVERIFY(serverSubSurface1->parentSurface()->below().isEmpty());
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QVERIFY(serverSubSurface1->parentSurface()->above().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 0);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface3);

    // raising subsurface1 should place it to top of stack
    subSurface1->placeAbove(subSurface3.get());
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface1);
    // after commit it's changed
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface1);

    // try placing 3 above 1, should result in 2, 1, 3
    subSurface3->placeAbove(subSurface1.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface3);

    // try placing 3 above 2, should result in 2, 3, 1
    subSurface3->placeAbove(subSurface2.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface1);

    // try placing 1 above 3 - shouldn't change
    subSurface1->placeAbove(subSurface3.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface1);

    // and 2 above 3 - > 3, 2, 1
    subSurface2->placeAbove(subSurface3.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface1);
}

void TestSubSurface::testPlaceBelow()
{
    using namespace KWaylandServer;
    // create needed Surfaces (one parent, three client
    std::unique_ptr<KWayland::Client::Surface> surface1(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> surface2(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> surface3(m_compositor->createSurface());
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, &KWaylandServer::SubCompositorInterface::subSurfaceCreated);

    // create the SubSurfaces for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface1(m_subCompositor->createSubSurface(surface1.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    std::unique_ptr<KWayland::Client::SubSurface> subSurface2(m_subCompositor->createSubSurface(surface2.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    std::unique_ptr<KWayland::Client::SubSurface> subSurface3(m_subCompositor->createSubSurface(surface3.get(), parent.get()));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface *>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QVERIFY(serverSubSurface1->parentSurface()->below().isEmpty());
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QVERIFY(serverSubSurface1->parentSurface()->above().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(2), serverSubSurface3);

    // lowering subsurface3 should place it to the bottom of stack
    subSurface3->lower();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface1);
    // after commit it's changed
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 1);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(1), serverSubSurface2);

    // place 1 below 3 -> 1, 3, 2
    subSurface1->placeBelow(subSurface3.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 2);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 1);
    QCOMPARE(serverSubSurface1->parentSurface()->above().at(0), serverSubSurface2);

    // 2 below 3 -> 1, 2, 3
    subSurface2->placeBelow(subSurface3.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(2), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 0);

    // 1 below 2 -> shouldn't change
    subSurface1->placeBelow(subSurface2.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(2), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 0);

    // and 3 below 1 -> 3, 1, 2
    subSurface3->placeBelow(subSurface1.get());
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->below().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(1), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->below().at(2), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->above().count(), 0);
}

void TestSubSurface::testSyncMode()
{
    // this test verifies that state is only applied when the parent surface commits its pending state
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(childSurface);

    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(parentSurface);
    // create subSurface for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(m_subCompositor->createSubSurface(surface.get(), parent.get()));

    // let's damage the child surface
    QSignalSpy childDamagedSpy(childSurface, &SurfaceInterface::damaged);

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit();

    // state should be applied when the parent surface's state gets applied
    QVERIFY(!childDamagedSpy.wait(100));
    QVERIFY(!childSurface->buffer());

    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    QImage image2(QSize(400, 400), QImage::Format_ARGB32_Premultiplied);
    image2.fill(Qt::red);
    parent->attachBuffer(m_shm->createBuffer(image2));
    parent->damage(QRect(0, 0, 400, 400));
    parent->commit();
    QVERIFY(childDamagedSpy.wait());
    QCOMPARE(childDamagedSpy.count(), 1);
    QVERIFY(childSurface->isMapped());
    QVERIFY(parentSurface->isMapped());

    // sending frame rendered to parent should also send it to child
    QSignalSpy frameRenderedSpy(surface.get(), &KWayland::Client::Surface::frameRendered);
    parentSurface->frameRendered(100);
    QVERIFY(frameRenderedSpy.wait());
}

void TestSubSurface::testDeSyncMode()
{
    // this test verifies that state gets applied immediately in desync mode
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(childSurface);

    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(parentSurface);
    // create subSurface for surface of parent
    std::unique_ptr<KWayland::Client::SubSurface> subSurface(m_subCompositor->createSubSurface(surface.get(), parent.get()));

    // let's damage the child surface
    QSignalSpy childDamagedSpy(childSurface, &SurfaceInterface::damaged);

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // state should be applied when the parent surface's state gets applied or when the subsurface switches to desync
    QVERIFY(!childDamagedSpy.wait(100));
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // setting to desync should apply the state directly
    subSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    QVERIFY(childDamagedSpy.wait());
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // and damaging again, should directly be applied
    image.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(childDamagedSpy.wait());
}

void TestSubSurface::testMainSurfaceFromTree()
{
    // this test verifies that in a tree of surfaces every surface has the same main surface
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel1ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel1ServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel2ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel2ServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel3ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel3ServerSurface);

    m_subCompositor->createSubSurface(childLevel1Surface.get(), parentSurface.get());
    m_subCompositor->createSubSurface(childLevel2Surface.get(), childLevel1Surface.get());
    m_subCompositor->createSubSurface(childLevel3Surface.get(), childLevel2Surface.get());

    QSignalSpy parentCommittedSpy(parentServerSurface, &SurfaceInterface::committed);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(parentCommittedSpy.wait());

    QCOMPARE(parentServerSurface->below().count(), 0);
    QCOMPARE(parentServerSurface->above().count(), 1);
    auto child = parentServerSurface->above().constFirst();
    QCOMPARE(child->parentSurface(), parentServerSurface);
    QCOMPARE(child->mainSurface(), parentServerSurface);
    QCOMPARE(child->surface()->below().count(), 0);
    QCOMPARE(child->surface()->above().count(), 1);
    auto child2 = child->surface()->above().constFirst();
    QCOMPARE(child2->parentSurface(), child->surface());
    QCOMPARE(child2->mainSurface(), parentServerSurface);
    QCOMPARE(child2->surface()->below().count(), 0);
    QCOMPARE(child2->surface()->above().count(), 1);
    auto child3 = child2->surface()->above().constFirst();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->below().count(), 0);
    QCOMPARE(child3->surface()->above().count(), 0);
}

void TestSubSurface::testRemoveSurface()
{
    // this test verifies that removing the surface also removes the sub-surface from the parent
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childServerSurface);

    QSignalSpy childrenChangedSpy(parentServerSurface, &SurfaceInterface::childSubSurfacesChanged);

    m_subCompositor->createSubSurface(childSurface.get(), parentSurface.get());
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(childrenChangedSpy.wait());

    QCOMPARE(parentServerSurface->below().count(), 0);
    QCOMPARE(parentServerSurface->above().count(), 1);

    // destroy surface, takes place immediately
    childSurface.reset();
    QVERIFY(childrenChangedSpy.wait());
    QCOMPARE(parentServerSurface->below().count(), 0);
    QCOMPARE(parentServerSurface->above().count(), 0);
}

void TestSubSurface::testMappingOfSurfaceTree()
{
    // this test verifies mapping and unmapping of a sub-surface tree
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);

    std::unique_ptr<KWayland::Client::Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(parentServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel1ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel1ServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel2ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel2ServerSurface);
    std::unique_ptr<KWayland::Client::Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel3ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface *>();
    QVERIFY(childLevel3ServerSurface);

    auto subSurfaceLevel1 = m_subCompositor->createSubSurface(childLevel1Surface.get(), parentSurface.get());
    auto subSurfaceLevel2 = m_subCompositor->createSubSurface(childLevel2Surface.get(), childLevel1Surface.get());
    auto subSurfaceLevel3 = m_subCompositor->createSubSurface(childLevel3Surface.get(), childLevel2Surface.get());

    QSignalSpy parentCommittedSpy(parentServerSurface, &SurfaceInterface::committed);
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(parentCommittedSpy.wait());

    QCOMPARE(parentServerSurface->below().count(), 0);
    QCOMPARE(parentServerSurface->above().count(), 1);
    auto child = parentServerSurface->above().constFirst();
    QCOMPARE(child->surface()->below().count(), 0);
    QCOMPARE(child->surface()->above().count(), 1);
    auto child2 = child->surface()->above().constFirst();
    QCOMPARE(child2->surface()->below().count(), 0);
    QCOMPARE(child2->surface()->above().count(), 1);
    auto child3 = child2->surface()->above().constFirst();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->below().count(), 0);
    QCOMPARE(child3->surface()->above().count(), 0);

    // so far no surface is mapped
    QVERIFY(!parentServerSurface->isMapped());
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // first set all subsurfaces to desync, to simplify
    subSurfaceLevel1->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    subSurfaceLevel2->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    subSurfaceLevel3->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);

    // first map the child, should not map it
    QSignalSpy child3DamageSpy(child3->surface(), &SurfaceInterface::damaged);
    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    childLevel3Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel3Surface->damage(QRect(0, 0, 200, 200));
    childLevel3Surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(child3DamageSpy.wait());
    QVERIFY(child3->surface()->buffer());
    QVERIFY(!child3->surface()->isMapped());

    // let's map the top level
    QSignalSpy parentSpy(parentServerSurface, &SurfaceInterface::damaged);
    parentSurface->attachBuffer(m_shm->createBuffer(image));
    parentSurface->damage(QRect(0, 0, 200, 200));
    parentSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(parentSpy.wait());
    QVERIFY(parentServerSurface->isMapped());
    // children should not yet be mapped
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // next level
    QSignalSpy child2DamageSpy(child2->surface(), &SurfaceInterface::damaged);
    childLevel2Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel2Surface->damage(QRect(0, 0, 200, 200));
    childLevel2Surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(child2DamageSpy.wait());
    QVERIFY(parentServerSurface->isMapped());
    // children should not yet be mapped
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // last but not least the first child level, which should map all our subsurfaces
    QSignalSpy child1DamageSpy(child->surface(), &SurfaceInterface::damaged);
    childLevel1Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel1Surface->damage(QRect(0, 0, 200, 200));
    childLevel1Surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(child1DamageSpy.wait());

    // everything is mapped
    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(child->surface()->isMapped());
    QVERIFY(child2->surface()->isMapped());
    QVERIFY(child3->surface()->isMapped());

    // unmapping a parent should unmap the complete tree
    QSignalSpy unmappedSpy(child->surface(), &SurfaceInterface::unmapped);
    childLevel1Surface->attachBuffer(KWayland::Client::Buffer::Ptr());
    childLevel1Surface->damage(QRect(0, 0, 200, 200));
    childLevel1Surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(unmappedSpy.wait());

    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());
}

void TestSubSurface::testSurfaceAt()
{
    // this test verifies that the correct surface is picked in a sub-surface tree
    using namespace KWaylandServer;
    // first create a parent surface and map it
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    parent->attachBuffer(m_shm->createBuffer(image));
    parent->damage(QRect(0, 0, 100, 100));
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *parentServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();

    // directChild1 occupies the top-left quarter of the parent surface
    QImage directImage(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
    std::unique_ptr<KWayland::Client::Surface> directChild1(m_compositor->createSurface());
    directChild1->attachBuffer(m_shm->createBuffer(directImage));
    directChild1->damage(QRect(0, 0, 50, 50));
    directChild1->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *directChild1ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(directChild1ServerSurface);

    // directChild2 occupies the bottom-right quarter of the parent surface
    std::unique_ptr<KWayland::Client::Surface> directChild2(m_compositor->createSurface());
    directChild2->attachBuffer(m_shm->createBuffer(directImage));
    directChild2->damage(QRect(0, 0, 50, 50));
    directChild2->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *directChild2ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    QVERIFY(directChild2ServerSurface);

    // create the sub surfaces for them
    std::unique_ptr<KWayland::Client::SubSurface> directChild1SubSurface(m_subCompositor->createSubSurface(directChild1.get(), parent.get()));
    directChild1SubSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    std::unique_ptr<KWayland::Client::SubSurface> directChild2SubSurface(m_subCompositor->createSubSurface(directChild2.get(), parent.get()));
    directChild2SubSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    directChild2SubSurface->setPosition(QPoint(50, 50));

    // unset input regions for direct children
    QSignalSpy directChild1CommittedSpy(directChild1ServerSurface, &SurfaceInterface::committed);
    directChild1->setInputRegion(m_compositor->createRegion(QRegion()).get());
    directChild1->commit(KWayland::Client::Surface::CommitFlag::None);
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(directChild1CommittedSpy.wait());

    QSignalSpy directChild2CommittedSpy(directChild2ServerSurface, &SurfaceInterface::committed);
    directChild2->setInputRegion(m_compositor->createRegion(QRegion()).get());
    directChild2->commit(KWayland::Client::Surface::CommitFlag::None);
    parent->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(directChild2CommittedSpy.wait());

    // each of the children gets a child
    std::unique_ptr<KWayland::Client::Surface> childFor1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *childFor1ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    std::unique_ptr<KWayland::Client::Surface> childFor2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *childFor2ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();

    // create sub surfaces for them
    std::unique_ptr<KWayland::Client::SubSurface> childFor1SubSurface(m_subCompositor->createSubSurface(childFor1.get(), directChild1.get()));
    childFor1SubSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    std::unique_ptr<KWayland::Client::SubSurface> childFor2SubSurface(m_subCompositor->createSubSurface(childFor2.get(), directChild2.get()));
    childFor2SubSurface->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);

    // now let's render both grand children
    QImage partImage(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
    partImage.fill(Qt::green);
    childFor1->attachBuffer(m_shm->createBuffer(partImage));
    childFor1->damage(QRect(0, 0, 50, 50));
    childFor1->commit(KWayland::Client::Surface::CommitFlag::None);
    partImage.fill(Qt::blue);

    QSignalSpy childFor2CommittedSpy(childFor2ServerSurface, &SurfaceInterface::committed);
    childFor2->attachBuffer(m_shm->createBuffer(partImage));
    // child for 2's input region is subdivided into quadrants, with input mask on the top left and bottom right
    QRegion region;
    region += QRect(0, 0, 25, 25);
    region += QRect(25, 25, 25, 25);
    childFor2->setInputRegion(m_compositor->createRegion(region).get());
    childFor2->damage(QRect(0, 0, 50, 50));
    childFor2->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(childFor2CommittedSpy.wait());

    QCOMPARE(directChild1ServerSurface->subSurface()->parentSurface(), parentServerSurface);
    QCOMPARE(directChild2ServerSurface->subSurface()->parentSurface(), parentServerSurface);
    QCOMPARE(childFor1ServerSurface->subSurface()->parentSurface(), directChild1ServerSurface);
    QCOMPARE(childFor2ServerSurface->subSurface()->parentSurface(), directChild2ServerSurface);

    // now let's test a few positions
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(0, 0)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(49, 49)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(50, 50)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(99, 99)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(99, 50)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(50, 99)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(25, 75)), parentServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(75, 25)), parentServerSurface);

    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(0, 0)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(49, 49)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(50, 50)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(99, 99)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(99, 50)), parentServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(50, 99)), parentServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(25, 75)), parentServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(75, 25)), parentServerSurface);

    // outside the geometries should be no surface
    QVERIFY(!parentServerSurface->surfaceAt(QPointF(-1, -1)));
    QVERIFY(!parentServerSurface->surfaceAt(QPointF(101, 101)));

    // on the surface edge right/bottom edges should not trigger as contained
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(50, 25)), parentServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(25, 50)), parentServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(50, 25)), parentServerSurface);
    QCOMPARE(parentServerSurface->inputSurfaceAt(QPointF(25, 50)), parentServerSurface);
}

void TestSubSurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a sub-surface works
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    std::unique_ptr<KWayland::Client::Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    // create sub-surface
    m_subCompositor->createSubSurface(child.get(), parent.get());

    // let's damage this surface, will be in sub-surface pending state
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    child->attachBuffer(m_shm->createBuffer(image));
    child->damage(QRect(0, 0, 100, 100));
    child->commit(KWayland::Client::Surface::CommitFlag::None);
    m_connection->flush();

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    delete m_shm;
    m_shm = nullptr;
    child.reset();
    QVERIFY(destroySpy.wait());
}

void TestSubSurface::testDestroyParentSurface()
{
    // this test verifies that destroying a parent surface does not create problems
    // see BUG 389231
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverParentSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    std::unique_ptr<KWayland::Client::Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    std::unique_ptr<KWayland::Client::Surface> grandChild(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverGrandChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface *>();
    // create sub-surface in desynchronized mode as Qt uses them
    auto sub1 = m_subCompositor->createSubSurface(child.get(), parent.get());
    sub1->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);
    auto sub2 = m_subCompositor->createSubSurface(grandChild.get(), child.get());
    sub2->setMode(KWayland::Client::SubSurface::Mode::Desynchronized);

    // let's damage this surface
    // and at the same time delete the parent surface
    parent.reset();
    QSignalSpy parentDestroyedSpy(serverParentSurface, &QObject::destroyed);
    QVERIFY(parentDestroyedSpy.wait());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    grandChild->attachBuffer(m_shm->createBuffer(image));
    grandChild->damage(QRect(0, 0, 100, 100));
    grandChild->commit(KWayland::Client::Surface::CommitFlag::None);
    QSignalSpy damagedSpy(serverGrandChildSurface, &SurfaceInterface::damaged);
    QVERIFY(damagedSpy.wait());

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    child.reset();
    QVERIFY(destroySpy.wait());
}

QTEST_GUILESS_MAIN(TestSubSurface)
#include "test_wayland_subsurface.moc"
