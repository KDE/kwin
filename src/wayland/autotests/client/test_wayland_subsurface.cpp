/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/subcompositor.h"
#include "KWayland/Client/subsurface.h"
#include "KWayland/Client/surface.h"
#include "../../src/server/buffer_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/subcompositor_interface.h"
#include "../../src/server/surface_interface.h"
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
    void testDestroy();
    void testCast();
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
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
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
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    QVERIFY(compositorSpy.isValid());
    QSignalSpy subCompositorSpy(&registry, SIGNAL(subCompositorAnnounced(quint32,quint32)));
    QVERIFY(subCompositorSpy.isValid());
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

    m_shm = registry.createShmPool(registry.interface(KWayland::Client::Registry::Interface::Shm).name, registry.interface(KWayland::Client::Registry::Interface::Shm).version, this);
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
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWaylandServer::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    // create two Surfaces
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    surfaceCreatedSpy.clear();
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverParentSurface = surfaceCreatedSpy.first().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(serverParentSurface);

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWaylandServer::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));

    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);
    QVERIFY(serverSubSurface->parentSurface());
    QCOMPARE(serverSubSurface->parentSurface(), serverParentSurface);
    QCOMPARE(serverSubSurface->surface(), serverSurface);
    QCOMPARE(serverSurface->subSurface(), serverSubSurface);
    QCOMPARE(serverSubSurface->mainSurface(), serverParentSurface);
    // children are only added after committing the surface
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 0);
    // so let's commit the surface, to apply the stacking change
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 1);
    QCOMPARE(serverParentSurface->childSubSurfaces().first(), serverSubSurface);

    // and let's destroy it again
    QSignalSpy destroyedSpy(serverSubSurface, SIGNAL(destroyed(QObject*)));
    QVERIFY(destroyedSpy.isValid());
    subSurface.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(serverSurface->subSurface(), QPointer<SubSurfaceInterface>());
    // only applied after next commit
    QEXPECT_FAIL("", "Incorrect removing of child windows to workaround QtWayland behavior", Continue);
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 1);
    // but the surface should be invalid
    if (!serverParentSurface->childSubSurfaces().isEmpty()) {
        QVERIFY(!serverParentSurface->childSubSurfaces().first());
    }
    // committing the state should solve it
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 0);
}

void TestSubSurface::testMode()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create two Surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWaylandServer::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);

    // both client and server subsurface should be in synchronized mode
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Synchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Synchronized);

    // verify that we can change to desynchronized
    QSignalSpy modeChangedSpy(serverSubSurface, SIGNAL(modeChanged(KWaylandServer::SubSurfaceInterface::Mode)));
    QVERIFY(modeChangedSpy.isValid());

    subSurface->setMode(SubSurface::Mode::Desynchronized);
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Desynchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.first().first().value<KWaylandServer::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Desynchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Desynchronized);

    // setting the same again won't change
    subSurface->setMode(SubSurface::Mode::Desynchronized);
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Desynchronized);
    // not testing the signal, we do that after changing to synchronized

    // and change back to synchronized
    subSurface->setMode(SubSurface::Mode::Synchronized);
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Synchronized);

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
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create two Surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWaylandServer::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);

    // create a signalspy
    QSignalSpy subsurfaceTreeChanged(serverSubSurface->parentSurface(), &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subsurfaceTreeChanged.isValid());

    // put the subsurface in the desired commit mode
    QFETCH(KWayland::Client::SubSurface::Mode, commitMode);
    subSurface->setMode(commitMode);

    // both client and server should have a default position
    QCOMPARE(subSurface->position(), QPoint());
    QCOMPARE(serverSubSurface->position(), QPoint());

    QSignalSpy positionChangedSpy(serverSubSurface, SIGNAL(positionChanged(QPoint)));
    QVERIFY(positionChangedSpy.isValid());

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
    parent->commit(Surface::CommitFlag::None);
    QCOMPARE(subsurfaceTreeChanged.count(), 0);
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QCOMPARE(positionChangedSpy.first().first().toPoint(), QPoint(20, 30));
    QCOMPARE(serverSubSurface->position(), QPoint(20, 30));
    QCOMPARE(subsurfaceTreeChanged.count(), 1);
}

