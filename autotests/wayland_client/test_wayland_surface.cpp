/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// KWin
#include "../../wayland_client/compositor.h"
#include "../../wayland_client/connection_thread.h"
#include "../../wayland_client/surface.h"
#include "../../wayland_client/registry.h"
#include "../../wayland_server/compositor_interface.h"
#include "../../wayland_server/display.h"
#include "../../wayland_server/surface_interface.h"
// Wayland
#include <wayland-client-protocol.h>

class TestWaylandSurface : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandSurface(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testStaticAccessor();
    void testDamage();
    void testFrameCallback();

private:
    KWin::WaylandServer::Display *m_display;
    KWin::WaylandServer::CompositorInterface *m_compositorInterface;
    KWin::Wayland::ConnectionThread *m_connection;
    KWin::Wayland::Compositor *m_compositor;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-surface-0");

TestWaylandSurface::TestWaylandSurface(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandSurface::init()
{
    using namespace KWin::WaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = m_display->createCompositor(m_display);
    QVERIFY(m_compositorInterface);
    m_compositorInterface->create();
    QVERIFY(m_compositorInterface->isValid());

    // setup connection
    m_connection = new KWin::Wayland::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, m_connection,
        [this]() {
            if (m_connection->display()) {
                wl_display_flush(m_connection->display());
            }
        }
    );

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    KWin::Wayland::Registry registry;
    QSignalSpy compositorSpy(&registry, SIGNAL(compositorAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(compositorSpy.wait());

    m_compositor = new KWin::Wayland::Compositor(this);
    m_compositor->setup(registry.bindCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_compositor->isValid());
}

void TestWaylandSurface::cleanup()
{
    if (m_compositor) {
        delete m_compositor;
        m_compositor = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    delete m_compositorInterface;
    m_compositorInterface = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandSurface::testStaticAccessor()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWin::WaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());

    QVERIFY(KWin::Wayland::Surface::all().isEmpty());
    KWin::Wayland::Surface *s1 = m_compositor->createSurface();
    QVERIFY(s1->isValid());
    QCOMPARE(KWin::Wayland::Surface::all().count(), 1);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);
    QVERIFY(serverSurfaceCreated.wait());

    QVERIFY(!s1->size().isValid());
    QSignalSpy sizeChangedSpy(s1, SIGNAL(sizeChanged(QSize)));
    QVERIFY(sizeChangedSpy.isValid());
    const QSize testSize(200, 300);
    s1->setSize(testSize);
    QCOMPARE(s1->size(), testSize);
    QCOMPARE(sizeChangedSpy.count(), 1);
    QCOMPARE(sizeChangedSpy.first().first().toSize(), testSize);

    // add another surface
    KWin::Wayland::Surface *s2 = m_compositor->createSurface();
    QVERIFY(s2->isValid());
    QCOMPARE(KWin::Wayland::Surface::all().count(), 2);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::all().last(), s2);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s2), s2);
    serverSurfaceCreated.clear();
    QVERIFY(serverSurfaceCreated.wait());

    // delete s2 again
    delete s2;
    QCOMPARE(KWin::Wayland::Surface::all().count(), 1);
    QCOMPARE(KWin::Wayland::Surface::all().first(), s1);
    QCOMPARE(KWin::Wayland::Surface::get(*s1), s1);

    // and finally delete the last one
    delete s1;
    QVERIFY(KWin::Wayland::Surface::all().isEmpty());
    QVERIFY(!KWin::Wayland::Surface::get(nullptr));
}

void TestWaylandSurface::testDamage()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWin::WaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWin::Wayland::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWin::WaylandServer::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::WaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);
    QCOMPARE(serverSurface->damage(), QRegion());

    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    // TODO: actually we would need to attach a buffer first
    s->damage(QRect(0, 0, 10, 10));
    s->commit(KWin::Wayland::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), QRegion(0, 0, 10, 10));
    QCOMPARE(damageSpy.first().first().value<QRegion>(), QRegion(0, 0, 10, 10));

    // damage multiple times
    QRegion testRegion(5, 8, 3, 6);
    testRegion = testRegion.united(QRect(10, 20, 30, 15));
    s->damage(testRegion);
    damageSpy.clear();
    s->commit(KWin::Wayland::Surface::CommitFlag::None);
    QVERIFY(damageSpy.wait());
    QCOMPARE(serverSurface->damage(), testRegion);
    QCOMPARE(damageSpy.first().first().value<QRegion>(), testRegion);
}

void TestWaylandSurface::testFrameCallback()
{
    QSignalSpy serverSurfaceCreated(m_compositorInterface, SIGNAL(surfaceCreated(KWin::WaylandServer::SurfaceInterface*)));
    QVERIFY(serverSurfaceCreated.isValid());
    KWin::Wayland::Surface *s = m_compositor->createSurface();
    QVERIFY(serverSurfaceCreated.wait());
    KWin::WaylandServer::SurfaceInterface *serverSurface = serverSurfaceCreated.first().first().value<KWin::WaylandServer::SurfaceInterface*>();
    QVERIFY(serverSurface);

    QSignalSpy damageSpy(serverSurface, SIGNAL(damaged(QRegion)));
    QVERIFY(damageSpy.isValid());

    QSignalSpy frameRenderedSpy(s, SIGNAL(frameRendered()));
    QVERIFY(frameRenderedSpy.isValid());
    s->damage(QRect(0, 0, 10, 10));
    s->commit();
    QVERIFY(damageSpy.wait());
    serverSurface->frameRendered(10);
    QVERIFY(frameRenderedSpy.isEmpty());
    QVERIFY(frameRenderedSpy.wait());
    QVERIFY(!frameRenderedSpy.isEmpty());
}

QTEST_MAIN(TestWaylandSurface)
#include "test_wayland_surface.moc"
