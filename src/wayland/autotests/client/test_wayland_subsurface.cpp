/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../src/client/compositor.h"
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/registry.h"
#include "../../src/client/subcompositor.h"
#include "../../src/client/subsurface.h"
#include "../../src/client/surface.h"
#include "../../src/server/display.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/subcompositor_interface.h"
#include "../../src/server/surface_interface.h"
// Wayland
#include <wayland-client.h>

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
    void testPosition();
    void testPlaceAbove();
    void testPlaceBelow();
    void testDestroy();
    void testCast();

private:
    KWayland::Server::Display *m_display;
    KWayland::Server::CompositorInterface *m_compositorInterface;
    KWayland::Server::SubCompositorInterface *m_subcompositorInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::Compositor *m_compositor;
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
    , m_subCompositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestSubSurface::init()
{
    using namespace KWayland::Server;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

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

    m_compositorInterface = m_display->createCompositor(m_display);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    m_subcompositorInterface = m_display->createSubCompositor(m_display);
    QVERIFY(m_subcompositorInterface);
    m_subcompositorInterface->create();
    QVERIFY(m_subcompositorInterface->isValid());

    QVERIFY(subCompositorSpy.wait());
    m_subCompositor = registry.createSubCompositor(subCompositorSpy.first().first().value<quint32>(), subCompositorSpy.first().last().value<quint32>(), this);

    if (compositorSpy.isEmpty()) {
        QVERIFY(compositorSpy.wait());
    }
    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
}

void TestSubSurface::cleanup()
{
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
    using namespace KWayland::Server;
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, SIGNAL(surfaceCreated(KWayland::Server::SurfaceInterface*)));
    QVERIFY(surfaceCreatedSpy.isValid());

    // create two Surfaces
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverSurface);

    surfaceCreatedSpy.clear();
    QScopedPointer<Surface> parent(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());
    SurfaceInterface *serverParentSurface = surfaceCreatedSpy.first().first().value<KWayland::Server::SurfaceInterface*>();
    QVERIFY(serverParentSurface);

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWayland::Server::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create subSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));

    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);
    QVERIFY(serverSubSurface->parentSurface());
    QCOMPARE(serverSubSurface->parentSurface().data(), serverParentSurface);
    QCOMPARE(serverSubSurface->surface().data(), serverSurface);
    QCOMPARE(serverSurface->subSurface().data(), serverSubSurface);
    // children are only added after committing the surface
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 0);
    // so let's commit the surface, to apply the stacking change
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 1);
    QCOMPARE(serverParentSurface->childSubSurfaces().first().data(), serverSubSurface);

    // and let's destroy it again
    QSignalSpy destroyedSpy(serverSubSurface, SIGNAL(destroyed(QObject*)));
    QVERIFY(destroyedSpy.isValid());
    subSurface.reset();
    QVERIFY(destroyedSpy.wait());
    QCOMPARE(serverSurface->subSurface(), QPointer<SubSurfaceInterface>());
    // only applied after next commit
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 1);
    // but the surface should be invalid
    QVERIFY(serverParentSurface->childSubSurfaces().first().isNull());
    // committing the state should solve it
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverParentSurface->childSubSurfaces().count(), 0);
}

void TestSubSurface::testMode()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // create two Surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWayland::Server::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);

    // both client and server subsurface should be in synchronized mode
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Synchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Synchronized);

    // verify that we can change to desynchronized
    QSignalSpy modeChangedSpy(serverSubSurface, SIGNAL(modeChanged(KWayland::Server::SubSurfaceInterface::Mode)));
    QVERIFY(modeChangedSpy.isValid());

    subSurface->setMode(SubSurface::Mode::Desynchronized);
    QCOMPARE(subSurface->mode(), SubSurface::Mode::Desynchronized);

    QVERIFY(modeChangedSpy.wait());
    QCOMPARE(modeChangedSpy.first().first().value<KWayland::Server::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Desynchronized);
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
    QCOMPARE(modeChangedSpy.first().first().value<KWayland::Server::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Desynchronized);
    QCOMPARE(modeChangedSpy.last().first().value<KWayland::Server::SubSurfaceInterface::Mode>(), SubSurfaceInterface::Mode::Synchronized);
    QCOMPARE(serverSubSurface->mode(), SubSurfaceInterface::Mode::Synchronized);
}