void TestSubSurface::testPlaceAbove()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create needed Surfaces (one parent, three client
    QScopedPointer<Surface> surface1(m_compositor->createSurface());
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QScopedPointer<Surface> surface3(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWaylandServer::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurfaces for surface of parent
    QScopedPointer<SubSurface> subSurface1(m_subCompositor->createSubSurface(QPointer<Surface>(surface1.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface2(m_subCompositor->createSubSurface(QPointer<Surface>(surface2.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface3(m_subCompositor->createSubSurface(QPointer<Surface>(surface3.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QVERIFY(serverSubSurface1->parentSurface()->childSubSurfaces().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface3);

    // raising subsurface1 should place it to top of stack
    subSurface1->raise();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    // after commit it's changed
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface1);

    // try placing 3 above 1, should result in 2, 1, 3
    subSurface3->placeAbove(QPointer<SubSurface>(subSurface1.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface3);

    // try placing 3 above 2, should result in 2, 3, 1
    subSurface3->placeAbove(QPointer<SubSurface>(subSurface2.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface1);

    // try placing 1 above 3 - shouldn't change
    subSurface1->placeAbove(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface1);

    // and 2 above 3 - > 3, 2, 1
    subSurface2->placeAbove(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface1);
}

void TestSubSurface::testPlaceBelow()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create needed Surfaces (one parent, three client
    QScopedPointer<Surface> surface1(m_compositor->createSurface());
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QScopedPointer<Surface> surface3(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWaylandServer::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurfaces for surface of parent
    QScopedPointer<SubSurface> subSurface1(m_subCompositor->createSubSurface(QPointer<Surface>(surface1.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface2(m_subCompositor->createSubSurface(QPointer<Surface>(surface2.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface3(m_subCompositor->createSubSurface(QPointer<Surface>(surface3.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWaylandServer::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QEXPECT_FAIL("", "Incorrect adding of child windows to workaround QtWayland behavior", Continue);
    QVERIFY(serverSubSurface1->parentSurface()->childSubSurfaces().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface3);

    // lowering subsurface3 should place it to the bottom of stack
    subSurface3->lower();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    // after commit it's changed
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface2);

    // place 1 below 3 -> 1, 3, 2
    subSurface1->placeBelow(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface2);

    // 2 below 3 -> 1, 2, 3
    subSurface2->placeBelow(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface3);

    // 1 below 2 -> shouldn't change
    subSurface1->placeBelow(QPointer<SubSurface>(subSurface2.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface3);

    // and 3 below 1 -> 3, 1, 2
    subSurface3->placeBelow(QPointer<SubSurface>(subSurface1.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2), serverSubSurface2);
}

void TestSubSurface::testDestroy()
{
    using namespace KWayland::Client;

    // create two Surfaces
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));

    connect(m_connection, &ConnectionThread::connectionDied, m_compositor, &Compositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_subCompositor, &SubCompositor::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_shm, &ShmPool::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, m_queue, &EventQueue::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, surface.data(), &Surface::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, parent.data(), &Surface::destroy);
    connect(m_connection, &ConnectionThread::connectionDied, subSurface.data(), &SubSurface::destroy);
    QVERIFY(subSurface->isValid());

    QSignalSpy connectionDiedSpy(m_connection, SIGNAL(connectionDied()));
    QVERIFY(connectionDiedSpy.isValid());
    delete m_display;
    m_display = nullptr;
    QVERIFY(connectionDiedSpy.wait());

    // now the pool should be destroyed;
    QVERIFY(!subSurface->isValid());

    // calling destroy again should not fail
    subSurface->destroy();
}

void TestSubSurface::testCast()
{
    using namespace KWayland::Client;

    // create two Surfaces
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));

    QCOMPARE(SubSurface::get(*(subSurface.data())), QPointer<SubSurface>(subSurface.data()));
}

void TestSubSurface::testSyncMode()
{
    // this test verifies that state is only applied when the parent surface commits its pending state
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(childSurface);

    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(parentSurface);
    QSignalSpy subSurfaceTreeChangedSpy(parentSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subSurfaceTreeChangedSpy.isValid());
    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceTreeChangedSpy.wait());
    QCOMPARE(subSurfaceTreeChangedSpy.count(), 1);

    // let's damage the child surface
    QSignalSpy childDamagedSpy(childSurface, &SurfaceInterface::damaged);
    QVERIFY(childDamagedSpy.isValid());

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
    QCOMPARE(subSurfaceTreeChangedSpy.count(), 2);
    QCOMPARE(childSurface->buffer()->data(), image);
    QCOMPARE(parentSurface->buffer()->data(), image2);
    QVERIFY(childSurface->isMapped());
    QVERIFY(parentSurface->isMapped());

    // sending frame rendered to parent should also send it to child
    QSignalSpy frameRenderedSpy(surface.data(), &Surface::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    parentSurface->frameRendered(100);
    QVERIFY(frameRenderedSpy.wait());
}

void TestSubSurface::testDeSyncMode()
{
    // this test verifies that state gets applied immediately in desync mode
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
    QVERIFY(childSurface);

    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(parentSurface);
    QSignalSpy subSurfaceTreeChangedSpy(parentSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subSurfaceTreeChangedSpy.isValid());
    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceTreeChangedSpy.wait());
    QCOMPARE(subSurfaceTreeChangedSpy.count(), 1);

    // let's damage the child surface
    QSignalSpy childDamagedSpy(childSurface, &SurfaceInterface::damaged);
    QVERIFY(childDamagedSpy.isValid());

    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(Surface::CommitFlag::None);

    // state should be applied when the parent surface's state gets applied or when the subsurface switches to desync
    QVERIFY(!childDamagedSpy.wait(100));
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // setting to desync should apply the state directly
    subSurface->setMode(SubSurface::Mode::Desynchronized);
    QVERIFY(childDamagedSpy.wait());
    QCOMPARE(subSurfaceTreeChangedSpy.count(), 2);
    QCOMPARE(childSurface->buffer()->data(), image);
    QVERIFY(!childSurface->isMapped());
    QVERIFY(!parentSurface->isMapped());

    // and damaging again, should directly be applied
    image.fill(Qt::red);
    surface->attachBuffer(m_shm->createBuffer(image));
    surface->damage(QRect(0, 0, 200, 200));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(childDamagedSpy.wait());
    QCOMPARE(subSurfaceTreeChangedSpy.count(), 3);
    QCOMPARE(childSurface->buffer()->data(), image);
}


void TestSubSurface::testMainSurfaceFromTree()
{
    // this test verifies that in a tree of surfaces every surface has the same main surface
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(parentServerSurface);
    QScopedPointer<Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel1ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel1ServerSurface);
    QScopedPointer<Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel2ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel2ServerSurface);
    QScopedPointer<Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel3ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel3ServerSurface);

    QSignalSpy subSurfaceTreeChangedSpy(parentServerSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subSurfaceTreeChangedSpy.isValid());

    m_subCompositor->createSubSurface(childLevel1Surface.data(), parentSurface.data());
    m_subCompositor->createSubSurface(childLevel2Surface.data(), childLevel1Surface.data());
    m_subCompositor->createSubSurface(childLevel3Surface.data(), childLevel2Surface.data());

    parentSurface->commit(Surface::CommitFlag::None);
    QVERIFY(subSurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubSurfaces().count(), 1);
    auto child = parentServerSurface->childSubSurfaces().first();
    QCOMPARE(child->parentSurface(), parentServerSurface);
    QCOMPARE(child->mainSurface(), parentServerSurface);
    QCOMPARE(child->surface()->childSubSurfaces().count(), 1);
    auto child2 = child->surface()->childSubSurfaces().first();
    QCOMPARE(child2->parentSurface(), child->surface());
    QCOMPARE(child2->mainSurface(), parentServerSurface);
    QCOMPARE(child2->surface()->childSubSurfaces().count(), 1);
    auto child3 = child2->surface()->childSubSurfaces().first();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->childSubSurfaces().count(), 0);
}

void TestSubSurface::testRemoveSurface()
{
    // this test verifies that removing the surface also removes the sub-surface from the parent
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(parentServerSurface);
    QScopedPointer<Surface> childSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childServerSurface);

    QSignalSpy subSurfaceTreeChangedSpy(parentServerSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subSurfaceTreeChangedSpy.isValid());

    m_subCompositor->createSubSurface(childSurface.data(), parentSurface.data());
    parentSurface->commit(Surface::CommitFlag::None);
    QVERIFY(subSurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubSurfaces().count(), 1);

    // destroy surface, takes place immediately
    childSurface.reset();
    QVERIFY(subSurfaceTreeChangedSpy.wait());
    QCOMPARE(parentServerSurface->childSubSurfaces().count(), 0);
}

void TestSubSurface::testMappingOfSurfaceTree()
{
    // this test verifies mapping and unmapping of a sub-surface tree
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());

    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto parentServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(parentServerSurface);
    QScopedPointer<Surface> childLevel1Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel1ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel1ServerSurface);
    QScopedPointer<Surface> childLevel2Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel2ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel2ServerSurface);
    QScopedPointer<Surface> childLevel3Surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    auto childLevel3ServerSurface = surfaceCreatedSpy.last().first().value<SurfaceInterface*>();
    QVERIFY(childLevel3ServerSurface);

    QSignalSpy subSurfaceTreeChangedSpy(parentServerSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(subSurfaceTreeChangedSpy.isValid());

    auto subSurfaceLevel1 = m_subCompositor->createSubSurface(childLevel1Surface.data(), parentSurface.data());
    auto subSurfaceLevel2 = m_subCompositor->createSubSurface(childLevel2Surface.data(), childLevel1Surface.data());
    auto subSurfaceLevel3 = m_subCompositor->createSubSurface(childLevel3Surface.data(), childLevel2Surface.data());

    parentSurface->commit(Surface::CommitFlag::None);
    QVERIFY(subSurfaceTreeChangedSpy.wait());

    QCOMPARE(parentServerSurface->childSubSurfaces().count(), 1);
    auto child = parentServerSurface->childSubSurfaces().first();
    QCOMPARE(child->surface()->childSubSurfaces().count(), 1);
    auto child2 = child->surface()->childSubSurfaces().first();
    QCOMPARE(child2->surface()->childSubSurfaces().count(), 1);
    auto child3 = child2->surface()->childSubSurfaces().first();
    QCOMPARE(child3->parentSurface(), child2->surface());
    QCOMPARE(child3->mainSurface(), parentServerSurface);
    QCOMPARE(child3->surface()->childSubSurfaces().count(), 0);

    // so far no surface is mapped
    QVERIFY(!parentServerSurface->isMapped());
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());

    // first set all subsurfaces to desync, to simplify
    subSurfaceLevel1->setMode(SubSurface::Mode::Desynchronized);
    subSurfaceLevel2->setMode(SubSurface::Mode::Desynchronized);
    subSurfaceLevel3->setMode(SubSurface::Mode::Desynchronized);

    // first map the child, should not map it
    QSignalSpy child3DamageSpy(child3->surface(), &SurfaceInterface::damaged);
    QVERIFY(child3DamageSpy.isValid());
    QImage image(QSize(200, 200), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    childLevel3Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel3Surface->damage(QRect(0, 0, 200, 200));
    childLevel3Surface->commit(Surface::CommitFlag::None);
    QVERIFY(child3DamageSpy.wait());
    QVERIFY(child3->surface()->buffer());
    QVERIFY(!child3->surface()->isMapped());

    // let's map the top level
    QSignalSpy parentSpy(parentServerSurface, &SurfaceInterface::damaged);
    QVERIFY(parentSpy.isValid());
    parentSurface->attachBuffer(m_shm->createBuffer(image));
    parentSurface->damage(QRect(0, 0, 200, 200));
    parentSurface->commit(Surface::CommitFlag::None);
    QVERIFY(parentSpy.wait());
    QVERIFY(parentServerSurface->isMapped());
    // children should not yet be mapped
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child->surface()->isMapped());
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child2->surface()->isMapped());
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child3->surface()->isMapped());

    // next level
    QSignalSpy child2DamageSpy(child2->surface(), &SurfaceInterface::damaged);
    QVERIFY(child2DamageSpy.isValid());
    childLevel2Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel2Surface->damage(QRect(0, 0, 200, 200));
    childLevel2Surface->commit(Surface::CommitFlag::None);
    QVERIFY(child2DamageSpy.wait());
    QVERIFY(parentServerSurface->isMapped());
    // children should not yet be mapped
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child->surface()->isMapped());
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child2->surface()->isMapped());
    QEXPECT_FAIL("", "Workaround for QtWayland bug https://bugreports.qt.io/browse/QTBUG-52192", Continue);
    QVERIFY(!child3->surface()->isMapped());

    // last but not least the first child level, which should map all our subsurfaces
    QSignalSpy child1DamageSpy(child->surface(), &SurfaceInterface::damaged);
    QVERIFY(child1DamageSpy.isValid());
    childLevel1Surface->attachBuffer(m_shm->createBuffer(image));
    childLevel1Surface->damage(QRect(0, 0, 200, 200));
    childLevel1Surface->commit(Surface::CommitFlag::None);
    QVERIFY(child1DamageSpy.wait());

    // everything is mapped
    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(child->surface()->isMapped());
    QVERIFY(child2->surface()->isMapped());
    QVERIFY(child3->surface()->isMapped());

    // unmapping a parent should unmap the complete tree
    QSignalSpy unmappedSpy(child->surface(), &SurfaceInterface::unmapped);
    QVERIFY(unmappedSpy.isValid());
    childLevel1Surface->attachBuffer(Buffer::Ptr());
    childLevel1Surface->damage(QRect(0, 0, 200, 200));
    childLevel1Surface->commit(Surface::CommitFlag::None);
    QVERIFY(unmappedSpy.wait());

    QVERIFY(parentServerSurface->isMapped());
    QVERIFY(!child->surface()->isMapped());
    QVERIFY(!child2->surface()->isMapped());
    QVERIFY(!child3->surface()->isMapped());
}

void TestSubSurface::testSurfaceAt()
{
    // this test verifies that the correct surface is picked in a sub-surface tree
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // first create a parent surface and map it
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    parent->attachBuffer(m_shm->createBuffer(image));
    parent->damage(QRect(0, 0, 100, 100));
    parent->commit(Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *parentServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();

    // create two child sub surfaces, those won't be mapped, just added to the parent
    // this is to simulate the behavior of QtWayland
    QScopedPointer<Surface> directChild1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *directChild1ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(directChild1ServerSurface);
    QScopedPointer<Surface> directChild2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *directChild2ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QVERIFY(directChild2ServerSurface);

    // create the sub surfaces for them
    QScopedPointer<SubSurface> directChild1SubSurface(m_subCompositor->createSubSurface(directChild1.data(), parent.data()));
    directChild1SubSurface->setMode(SubSurface::Mode::Desynchronized);
    QScopedPointer<SubSurface> directChild2SubSurface(m_subCompositor->createSubSurface(directChild2.data(), parent.data()));
    directChild2SubSurface->setMode(SubSurface::Mode::Desynchronized);

    // each of the children gets a child
    QScopedPointer<Surface> childFor1(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *childFor1ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QScopedPointer<Surface> childFor2(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *childFor2ServerSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();

    // create sub surfaces for them
    QScopedPointer<SubSurface> childFor1SubSurface(m_subCompositor->createSubSurface(childFor1.data(), directChild1.data()));
    childFor1SubSurface->setMode(SubSurface::Mode::Desynchronized);
    QScopedPointer<SubSurface> childFor2SubSurface(m_subCompositor->createSubSurface(childFor2.data(), directChild2.data()));
    childFor2SubSurface->setMode(SubSurface::Mode::Desynchronized);

    // both get a quarter of the grand-parent surface
    childFor2SubSurface->setPosition(QPoint(50, 50));
    childFor2->commit(Surface::CommitFlag::None);
    directChild2->commit(Surface::CommitFlag::None);
    parent->commit(Surface::CommitFlag::None);

    // now let's render both grand children
    QImage partImage(QSize(50, 50), QImage::Format_ARGB32_Premultiplied);
    partImage.fill(Qt::green);
    childFor1->attachBuffer(m_shm->createBuffer(partImage));
    childFor1->damage(QRect(0, 0, 50, 50));
    childFor1->commit(Surface::CommitFlag::None);
    partImage.fill(Qt::blue);

    childFor2->attachBuffer(m_shm->createBuffer(partImage));
    // child for 2's input region is subdivided into quadrants, with input mask on the top left and bottom right
    QRegion region;
    region += QRect(0,0,25,25);
    region += QRect(25,25,25,25);
    childFor2->setInputRegion(m_compositor->createRegion(region).get());
    childFor2->damage(QRect(0, 0, 50, 50));
    childFor2->commit(Surface::CommitFlag::None);

    QSignalSpy treeChangedSpy(parentServerSurface, &SurfaceInterface::subSurfaceTreeChanged);
    QVERIFY(treeChangedSpy.isValid());
    QVERIFY(treeChangedSpy.wait());

    QCOMPARE(directChild1ServerSurface->subSurface()->parentSurface(), parentServerSurface);
    QCOMPARE(directChild2ServerSurface->subSurface()->parentSurface(), parentServerSurface);
    QCOMPARE(childFor1ServerSurface->subSurface()->parentSurface(), directChild1ServerSurface);
    QCOMPARE(childFor2ServerSurface->subSurface()->parentSurface(), directChild2ServerSurface);

    // now let's test a few positions
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(0, 0)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(49, 49)), childFor1ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(50, 50)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(100, 100)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(100, 50)), childFor2ServerSurface);
    QCOMPARE(parentServerSurface->surfaceAt(QPointF(50, 100)), childFor2ServerSurface);
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
}

void TestSubSurface::testDestroyAttachedBuffer()
{
    // this test verifies that destroying of a buffer attached to a sub-surface works
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    QScopedPointer<Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    // create sub-surface
    m_subCompositor->createSubSurface(child.data(), parent.data());

    // let's damage this surface, will be in sub-surface pending state
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    child->attachBuffer(m_shm->createBuffer(image));
    child->damage(QRect(0, 0, 100, 100));
    child->commit(Surface::CommitFlag::None);
    m_connection->flush();

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    QVERIFY(destroySpy.isValid());
    delete m_shm;
    m_shm = nullptr;
    child.reset();
    QVERIFY(destroySpy.wait());
}

void TestSubSurface::testDestroyParentSurface()
{
    // this test verifies that destroying a parent surface does not create problems
    // see BUG 389231
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    // create surface
    QSignalSpy serverSurfaceCreated(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreated.isValid());
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverParentSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QScopedPointer<Surface> child(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    QScopedPointer<Surface> grandChild(m_compositor->createSurface());
    QVERIFY(serverSurfaceCreated.wait());
    SurfaceInterface *serverGrandChildSurface = serverSurfaceCreated.last().first().value<KWaylandServer::SurfaceInterface*>();
    // create sub-surface in desynchronized mode as Qt uses them
    auto sub1 = m_subCompositor->createSubSurface(child.data(), parent.data());
    sub1->setMode(SubSurface::Mode::Desynchronized);
    auto sub2 = m_subCompositor->createSubSurface(grandChild.data(), child.data());
    sub2->setMode(SubSurface::Mode::Desynchronized);

    // let's damage this surface
    // and at the same time delete the parent surface
    parent.reset();
    QSignalSpy parentDestroyedSpy(serverParentSurface, &QObject::destroyed);
    QVERIFY(parentDestroyedSpy.isValid());
    QVERIFY(parentDestroyedSpy.wait());
    QImage image(QSize(100, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    grandChild->attachBuffer(m_shm->createBuffer(image));
    grandChild->damage(QRect(0, 0, 100, 100));
    grandChild->commit(Surface::CommitFlag::None);
    QSignalSpy damagedSpy(serverGrandChildSurface, &SurfaceInterface::damaged);
    QVERIFY(damagedSpy.isValid());
    QVERIFY(damagedSpy.wait());

    // Let's try to destroy it
    QSignalSpy destroySpy(serverChildSurface, &QObject::destroyed);
    QVERIFY(destroySpy.isValid());
    child.reset();
    QVERIFY(destroySpy.wait());
}

QTEST_GUILESS_MAIN(TestSubSurface)
#include "test_wayland_subsurface.moc"