void TestSubSurface::testPosition()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // create two Surface
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWayland::Server::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurface for surface of parent
    QScopedPointer<SubSurface> subSurface(m_subCompositor->createSubSurface(QPointer<Surface>(surface.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface);

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
    QVERIFY(positionChangedSpy.wait());
    QCOMPARE(positionChangedSpy.count(), 1);
    QCOMPARE(positionChangedSpy.first().first().toPoint(), QPoint(20, 30));
    QCOMPARE(serverSubSurface->position(), QPoint(20, 30));
}

void TestSubSurface::testPlaceAbove()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // create needed Surfaces (one parent, three client
    QScopedPointer<Surface> surface1(m_compositor->createSurface());
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QScopedPointer<Surface> surface3(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWayland::Server::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurfaces for surface of parent
    QScopedPointer<SubSurface> subSurface1(m_subCompositor->createSubSurface(QPointer<Surface>(surface1.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface2(m_subCompositor->createSubSurface(QPointer<Surface>(surface2.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface3(m_subCompositor->createSubSurface(QPointer<Surface>(surface3.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QVERIFY(serverSubSurface1->parentSurface()->childSubSurfaces().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface3);

    // raising subsurface1 should place it to top of stack
    subSurface1->raise();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    // after commit it's changed
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface1);

    // try placing 3 above 1, should result in 2, 1, 3
    subSurface3->placeAbove(QPointer<SubSurface>(subSurface1.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface3);

    // try placing 3 above 2, should result in 2, 3, 1
    subSurface3->placeAbove(QPointer<SubSurface>(subSurface2.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface1);

    // try placing 1 above 3 - shouldn't change
    subSurface1->placeAbove(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface1);

    // and 2 above 3 - > 3, 2, 1
    subSurface2->placeAbove(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface1);
}

void TestSubSurface::testPlaceBelow()
{
    using namespace KWayland::Client;
    using namespace KWayland::Server;
    // create needed Surfaces (one parent, three client
    QScopedPointer<Surface> surface1(m_compositor->createSurface());
    QScopedPointer<Surface> surface2(m_compositor->createSurface());
    QScopedPointer<Surface> surface3(m_compositor->createSurface());
    QScopedPointer<Surface> parent(m_compositor->createSurface());

    QSignalSpy subSurfaceCreatedSpy(m_subcompositorInterface, SIGNAL(subSurfaceCreated(KWayland::Server::SubSurfaceInterface*)));
    QVERIFY(subSurfaceCreatedSpy.isValid());

    // create the SubSurfaces for surface of parent
    QScopedPointer<SubSurface> subSurface1(m_subCompositor->createSubSurface(QPointer<Surface>(surface1.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface1 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface1);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface2(m_subCompositor->createSubSurface(QPointer<Surface>(surface2.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface2 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface2);
    subSurfaceCreatedSpy.clear();
    QScopedPointer<SubSurface> subSurface3(m_subCompositor->createSubSurface(QPointer<Surface>(surface3.data()), QPointer<Surface>(parent.data())));
    QVERIFY(subSurfaceCreatedSpy.wait());
    SubSurfaceInterface *serverSubSurface3 = subSurfaceCreatedSpy.first().first().value<KWayland::Server::SubSurfaceInterface*>();
    QVERIFY(serverSubSurface3);
    subSurfaceCreatedSpy.clear();

    // so far the stacking order should still be empty
    QVERIFY(serverSubSurface1->parentSurface()->childSubSurfaces().isEmpty());

    // committing the parent should create the stacking order
    parent->commit(Surface::CommitFlag::None);
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface3);

    // lowering subsurface3 should place it to the bottom of stack
    subSurface3->lower();
    // ensure it's processed on server side
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    // but as long as parent is not committed it shouldn't change on server side
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    // after commit it's changed
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface2);

    // place 1 below 3 -> 1, 3, 2
    subSurface1->placeBelow(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface2);

    // 2 below 3 -> 1, 2, 3
    subSurface2->placeBelow(QPointer<SubSurface>(subSurface3.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface3);

    // 1 below 2 -> shouldn't change
    subSurface1->placeBelow(QPointer<SubSurface>(subSurface2.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface2);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface3);

    // and 3 below 1 -> 3, 1, 2
    subSurface3->placeBelow(QPointer<SubSurface>(subSurface1.data()));
    parent->commit(Surface::CommitFlag::None);
    wl_display_flush(m_connection->display());
    QCoreApplication::processEvents();
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().count(), 3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(0).data(), serverSubSurface3);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(1).data(), serverSubSurface1);
    QCOMPARE(serverSubSurface1->parentSurface()->childSubSurfaces().at(2).data(), serverSubSurface2);
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

QTEST_GUILESS_MAIN(TestSubSurface)
#include "test_wayland_subsurface.moc"
